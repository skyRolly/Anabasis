# THREAD_MODEL.md

The **implemented** thread model, written from **ADR-0011** (Accepted 2026-07-31) with code
citations, as `THREADING_POLICY.md` requires of P1. This file *describes* the code; the binding
rules stay in the policy and the ADR. When this file and the code disagree, the code is drifting or
this file is stale — report it (`SOURCE_OF_TRUTH.md`), do not silently pick one.

**Status:** P1 skeleton. The inventory below covers every cross-thread edge that exists in the
tree today. Rows the ADR plans for P2–P4 (SPSC rings, command atomics, the OQ-013 trim transport)
are listed as **planned** so their absence is visible rather than implied.

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
| Parameters → engine | 44 cached `getRawParameterValue` atomics read once per block into the POD snapshot (`CachedParams::toEngine`), never piecemeal | GUI → Audio (automatable params) | `src/PluginParameters.cpp` (`resolve`/`toEngine`), `src/PluginProcessor.cpp:81-88` |
| Host-hidden config → engine | `InternalState` relaxed atomic mirrors (`osMirror`/`phaseMirror`/`offlineMirror`/`lockMirror`), synced on every tree write, read per block | GUI → Audio (host-hidden session state) | `src/InternalState.h:153`, `syncAtomics()` |
| `nonRealtime` → latency predictor | `std::atomic<bool> nonRealtimeFlag`, written in `setNonRealtime()` (a host-thread callback), read by the snapshot build and the predictor | atomic mirror (same class as the row above) | `src/PluginProcessor.h:110`, `src/PluginProcessor.cpp:48-51,66,88` |
| Engaged lookahead window → any reader | `std::atomic<int> engagedWindow`, relaxed, written per sample on the audio thread; payload-free diagnostic (the smoothing test reads it) | Audio → GUI (staleness-hint class: monotonic display/diagnostic data) | `src/dsp/AnabasisEngine.h:84,54-55` |
| Forced-duck request → engine | `std::atomic<bool> duckRequested`, set by the wrapper before every bulk swap (A/B, preset, session load), `exchange`-consumed at the block top | GUI → Audio (momentary / transient requests) | `src/dsp/AnabasisEngine.h` (`requestForcedDuck`), `src/PluginProcessor.cpp` (three call sites) |
| Meters → GUI | `pubLufsM/S/I`, `pubDbTpMax`, `pubPlr`, `pubGrDb` — relaxed atomics, ONE publish per block from `processBlock`; plus the engine's `grMinLinear`/`engagedWindow` diagnostic atomics | Audio → GUI (meters) | `src/PluginProcessor.h` (meter getters), `src/PluginProcessor.cpp` (the per-block publish) |
| GR/waveform history → GUI | `GrHistoryBuffer`: 4096-entry power-of-two SPSC ring, entry written FIRST, monotonic index release-stored AFTER, acquire-loaded stateless peeks on the reader side | Audio → GUI (time series, SPSC ring) | `src/dsp/GrHistoryBuffer.h` |
| Learn start/stop → engine | `std::atomic<bool> learnStartReq`/`learnStopReq`, set by the wrapper (`startLearn`/`stopLearn`), `exchange`-consumed at the block top | GUI → Audio (momentary / transient requests) | `src/dsp/AnabasisEngine.h` (`requestLearnStart/Stop`), `src/dsp/AnabasisEngine.cpp` (block top) |
| Learned-target restore → engine | `pendingRefOnset`/`pendingRefTilt` stored relaxed FIRST, then `adaptiveRestorePending` **release**-stored; the block top `exchange`s the flag with **acquire**, so a block that sees it reads that call's pair, never a torn one. `adaptiveClearPending` (plain flag, no payload) is the "absent ADAPTIVE" leg | GUI → Audio (momentary request + flag-orders-payload) | `src/dsp/AnabasisEngine.h` (`restoreLearnedTargets`), `src/dsp/AnabasisEngine.cpp` (block top) |
| Learned state → `getStateInformation` | `AdaptiveEngine::learned` atomic: refs published FIRST, flag **release**-stored; `hasLearned()` **acquire**-loads, so a saver that sees `true` reads the refs that store ordered before it | Audio → GUI (published state, flag-orders-payload) | `src/dsp/AdaptiveEngine.h` (`commitLearn`/`hasLearned`), `src/PluginProcessor.cpp` (`getStateInformation`) |
| Macro listener → message-thread mapper | `std::atomic<bool> mappingPending` set from whichever thread APVTS delivers `parameterChanged` on; drained on the message thread by `AsyncUpdater` (only posted when already on the message thread) + a 30 ms `Timer`; `std::atomic<int> restoreDepth` suppresses the drain across a restore (`ScopedRestore`) | **no row — see OQ-014** | `src/MacroEngine.cpp:28-35,63-66`, `src/MacroEngine.h:92-101,139` |

**The OQ-014 exception, stated rather than papered over.** `mappingPending` and `restoreDepth`
point any-thread → message-thread, a direction the table does not enumerate. They implement the
shape ADR-0005/ADR-0011 mandate ("the MacroEngine consumes macro changes solely through an async
message-thread listener" — `juce::AsyncUpdater` is itself an atomic flag plus a message post), so
one reading is that the table has a documentation gap; the other is that a small ratifying ADR is
owed. That is an owner call recorded in `OPEN_QUESTIONS.md` **OQ-014**, and this file deliberately
does not pre-empt it. The residual check-then-act window in the guard is recorded in
`KNOWN_ISSUES.md` KI-003.

## PDC

One `setLatencySamples` call site — `updateLatency()` (`src/PluginProcessor.h:84`,
`src/PluginProcessor.cpp:76`) — fed by the `const` race-free predictor in `src/dsp/Latency.h`,
which both the wrapper and the engine call so reported and actual cannot drift. Triggers, per
ADR-0004 item 5: `prepareToPlay`, the three `int_` latency-input callbacks (coalesced to one fire
per bulk read by `InternalState::ScopedLatencyBatch`), and `setNonRealtime()`. The last two are
host callbacks **not** delivered on the message thread — which is why the policy rule reads "off
the audio thread", not "on the message thread" (amended by ADR-0011, second correction). Nothing
recomputes PDC from `processBlock`.

## FTZ/DAZ

`juce::ScopedNoDenormals` at the top of `processBlock` (`src/PluginProcessor.cpp:81`) is the
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
  single-atomic exchange shape.
- **Frozen trim vector transport** — **OQ-013 Hard Stop**: four scalars, no permitted mechanism
  yet; no code may wire it until its ADR lands.

## Verification

`AnabasisStateTests`: `testMacroRestoreDoesNotClobber` and `testDrainInsideRestoreIsSuppressed`
(both mutation-verified) pin the macro edge's restore semantics;
`testLatencyNotifyIsBatchedAcrossARead` pins one PDC fire per bulk read. What cannot be verified
headlessly — an actual off-message-thread host restore, real GL-thread painting — is listed in
KI-003 and `TESTING.md` §What cannot be verified headlessly. `REALTIME_SAFETY_AUDIT.md` (end of
P2) audits allocation/lock freedom on the audio thread; not claimed in advance (C7).
