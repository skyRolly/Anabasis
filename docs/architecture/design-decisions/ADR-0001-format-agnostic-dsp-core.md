# ADR-0001 — Format-agnostic DSP core via a POD EngineParameters snapshot

**Status:** Accepted (2026-07-31 — owner sign-off on `docs/DESIGN.md`)

## Context

`DSP_POLICY.md` invariant 13 is binding from day one: *the DSP core is format-agnostic* —
`src/dsp/` depends only on `juce_dsp` / `juce_audio_basics`, is driven by a POD parameter
snapshot, and never includes the plugin wrapper, the editor, or any JUCE GUI/plugin-client
header. Its invariant→test row is build-level (`AnabasisDSP` links without the wrapper, due P1),
so the decomposition has to be settled before the first CMake file exists rather than discovered
when the test is written.

`DESIGN.md` §1.1 fixes the shape: a wrapper layer in the global namespace (`PluginProcessor`,
`PluginParameters`, `MacroEngine`, `PresetManager`, editor) above a DSP core in namespace
`anabasis`, with a POD `anabasis::EngineParameters` snapshot as the only thing that crosses
between them. §1.3 enumerates the modules on each side. The sibling product already runs this
pattern in shipped code (its ADR-0001), which is why this is an inheritance decision rather than
an invention — but the *contract* still has to be written down here, because Anabasis carries
obligations Anamorph does not: a macro layer whose mode switch must be sound-neutral (§5.1), an
adaptive engine that trims values inside the engine (§5.4), and a latency figure predicted from
the same snapshot type (§3.3).

## Problem

The wrapper owns an APVTS; the engine needs its values every block. Nothing about the boundary is
self-evident:

- The engine could simply hold an `AudioProcessorValueTreeState&` and read it. That is fewer
  moving parts and no duplication, and plenty of shipped plugins do it.
- If a snapshot is used instead, its *transport* is open: rebuild the whole struct every block, or
  push only what changed through a lock-free queue.
- Not every input is an APVTS parameter. `int_oversample`, `int_osPhase` and `int_offlineQuality`
  are host-hidden `ANABASIS_INTERNAL` fields (§4.3) that never enter the parameter tree at all,
  yet they change the rendered audio.
- The core also has to be *packaged* so the invariant-13 build test can pass, and INTERFACE vs
  STATIC is a real fork with a linking consequence, not a style preference.

## Options

- **A. Engine holds the APVTS (or raw `AudioProcessorParameter*`s) and reads it directly.**
  Rejected. It couples the DSP to JUCE parameter types and, through them, to the wrapper: the
  core can no longer be compiled without `juce_audio_processors`, which fails
  `DSP_POLICY.md` invariant 13 outright and its P1 build-level test with it. It also makes the
  engine untestable without instantiating a plugin, spreads normalised/denormalised conversion
  and choice-index decoding across DSP files, and gives the audio thread no single, cheap point
  at which to ask *did anything change?*.
- **B. Lock-free FIFO of individual parameter-change events.** Rejected. Per-block work becomes a
  function of how fast the user (or a host automation lane) is moving controls rather than a
  constant; coalescing, ordering and queue-overflow policy all become engine concerns; and there
  is no single object to compare, so the no-change gate and the discrete-diff below have nothing
  to operate on. It buys nothing here — the full snapshot is a small POD struct, and rebuilding
  it unconditionally is cheaper than administering a queue. Anamorph reached the same conclusion:
  no message queue, no FIFO of parameter changes.
- **C. Full POD `EngineParameters` snapshot rebuilt once per block from cached
  `getRawParameterValue` atomics, handed to the engine by value.** **Chosen.** Constant per-block
  cost, no string lookups, no allocation, no locks; one object to compare; host-hidden fields join
  it from an atomic mirror on equal terms; and the engine never names a JUCE parameter type, so
  invariant 13 holds by construction rather than by discipline.
- **D. Package the core as a STATIC library.** Rejected. JUCE module headers compile module object
  code into each consuming target; a STATIC `AnabasisDSP` built against its own copy of the JUCE
  modules duplicates that object code and breaks linking against the plugin-format targets, which
  carry their own. The core is therefore an **INTERFACE** library whose sources compile into each
  consuming final target — same rationale as the sibling's `AnamorphDSP`.
