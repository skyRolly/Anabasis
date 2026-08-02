# THREAD_MODEL.md

The **implemented** thread model, written from **ADR-0011** (Accepted 2026-07-31) with code
citations, as `THREADING_POLICY.md` requires of P1. This file *describes* the code; the binding
rules stay in the policy and the ADR. When this file and the code disagree, the code is drifting or
this file is stale — report it (`SOURCE_OF_TRUTH.md`), do not silently pick one.

**Status: P4 (P1–P4 landed).** The inventory below covers every cross-thread edge that exists in
the tree today, including the P2–P4 additions (the SPSC GR ring, the duck/Learn/restore command
atomics, the meter publish row). Still-planned rows (the P5 spectrum rings, the OQ-013 trim
transport) are listed under **Planned edges** so their absence is visible rather than implied.

## Thread inventory

| Thread | Exists at P1 | Owns |
|---|---|---|
| **Audio** | yes | `AnabasisEngine` state (rings, wedge, envelope, smoothers, crossfade), built-once-per-block `EngineParameters` snapshot |
| **Message/GUI** | yes | Editor, `MacroEngine`, `InternalState` tree, A/B slots + preset/undo bulk swaps, PDC recompute (with the two host-callback exceptions below) |
| **GPU render context** | yes, macOS/Windows only | Component painting when attached — created and driven by JUCE, holds no Anabasis state (`src/gui/PluginEditor.h:38`, attach gated per platform per DESIGN §6.1) |
| Workers | **none** | Adding one is an Architecture Review Gate item + Hard Stop (ADR-0011) |

## Cross-thread edges implemented at P1

Every edge, its mechanism, its direction, and which permitted-path table row (THREADING_POLICY.md)
it instantiates:

