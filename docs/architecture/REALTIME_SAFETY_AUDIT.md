# REALTIME_SAFETY_AUDIT.md

End-of-P2 audit of the audio-thread code paths against `REALTIME_AUDIO_POLICY.md` (no
allocation, no locks, no IO, no unbounded work on the audio thread) — the deliverable ADR-0011's
Consequences schedule for this point. **Method: line-level code inspection of every call reachable
from `processBlock`, with the sites cited.** What inspection cannot prove is listed under *Gaps*,
not claimed (C7).

**Audited revision:** the P2 transition-layer commit on PR #5, 2026-08-01.

## The audio-thread call graph

`AnabasisAudioProcessor::processBlock` (`src/PluginProcessor.cpp`):
`ScopedNoDenormals` → `CachedParams::toEngine` (49 relaxed atomic loads into the POD) →
`InternalState` mirror loads (4 relaxed atomics) → `AnabasisEngine::process`.

`AnabasisEngine::process` → per-block setters → `latchOsConfig` (only on a config change, at the
duck's silent bottom) → `processChunk` × ⌈blockSize / preparedMax⌉.

## Allocation

Every allocation in the DSP tree happens in `prepare()` or a function only `prepare()` calls:

| Site | When |
|---|---|
| `wetRing/dryRing/staging.setSize`, `ceilArr/wArr/pushArr.resize` | `AnabasisEngine::prepare` (`src/dsp/AnabasisEngine.cpp`) |
| eight `std::make_unique<juce::dsp::Oversampling>` + `initProcessing` | the same `prepare`, its `for (f, ph)` loop |
| wedge `assign` ×2 channels, sized for 16× | `LookaheadLimiter::prepare` (`src/dsp/LookaheadLimiter.h`) |
| the true-peak path's rings (`audio`, `ceilings`, `requirement`, `need`, `forwardMin`) — sized from the rate once | `CeilingClamp::prepare` (`src/dsp/CeilingClamp.h`), called by `AnabasisEngine::prepare` (ADR-0041, added 2026-09-27) |
| the TP-engagement decay (`EngagementTail`) — fixed arrays only; `prepare` sets its length and designs its private detector | `EngagementTail::prepare` (`src/dsp/CeilingClamp.h`), called by `AnabasisEngine::prepare` (ADR-0041 decision 5, revised 2026-09-27) |

Citations here are **symbol-based on purpose**: the first version of this table carried line
ranges, and every one of them had drifted by the time it was next read (`pushArr` did not even
exist yet). `THREAD_MODEL.md` learned the same lesson and states it — a line number in a document
is an assertion nobody re-runs.

The audio-thread paths perform **zero** allocation: `latchOsConfig` is selection among existing
objects plus plain-float recomputation (`setRate` on the limiter and clipper recompute one-pole
constants; `SmoothedValue::reset` writes members); `juce::dsp::Oversampling::reset()` is
`noexcept` and clears preallocated storage (verified in the pinned source —
`juce_Oversampling.cpp`, `FloatVectorOperations`-class clears); `AudioBuffer::clear` on the
already-sized ring is a bounded memset (~500 KB worst case at 16×, ≈ tens of µs — a deliberate,
bounded cost paid only at a latch, never per block).

## Locks, IO, unbounded work

None found on any audited path. The only loops whose trip counts are not compile-time constants
are bounded by `prepare()`-fixed quantities (chunk length ≤ maxBlock, region ≤ maxBlock·16, wedge
expiry amortised O(1); the ceiling clamp's true-peak windows — 32 entries and the attack length,
0.25 ms of samples — scanned only while they hold a value below unity, ADR-0041). The 2026-09-27
additions were inspected to the same standard and are also under `testTheAudioPathAllocatesNothing`'s
guard, which now runs TP mode in its matrix and toggles it mid-stream: `CeilingClamp::reset` and
`processFrameTruePeak` (fills and fixed-size loops over prepared storage),
`ClampTruePeakDetector::processFrame` (fixed 32-sample window, no state beyond its arrays), and
`AdaptiveEngine::resumeAfterReset` (four float copies and five atomic stores, once per reset —
ADR-0042). The PR #42 review added `EngagementTail` (`CeilingClamp.h`, ADR-0041 decision 5
revised), which holds only fixed arrays and a private detector: `start` runs once per TP engagement and is
bounded at 13 replays of 63 detector frames (measured on the reference Xeon: ~7 µs when the decay
fits unscaled, ~80 µs when the bisection runs — means of 2000 calls; the slowest single call in
3 × 2000, as thread CPU time on the shared CI-class container, ~0.25 ms against a 1.33 ms 64-sample
block at 48 kHz),
`value`/`advance`/`pushEmitted` are a few multiplies and stores per sample. The TP-mode matrix of the
allocation guard crosses engagements, so `start` runs under the guard too. **Since the PR #42 review the
ceiling stage's true-peak half is also inside the compile-time tier** (`tests/realtime_effects.cpp`,
`-Wfunction-effects`, ADR-0029): `CeilingClamp.h` and the new `ClampTruePeakDetector.h` include no
JUCE module, the driver calls `processFrameTruePeak`, the detector and every `EngagementTail` entry
point, and a violation seeded inside the tail's junction check is reported there. No `juce::String`,
no logging, no file access, no message posting.
`MacroEngine::parameterChanged` — which APVTS may deliver ON the audio thread during automation —
stores one relaxed atomic and returns; `triggerAsyncUpdate` (which takes a platform lock) is
gated behind `MessageManager::existsAndIsCurrentThread()` (`src/MacroEngine.cpp:28-35`).