- **E. Gate Advanced-only fields to neutral while building the snapshot** (the sibling's
  `advancedMode` behaviour). Rejected. It makes the view toggle a *value* change — the exact
  opposite of `MODE_AND_ADAPTATION_POLICY.md` invariants 1–2 and `DESIGN.md` §5.1. Recorded here
  because the pattern is being copied wholesale and this one line of it must not be.

## Decision

**Packaging.** The DSP core lives in namespace `anabasis` under `src/dsp/` and is built as the
`AnabasisDSP` **INTERFACE** library: include directory `src/dsp`, INTERFACE sources compiled into
each consuming final target. It links `juce_dsp` and `juce_audio_basics` and nothing else. No file
under `src/dsp/` may include the wrapper, the editor, or any JUCE GUI or plugin-client header.
The exact target graph, options and hardening flags that realise this are **ADR-0008's** scope;
ADR-0001 owns only the constraint that the core is a wrapper-free compilation unit set.

**The snapshot type.** `src/dsp/EngineParameters.h` declares a POD struct of enums, bools and
floats grouped by chain stage, every member carrying a neutral member-initialiser default. It has
no methods that touch JUCE parameter types, no pointers back into the wrapper, and no dynamic
members.

**Building it.** `PluginParameters` caches the `std::atomic<float>*` returned by
`apvts.getRawParameterValue()` once at construction (a `bind()` step), so no per-block lookup by
string ever occurs. Every `processBlock`, the wrapper calls `toEngine()`, which rebuilds the
**entire** snapshot with relaxed atomic loads, then hands it to the engine
(`setParameters(e)` followed by `process(buffer)`). Two rules on the build:

1. `toEngine()` is a **total, unconditional** function of the current parameter state. It must not
   gate, neutralise or skip a field on `advancedMode` or on any other view state — see option E.
2. Discrete choice indices are read with `juce::roundToInt`, never a truncating `(int)` cast: the
   exact-raw value restored by a host can sit a hair below its integer (e.g. `1.9999`), and a
   truncating cast silently selects the wrong choice.

Host-hidden `ANABASIS_INTERNAL` fields (§4.3) bypass APVTS entirely and enter the same snapshot
from `InternalState`'s relaxed atomic mirrors.

**Ownership of behaviour.** The engine owns all smoothing (20 ms standard time constant, §2.1) and
the continuous/discrete split; the wrapper owns none. The engine never sees a JUCE parameter type,
a parameter ID, or a normalised value.

**The no-change gate.** On adoption the engine compares the incoming snapshot against the one it
holds with a bitwise field-by-field `sameParameters` predicate. Equal ⇒ no smoother targets are
recomputed and no transition logic runs. Unequal ⇒ the snapshot is copied in, continuous fields
retarget their smoothers, and a **discrete diff** identifies which discrete fields moved so the
transition layer (§2.8) can route them — duck, latch, or neither.

**The maintenance rule (binding).** Every field added to `EngineParameters` **must** be added, in
the same unit of work, to all three functions: the `sameParameters` comparison, the copy, and the
discrete diff (if the field is discrete). A field present in the struct but absent from the
comparison makes the gate report "no change" for a real change, so the engine keeps stale targets;
absent from the copy it is never adopted; absent from the discrete diff its change bypasses the
duck and clicks. None of the three failures is caught by a compiler, so this is a review checklist
item on every parameter addition from P1 onward, and it is the reason the parameter surface
(ADR-0010) and this ADR must be read together.

## Consequences

- `DSP_POLICY.md` invariant 13 becomes structurally satisfiable and its P1 build-level test
  (`AnabasisDSP` links without the wrapper) becomes writable as stated.
- The engine is unit-testable without a plugin host: construct `EngineParameters`, call
  `setParameters` / `process`. Every P2 DSP test — null tests, the ceiling sweep, the impulse
  latency test — runs against the core directly.
- **Parameter changes take effect at block boundaries, not sample-accurately.** Sample-accurate
  intra-block automation is foreclosed. Accepted: the engine's 20 ms smoothing dominates a block
  of quantisation at any realistic buffer size, and no parameter in §4.2 is designed to be swept
  at audio rate (`lookahead` is explicitly set-and-leave, §4.2 note ³).
- Per-block cost is constant and independent of user activity: 49 APVTS fields (§4.2) plus the
  §4.3 host-hidden fields, all relaxed loads. The gate does **not** save the rebuild — it saves
  the downstream target recomputation and transition work, which is the expensive half.
- The same POD type is the input to the const, race-free `predictLatency(snapshot)` used on the
  message thread for PDC (§1.4, ADR-0004), so latency prediction and rendering can never disagree
  about which parameter values they are describing.