| Edge | Mechanism | Table row | Code |
|---|---|---|---|
| Parameters → engine | **45** cached `getRawParameterValue` atomics (`kCachedParamCount` — 44 at P1, +`freeze` at P4) read once per block into the POD snapshot (`CachedParams::toEngine`), never piecemeal | GUI → Audio (automatable params) | `src/PluginParameters.cpp` (`resolve`/`toEngine`), `src/PluginProcessor.cpp` (`processBlock` snapshot build) |
| Host-hidden config → engine | `InternalState` relaxed atomic mirrors (`osMirror`/`phaseMirror`/`offlineMirror`/`lockMirror`), synced on every tree write, read per block | GUI → Audio (host-hidden session state) | `src/InternalState.h:153`, `syncAtomics()` |
| `nonRealtime` → latency predictor | `std::atomic<bool> nonRealtimeFlag`, written in `setNonRealtime()` (a host-thread callback), read by the snapshot build and the predictor | atomic mirror (same class as the row above) | `src/PluginProcessor.h` (`nonRealtimeFlag`), `src/PluginProcessor.cpp` (`setNonRealtime`/`updateLatency`/`processBlock`) |
| Engaged lookahead window → any reader | `std::atomic<int> engagedWindow`, relaxed, written per sample on the audio thread; payload-free diagnostic (the smoothing test reads it) | Audio → GUI (staleness-hint class: monotonic display/diagnostic data) | `src/dsp/AnabasisEngine.h` (`engagedWindowSamples`; line-number cites here went stale twice — symbols only) |
| Forced-duck request → engine | `std::atomic<bool> duckRequested`, set by the wrapper before every bulk swap (A/B, preset, session load), `exchange`-consumed at the block top | GUI → Audio (momentary / transient requests) | `src/dsp/AnabasisEngine.h` (`requestForcedDuck`), `src/PluginProcessor.cpp` (three call sites) |
| Meters → GUI | `pubLufsM/S/I`, `pubDbTpMax`, `pubPlr`, `pubGrDb` — relaxed atomics, ONE publish per block from `processBlock`, **fed from the engine's §2.9 render tap** (the programme path before the monitor-only delta/comp stages — the buffer itself carries the listening path); plus the engine's `grMinLinear`/`engagedWindow` diagnostic atomics | Audio → GUI (meters) | `src/PluginProcessor.cpp` (the per-block publish), `src/dsp/AnabasisEngine.h` (`outputLoudness`/`lastRenderTpMax`) |
| GR/waveform history → GUI | `GrHistoryBuffer`: 4096-entry power-of-two SPSC ring, entry written FIRST, monotonic index release-stored AFTER, acquire-loaded stateless peeks on the reader side | Audio → GUI (time series, SPSC ring) | `src/dsp/GrHistoryBuffer.h` |
| Learn start/stop → engine | ONE staged record: a `learnCmdCode` payload (start / commit / commitThenStart) stored relaxed, the single `learnCmdPending` flag **release**-stored after, `exchange`-consumed with **acquire** at the block top. The writer COMPOSES on its own thread — a start landing on an unconsumed commit becomes one commitThenStart — because ordering information exists only there; two flags with a fixed consumption order silently discarded both commands when a stop and a start fell in the same block | GUI → Audio (**bounded staged record**, ADR-0012) | `src/dsp/AnabasisEngine.h` (`requestLearnStart/Stop`), `src/dsp/AnabasisEngine.cpp` (block top) |
| Learned-target restore → engine | ONE staged record: `pendingLearned` (the learned/never-learned discriminator) + `pendingRefOnset`/`pendingRefTilt` stored relaxed FIRST, then the single `adaptivePending` flag **release**-stored; the block top `exchange`s it with **acquire**, so a block that sees the flag reads that call's whole record, never a torn one, and the LAST restore staged before the block is the one that lands. Two flags with a fixed consumption order could not express last-writer-wins — an un-learned session loaded after a learned one inherited the learned references | GUI → Audio (**bounded staged record**, ADR-0012) | `src/dsp/AnabasisEngine.h` (`restoreLearnedTargets`), `src/dsp/AnabasisEngine.cpp` (block top) |
| Learned state → `getStateInformation` | `AdaptiveEngine::learned` atomic: refs published FIRST, flag **release**-stored; `hasLearned()` **acquire**-loads, so a saver that sees `true` reads the refs that store ordered before it | Audio → GUI (published state read as a unit — the ADR-0012 contract mirrored) | `src/dsp/AdaptiveEngine.h` (`commitLearn`/`hasLearned`), `src/PluginProcessor.cpp` (`getStateInformation`) |
| Macro listener → message-thread mapper | `std::atomic<bool> mappingPending` set from whichever thread APVTS delivers `parameterChanged` on; drained on the message thread by `AsyncUpdater` (only posted when already on the message thread) + a 30 ms `Timer`; `std::atomic<int> restoreDepth` suppresses the drain across a restore (`ScopedRestore`) | **no row — see OQ-014** | `src/MacroEngine.cpp:28-35,63-66`, `src/MacroEngine.h:92-101,139` |

**How the staged-record row came to exist (ADR-0012).** The two learned-target rows above were
first written here under invented row names ("momentary request + flag-orders-payload"), which read
as if the permitted-path table authorised them. It did not: the restore stages two floats plus a
discriminator behind a separate release-stored flag, and the sentinel row excluded verbatim
anything "wider than one lock-free scalar". The mechanism copied the GR ring's release/acquire
discipline, and Audio→GUI authorisation does not carry over to GUI→Audio. External review caught
it (2026-08-01), it was recorded as OQ-015 rather than redesigned under review pressure, and the
owner ratified the implementation unchanged: **ADR-0012** adds the staged-record row with its six
mandatory conditions, and the rows above now cite it. The implementation did not change; the
authorisation did. **OQ-013 remains open** — ADR-0012 gave its trim vector a permitted transport
but deliberately did not decide whether a restored vector may be injected into a running engine.