## Transcendentals and denormals

`pow/exp/log/cos/acos/nearbyint` are used per sample or per event — constant-time libm calls,
permitted; no `printf`-class variadic calls. `ScopedNoDenormals` at the top of `processBlock` is
the single FTZ/DAZ mechanism (THREAD_MODEL); envelope states additionally never linger in the
denormal range by construction (release targets are ≥ 0 with finite alphas, dither error is
grid-quantised).

## Exceptions

`AnabasisEngine::process` is `noexcept`; the JUCE dsp calls inside are not annotated but do not
throw on the audited paths (no allocation → no `bad_alloc`; `AudioBlock` arithmetic is
noexcept-in-fact). An exception here would terminate rather than corrupt — the standard JUCE
posture — and the only allocation-bearing calls sit in `prepare`, which the host does not invoke
on the audio thread.

## Gaps — inspected, not machine-verified

- ~~**No sanitizer/RT-checker run**~~ — **CLOSED at 0.2.0 (ADR-0029).** This entry asked for "a
  malloc-interposition run (e.g. an RT-safety checker under the DSP suite)" and tracked it "for
  P6's gate"; P6 closed without it and the request stood for four versions. `tests/AllocationGuard.h`
  is that instrument: replaceable `operator new`/`delete` plus glibc malloc interposition, armed
  only around `AnabasisEngine::process` and compiled into `AnabasisTests`.
  **The allocation claims in this document are therefore machine-verified rather than
  inspected**, over 2,040 armed `process()` calls across 80 configurations (both channel counts ×
  all five oversample factors × both phase modes × four parameter sets, plus a mid-stream
  oversample rewire driven without a re-prepare): **0 allocations, both counters proved live in
  the same run.** For scale, the same guard reports `new=205 malloc=1313` for a single
  `prepare (48000, 256, 2)` — which is what the audit means when it says allocation is confined
  there.
  Two things this does NOT close, stated so they are not read as closed: the guard sees only the
  code the suite executes (`scripts/check-realtime.py` reads the rest, and the gcov figure for
  that gap is in its header), and it counts ALLOCATION only — locks and blocking calls are
  RealtimeSanitizer's half, which is Clang/Linux+macOS only.
- **RealtimeSanitizer has not yet run in this repository.** The `realtime` job and the
  `ANABASIS_NONBLOCKING` annotation land with 0.2.0, but the pinned Clang that carries the RTSan
  runtime is a CI-side toolchain; the first green `realtime` run is **owed** and is recorded here
  rather than assumed. The allocation half is covered meanwhile by the guard above on every
  platform including the one RTSan can never reach.
- **Host callback threading** is a host contract (KI-003): the audit covers our side of each
  boundary only.
- The **GUI/message-thread half** (editor paint, MacroEngine timer) is out of scope here — it has
  no real-time obligation.