- The adaptive engine's trims (§5.4) are applied **inside** the engine, around snapshot values;
  they are not parameter writes and never appear in the snapshot the wrapper builds. The snapshot
  therefore stays a pure function of parameter state, which is what makes the §5.2 macro mapping
  and Freeze's bit-repeatability analysable.
- **The snapshot is not a serialization format.** It carries no compatibility contract: adding,
  reordering or removing a field is an internal change, never a `kVersion` bump. Session/preset
  schema is ADR-0007's; parameter IDs, ranges and defaults are ADR-0010's.
- Cost accepted: two representations of the same values (APVTS and the struct) and a maintenance
  rule the compiler cannot enforce. The alternative — one representation, option A — costs
  invariant 13.
- Forecloses the engine reading host state (transport, sample position, bus layout, parameter
  metadata) directly. Anything of that kind must be lifted into the snapshot by the wrapper or
  stay wrapper-side.

## Related code

None yet — P1 onward. Planned: `src/dsp/EngineParameters.h` (the POD snapshot),
`src/dsp/AnabasisEngine.{h,cpp}` (chain owner, snapshot adoption, `sameParameters` gate, discrete
diff, transitions), the remaining `AnabasisDSP` sources `src/dsp/TiltEq.h`,
`src/dsp/MasteringComp.{h,cpp}`, `src/dsp/ClipSat.{h,cpp}`, `src/dsp/LookaheadLimiter.{h,cpp}`,
`src/dsp/TruePeak.h`, `src/dsp/CeilingClamp.h`, `src/dsp/Dither.h`,
`src/dsp/LoudnessMeter.{h,cpp}`, `src/dsp/LoudnessComp.{h,cpp}`,
`src/dsp/AdaptiveEngine.{h,cpp}`, `src/dsp/GrHistoryBuffer.h`; and wrapper-side
`src/PluginParameters.{h,cpp}` (`pid::` IDs, layout, `toEngine()` snapshot builder),
`src/PluginProcessor.{h,cpp}` (per-block build + `predictLatency`), `src/InternalState.h`
(host-hidden atomic mirrors).

Evidence [Unverified]:
- Design: `docs/DESIGN.md` §1.1 (two-layer decomposition, POD snapshot from cached
  `getRawParameterValue` atomics, bitwise `sameParameters` gate, the compare/copy/discrete-diff
  maintenance rule), §1.3 (planned module inventory), §1.4 (threading model,
  `predictLatency(snapshot)`), §2.1 (20 ms smoothing constant), §4.2 (49 parameters), §4.3
  (host-hidden `ANABASIS_INTERNAL` fields outside APVTS), §5.1 (Anamorph's snapshot-time Advanced
  gating is the *opposite* of the Anabasis contract), §5.4 (trims are engine-internal, not
  parameter writes), §10 (ADR set and the ADR-0008 boundary).
- Research: `worklogs/2026-07-30-p0-anamorph-research.md` — "Parameter snapshot pattern
  (APVTS → audio thread)" (`bind()`, no FIFO, `roundToInt` for discrete indices), the
  `patterns_to_copy` POD-snapshot entry, and "CMake target graph — complete inventory"
  (INTERFACE-not-STATIC so sources compile into each consuming target, avoiding duplicated JUCE
  module object code).
- Precedent [Verified — cited via `DESIGN.md` §1.1]: `Anamorph:src/PluginParameters.cpp:286-389`
  (snapshot builder), `Anamorph:src/dsp/EngineParameters.h:28-103` (POD layout),
  `Anamorph:src/dsp/AnamorphEngine.cpp:160-209` (`sameParameters` gate + discrete diff).
- Precedent [Verified — cited via `DESIGN.md` §2.1 and §1.4]:
  `Anamorph:src/dsp/AnamorphEngine.cpp:58-81` (engine-side smoothing),
  `Anamorph:src/PluginProcessor.cpp:88-105` (const `predictLatency`, single `setLatencySamples`
  call site).
- Policy: `docs/policies/DSP_POLICY.md` invariant 13 and its invariant→test row;
  `docs/policies/MODE_AND_ADAPTATION_POLICY.md` invariants 1–2 (option E).
- Anabasis runtime claims are **Unverified** by construction: no `src/` exists at sign-off. Every
  statement about Anabasis behaviour above is the contract the P1+ code must satisfy, not a
  measurement (C2). Code evidence accrues from P1.