**The OQ-014 exception, stated rather than papered over.** `mappingPending` and `restoreDepth`
point any-thread → message-thread, a direction the table does not enumerate. They implement the
shape ADR-0005/ADR-0011 mandate ("the MacroEngine consumes macro changes solely through an async
message-thread listener" — `juce::AsyncUpdater` is itself an atomic flag plus a message post), so
one reading is that the table has a documentation gap; the other is that a small ratifying ADR is
owed. That is an owner call recorded in `OPEN_QUESTIONS.md` **OQ-014**, and this file deliberately
does not pre-empt it. The residual check-then-act window in the guard is recorded in
`KNOWN_ISSUES.md` KI-003.

## PDC

One `setLatencySamples` call site — `updateLatency()` (`src/PluginProcessor.h`,
`src/PluginProcessor.cpp`) — fed by the `const` race-free predictor in `src/dsp/Latency.h`,
which both the wrapper and the engine call so reported and actual cannot drift. Triggers, per
ADR-0004 item 5: `prepareToPlay`, the three `int_` latency-input callbacks (coalesced to one fire
per bulk read by `InternalState::ScopedLatencyBatch`), and `setNonRealtime()`. The last two are
host callbacks **not** delivered on the message thread — which is why the policy rule reads "off
the audio thread", not "on the message thread" (amended by ADR-0011, second correction). Nothing
recomputes PDC from `processBlock`.

## FTZ/DAZ

`juce::ScopedNoDenormals` at the top of `processBlock` (`src/PluginProcessor.cpp`) is the
single flush-to-zero mechanism; no module carries its own.

## Which context paints

The OpenGL context attaches on macOS/Windows only, never Linux/X11 (`src/gui/PluginEditor.h:38`
and the platform gate around its attach). When attached, JUCE paints components on the GL render
thread; when not, on the message thread. The rule that keeps both safe is the one the policy
already mandates: GUI-side reads of published state are stateless `const` peeks (at P1 the only
such read is `getLatencySamples()` in `paint()`), so the identity of the painting thread carries
no correctness weight. Recorded here per ADR-0011 §Consequences; no policy amendment implied.

## Planned edges (not yet in the tree)

- **Spectrum capture rings** (post-input-gain and post-chain, the dual-trace §2.9 overlay) —
  same ScopeBuffer idiom as the implemented GrHistoryBuffer; land with the P5 spectrum view.
- **Command atomics** — the forced-duck request and the P4 Learn start/stop + learned-target
  restore are IMPLEMENTED (see the table); the meter hold reset follows at P5, same
  single-atomic exchange shape. **Its scope is wider than a GUI button:** `dbTpMaxHold` and the
  integrated-LUFS histogram are session-cumulative and are cleared only by `prepareToPlay`, so
  they also survive a `setStateInformation` (loading a different session keeps the previous
  programme's true-peak maximum) and an `AudioProcessor::reset()` — which this processor does not
  override. Whether a state load should clear them is a P5 decision, not an oversight. The same
  decision covers `GrHistoryBuffer::reset()`, which today **rewinds** the ring's monotonic write
  index to 0 and bulk-clears all 4096 entries: a reader holding a cached `available()` would see
  the index go backwards, and the bulk clear is a host-thread write against a `const` peek that
  the SPSC row does not cover (it scopes the audio-thread producer). Inert with no reader before
  P5 — and the reader contract is what decides whether the index stays monotonic across a reset
  or gains a generation counter, so it is designed there rather than guessed at now.
- **Frozen trim vector transport** — **OQ-013 Hard Stop**: four scalars, no permitted mechanism
  yet; no code may wire it until its ADR lands. **ADR-0012 settled the transport** (the staged
  record row fits a four-scalar vector); what keeps OQ-013 open is whether a restored vector may
  be injected into a running engine at all.

## Verification

`AnabasisStateTests`: `testMacroRestoreDoesNotClobber` and `testDrainInsideRestoreIsSuppressed`
(both mutation-verified) pin the macro edge's restore semantics;
`testLatencyNotifyIsBatchedAcrossARead` pins one PDC fire per bulk read. What cannot be verified
headlessly — an actual off-message-thread host restore, real GL-thread painting — is listed in
KI-003 and `TESTING.md` §What cannot be verified headlessly. `REALTIME_SAFETY_AUDIT.md` (end of
P2) audits allocation/lock freedom on the audio thread; not claimed in advance (C7).
