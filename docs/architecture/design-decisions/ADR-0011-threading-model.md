# ADR-0011 — Threading model: two threads, no workers, atomic and SPSC publication

**Status:** Accepted (2026-07-31 — owner sign-off on `docs/DESIGN.md`)

## Context

`ADR_POLICY.md` lists the **threading model** among the changes an ADR is mandatory for, and
`THREADING_POLICY.md` is binding from day one: it fixes the thread inventory (audio ·
message/GUI · an optional GPU render context · **no worker threads**), enumerates the *only*
permitted cross-thread paths, forbids allocation/locking/painting/IO on the audio thread, pins
the atomic orderings, and defers the concrete model to `docs/architecture/THREAD_MODEL.md`
written at P1. It also states the enforcement asymmetry that makes this a P0 decision rather
than a P2 one: adding a thread — "including a worker for analysis, preset scanning, or
metering" — is an Architecture Review Gate item and an AI-agent Hard Stop. A thread admitted
casually at P2 cannot be removed casually at P3, and every lock-free contract in the engine is
written against the number of threads that exist.

`DESIGN.md` §1.4 fixes the inventory and the ownership split; §5.2 places the MacroEngine on the
message thread and makes its re-entrancy flag a plain `bool` on the strength of that placement.
The sibling product runs the same shape in shipped code — zero worker threads, FFT on the GUI
thread (`Anamorph:docs/architecture/THREAD_MODEL.md` [Verified]) — but Anabasis carries three
loads Anamorph does not, and each is a plausible argument for a worker: linear-phase FIR
oversampling kernels (§3.1), a 16× offline-render quality mode (§3.1), and an adaptive engine
with feature extraction plus Learn (§5.4). The P0 research pass named the first of these
explicitly as an open P0 decision "with no Anamorph precedent" — background FIR-design thread
versus precomputed kernels (`worklogs/2026-07-30-p0-anamorph-research.md`, `anabasis_notes`).
This ADR answers it.

## Problem

Three questions, none self-evident:

1. **How many threads, and does anything in Anabasis force a third?** Designing a linear-phase
   half-band FIR kernel set is not free, and doing it inside `prepareToPlay` lengthens a call
   the host makes while the transport may already be rolling. 16× oversampling multiplies both
   the buffer footprint and the per-block work. Either could be read as the classic
   "do it on a worker" case.
2. **What crosses between the threads, and in which direction?** Parameters have to reach the
   engine; meters, features and time-series history have to reach the UI; momentary commands
   (duck, Learn start/stop, meter hold reset) have to reach the audio thread. Each of these has
   at least two standard implementations, and the wrong choice for the parameter path in
   particular (a FIFO) makes per-block cost a function of user activity.
3. **Which thread owns the pieces that are neither pure DSP nor pure UI?** The MacroEngine
   writes managed parameters (§5.2); PDC has to be recomputed when a latency source moves
   (§3.3). Both touch state the other thread also touches, and both have an obvious-but-wrong
   home on the audio thread, where they would be cheap to call and impossible to make safe.

## Options

- **A. Two threads. Audio thread owns the engine and all analysis; message thread owns UI, the
  MacroEngine, Learn control and PDC; publication is relaxed atomics + SPSC rings; commands are
  single atomics. Chosen.** It is exactly the shape `THREADING_POLICY.md` permits, with no path
  outside its table, and it is proven in the sibling product at the same scale of metering and
  transition machinery. Every claim the rest of the design makes about determinism —
  Freeze's bit-repeatability (§5.4), the no-change gate (§1.1), the duck's silent-bottom swaps
  (§2.8) — is stated against a single audio-thread owner of engine state, and stays true here by
  construction rather than by discipline.
- **B. Add a background FIR-design worker for linear-phase kernels (and/or for 16× preparation).**
  Rejected for v1. It adds a thread — a Hard Stop and a Review Gate item — to satisfy a case that
  precomputation already covers: **all** oversampler instances, for **every** factor and **both**
  phase modes, are constructed and `initProcessing`'d at `prepare()`, so a runtime factor or
  phase change designs nothing, allocates nothing, and waits on nothing
  (`Anamorph:src/dsp/AnamorphEngine.cpp:44-56` [Verified] does this for three factors already).
  A worker would additionally have to publish a kernel set to the audio thread mid-stream — a new
  cross-thread path carrying a *payload*, which none of the permitted paths do — and would need a
  lifetime/cancellation story for the case where the user switches phase mode twice while a
  design is in flight. It buys a shorter `prepare()`; it costs the one property the whole
  threading model is chosen for.
- **C. Lock-free FIFO of parameter-change events, message → audio, instead of the per-block POD
  snapshot.** Rejected. Per-block work becomes a function of how fast controls are moving;
  coalescing, ordering and overflow policy become engine concerns; and there is no single object
  to compare, so the bitwise `sameParameters` no-change gate and the discrete diff that routes
  changes to the duck have nothing to operate on (ADR-0001). The snapshot is a small POD struct
  and rebuilding it unconditionally is cheaper than administering a queue. Anamorph proves the
  point in shipped code: no message queue, no FIFO of parameter changes
  (`Anamorph:src/PluginParameters.cpp:286-389`, `Anamorph:src/dsp/EngineParameters.h:28-103`
  [Verified]).
- **D. Run the macro mapping on the audio thread** (engine derives managed values from the macro
  positions at snapshot-adoption time). Rejected on two independent grounds. Mechanically, the
  mapper's output is `setValueNotifyingHost` on real parameters — a message-thread API with
  listener, gesture and undo consequences — so an audio-thread mapper would need a queue back to
  the message thread and its re-entrancy flag would have to become an atomic that can interleave
  with a user gesture, defeating the discriminator §5.2 depends on. Semantically it is the
  "internal derived values" option ADR-0005 already rejected: the Advanced view would display
  numbers the DSP is not using.
- **E. Recompute PDC from audio-thread state, or call `setLatencySamples` from the audio thread.**
  Rejected. `THREADING_POLICY.md` forbids it outright ("PDC/latency must be recomputed on the
  message thread via a `const`, race-free predictor"), and the reason is structural: the audio
  thread's *effective* latency follows the latched OS state, which by design changes only at a
  reset or the silent duck bottom, whereas the host wants the figure the moment a setting is
  chosen. Two owners of one number produce exactly the mid-block latency change the design
  forbids.
- **F. An analysis/metering worker** (spectrum FFT, gated integrated-LUFS accumulation, feature
  extraction, preset scanning). Rejected. Feature extraction is block-rate arithmetic on
  pre-allocated state and belongs where the samples already are; integrated LUFS is a fixed-size
  histogram accumulator, not a growing container, precisely so it stays on the audio thread
  (`REALTIME_AUDIO_POLICY.md` consequence 3); and the spectrum FFT runs GUI-side off the capture
  rings (§2.9), which is where Anamorph runs its FFT too. Nothing here is unbounded, so nothing
  here needs to be moved off the real-time path.
- **G. Per-module denormal handling** (flush-to-zero code inside each DSP module) instead of one
  `ScopedNoDenormals`. Rejected as the *mechanism*: it is per-module code that can be forgotten
  silently, and it duplicates what one scoped guard does for the whole block
  (`Anamorph:src/PluginProcessor.cpp:109` [Verified]). Note this rejects the mechanism, not
  `REALTIME_AUDIO_POLICY.md` consequence 6's additional advice that envelope state be flushed
  below a threshold — that is a numerical-robustness measure inside a module, not a second
  FTZ/DAZ mechanism.

## Decision

**Inventory. Two threads carry Anabasis work, and there are no worker threads in v1.**

**Audio thread** — sole owner of engine state. It runs: the chain (§1.2), all smoothing, the
transition state machine (§2.8), feature extraction (§5.4), meter ballistics (§2.9), and the
adaptive trim slewing. It adopts one POD `EngineParameters` snapshot per block, rebuilt by the
wrapper from cached `getRawParameterValue` atomics plus the `ANABASIS_INTERNAL` atomic mirrors
(ADR-0001). It never allocates, locks, paints or does IO. `juce::ScopedNoDenormals` at the top of
`processBlock` is the **single** FTZ/DAZ mechanism for the entire block; no module carries its own.

**Message thread** — owner of everything else: the editor and all visualisers, the MacroEngine
(§5.2), Learn start/stop control, preset/A-B/undo bulk swaps, `InternalState`, and PDC. Because
the macro parameters are non-automatable and the MacroEngine consumes macro changes only through
an async message-thread listener, the MacroEngine is message-thread-only **by construction** —
which is what lets its re-entrancy flag (§5.2's manual-edit discriminator) be a plain `bool` with
no atomics and no possibility of interleaving with a user gesture.

**Publication, audio → message** (audio thread is the only producer on every one of these):

- **Meters** — LUFS M/S/I, dBTP, PLR, GR: pre-converted values stored into `std::atomic<float>`
  in a single per-block `publish()`, `memory_order_relaxed`. All ballistics state stays in plain
  floats on the audio thread; the GUI does no maths (`Anamorph:src/gui/LevelMeter.cpp:12-73`,
  `Anamorph:src/dsp/ScopeBuffer.h:21-91` idiom family [Verified]).
- **Adaptive features** — short-term LUFS, crest factor, spectral tilt/centroid, transient
  density: the same relaxed per-block atomic publish (§5.4).
- **Time series** — GR/waveform history (`GrHistoryBuffer`) and the two spectrum capture points
  (post-input-gain and post-chain, §2.9): three SPSC rings, one producer and one reader each,
  power-of-two storage, **one release-store per block** on a monotonic write index, acquire on
  the read side, reads are stateless `const` peeks that consume nothing
  (`Anamorph:src/dsp/ScopeBuffer.h:21-91` [Verified]).
- **Staleness hints** — relaxed monotonic generation counters carrying no payload.

**Commands, message → audio** — one `std::atomic` per request, consumed with `exchange` at the
top of the audio-thread consumer: the forced-duck request (set *before* the parameter swap,
§2.8), Learn start/stop, meter hold reset, and the sentinel-valued per-slot inject atomics that
deliver a frozen trim vector at the silent duck bottom (§4.4, §5.4 — the `abMatchGain` idiom,
`Anamorph:src/PluginProcessor.cpp:485-491` [Verified]). Host-hidden engine config
(`int_oversample`, `int_osPhase`, `int_offlineQuality`) crosses through the `InternalState`
atomic mirror, not through a command.

**PDC.** Latency is recomputed **only** on the message thread, by a `const`, race-free
`predictLatency(snapshot)` taking the same POD type the engine renders from, feeding a **single**
`setLatencySamples` call site (`Anamorph:src/PluginProcessor.cpp:88-105` [Verified]). It is
invoked from `prepareToPlay`, from the `InternalState` change callbacks for **all three**
latency-bearing host-hidden fields — `int_oversample`, `int_osPhase` **and
`int_offlineQuality`** — and from **`setNonRealtime()`**. Under ADR-0004 those are the only
remaining latency sources, so no APVTS listener needs to drive PDC at all.

> **Correction, same day (2026-07-31), for consistency with ADR-0004 — not a change of decision.**
> An earlier revision of this paragraph listed only the OS factor and phase. That contradicted
> ADR-0004 decision item 5: at `int_offlineQuality = Force Max` an offline bounce renders at 16×
> and *"the reported figure during `isNonRealtime()` uses the forced factor"*, which makes
> `int_offlineQuality` a **third** input to reported latency and the realtime→offline transition a
> **fourth** recompute trigger. Left as written, a P1 author would wire no recompute for a
> Force-Max change, and — in hosts that do not re-`prepare` when entering offline render — the host
> would compensate for the live factor while the render ran at 16×, time-shifting the bounce
> against the rest of the project. `setNonRealtime()` is named explicitly because it is the only
> callback guaranteed to fire on that transition.

The audio thread's effective latency follows its own latched OS state and is never written from the
message thread. The engine's dry-fill gate compares the predicted figure against the latched one
before engaging (`Anamorph:src/dsp/AnamorphEngine.cpp:290-307` [Verified]).

**No worker threads, and the two reasons one might be wanted are closed at `prepare()`.** Every
oversampler instance — every factor in `Off/2×/4×/8×/16×`, both phase modes — is constructed and
`initProcessing`'d at `prepare()`, and the linear-phase FIR kernels are designed there, so a
runtime factor or phase change is a latched selection among objects that already exist: it
designs nothing, allocates nothing and blocks nothing. All buffers — the lookahead line, the dry
ring (`maxLookahead(10 ms) + maxOsLatency(16×, linear) + maxBlock + 1`, §3.3), and the
oversampled scratch — are sized at `prepare()` for the **maximum** factor, which is what makes
the 16× Force-Max offline path (§3.1) require no separate machinery: it is the same latched
switch on buffers that were already large enough.

**Adding any thread later** — worker, analysis, scanner or otherwise — is an Architecture Review
Gate item and an AI-agent Hard Stop, and requires an ADR superseding this one.

## Consequences

- `THREADING_POLICY.md`'s permitted-path table is satisfied with **no path outside it**: every
  cross-thread edge above is one of its six rows. The policy's "Adaptive engine — where it runs"
  clause, which explicitly defers the choice to "an ADR before implementation", is discharged
  here: feature extraction and trim slewing run on the audio thread within the real-time budget
  (§9's ≤0.5% metering-and-features allocation), not on a worker and not on the message thread.
- The audio thread is the single owner of engine state, so Freeze's bit-repeatability
  (`MODE_AND_ADAPTATION_POLICY.md` invariant 3), the bitwise no-change gate, and the duck's
  silent-bottom swap all reason about one writer. No engine field needs to be atomic; the atomics
  in the design are boundary objects only.
- **`prepare()` gets more expensive, and that is the accepted trade.** Designing linear-phase FIR
  kernels and `initProcessing`-ing every oversampler up front moves work into a call the host
  makes off the audio thread, in exchange for a runtime factor/phase switch that cannot allocate
  or stall. Memory is likewise sized for 16× linear-phase whether or not the user ever selects
  it. Both costs are bounded and paid once per `prepareToPlay`; a worker would have traded them
  for an unbounded new failure surface.
- **Forecloses**, without an ADR superseding this one: any background analysis (offline loudness
  scan, batch preset analysis, asynchronous preset-folder scanning), any deferred kernel design,
  any audio-thread parameter write, and any second producer on a scope/GR ring.
- Parameter changes land at block boundaries; sample-accurate intra-block automation is
  foreclosed (ADR-0001's consequence, restated because it is a threading property).
- Macro behaviour under a host that exposes the non-automatable macro parameters anyway
  (`PARAMETER_COMPATIBILITY_POLICY.md` rule 5) is message-thread-rate mapping, and
  offline-render determinism for that unsupported usage is explicitly not promised (§5.2). This
  is a consequence of the MacroEngine's thread placement, so it is recorded here as well as in
  ADR-0005.
- The UI reads published atomics and ring peeks only — never engine state — so the vblank-paced
  FrameClock and the snapshot repaint gates (§6.5, `Anamorph:src/gui/FrameClock.h:10-167`
  [Verified]) can be copied without any locking layer, and an idle or hidden visualiser costs
  nothing.
- **One inventory nuance to record precisely at P1, not resolved here.** `THREADING_POLICY.md`
  names an "(optional) GPU render context" in its own inventory, and §6.1 attaches OpenGL on
  macOS/Windows (never Linux/X11). That context is created and driven by JUCE, is not a worker
  thread, and holds no Anabasis state — but component painting runs on it when attached, while
  the policy's ring rule is phrased with the message thread as the single reader. The rule that
  keeps this safe either way is already mandated and already the design: ring reads are
  **stateless `const` peeks**, so multiple read sites are safe. `docs/architecture/THREAD_MODEL.md`
  (P1) records which context paints, per repository; no policy amendment is asserted here.
- `docs/architecture/THREAD_MODEL.md` becomes a required P1 deliverable — `THREADING_POLICY.md`
  points at it for the concrete model and it does not yet exist. It is written from this ADR with
  code citations, and `REALTIME_SAFETY_AUDIT.md` (end of P2) audits compliance; neither is
  claimed in advance (C7).

## Related code

None yet — P1 onward. Planned: `src/PluginProcessor.{h,cpp}` (`processBlock` with
`ScopedNoDenormals` at the top, per-block snapshot build, command atomics, `predictLatency` and
the single `setLatencySamples` call site), `src/PluginParameters.{h,cpp}` (cached
`getRawParameterValue` atomics, `toEngine()`), `src/InternalState.h` (host-hidden atomic mirrors
and the PDC-recompute callback), `src/MacroEngine.{h,cpp}` (message-thread mapper and its
re-entrancy flag), `src/PresetManager.{h,cpp}`, `src/dsp/AnabasisEngine.{h,cpp}` (single-owner
engine state, transition state machine, duck/inject atomic consumers),
`src/dsp/EngineParameters.h`, `src/dsp/GrHistoryBuffer.h` (SPSC ring),
`src/dsp/LoudnessMeter.{h,cpp}`, `src/dsp/LoudnessComp.{h,cpp}`,
`src/dsp/AdaptiveEngine.{h,cpp}` (audio-thread features, trims, Learn accumulation),
`src/gui/FrameClock.h`, and the visualisers `GrHistoryView`, `LoudnessMeterView`, `SpectrumView`,
`CurveView`.

Evidence [Unverified]:
- Design: `docs/DESIGN.md` §1.4 (the thread inventory, the audio/message ownership split, relaxed
  atomics + SPSC publication, command atomics, `ScopedNoDenormals` as the single FTZ/DAZ
  mechanism, `predictLatency(snapshot)` with a single `setLatencySamples` call site, and "no
  worker threads in v1" with the precomputed-kernel/`initProcessing` rationale and the 16×
  buffer-sizing rule), §5.2 (MacroEngine on the message thread; the re-entrancy flag needs no
  atomics because it is message-thread-only by construction; the async-listener path for hosts
  that expose the macro anyway), §1.1 and §1.3 (POD snapshot boundary; planned modules), §2.8
  (forced-duck atomic set before the swap; only OS factor/phase latches), §2.9 (meter atomics,
  the GR SPSC ring, the two spectrum capture rings, GUI-side FFT, fixed-size LUFS accumulator),
  §3.1 and §3.3 (all oversamplers prepared up front; dry-ring sizing; latency sources), §5.4
  (audio-thread feature extraction, trim slewing, Learn, per-slot sentinel inject), §6.5 (UI
  reads published atomics only), §9 (metering + features budget allocation), §10 (ADR-0011 scope).
- Research: `worklogs/2026-07-30-p0-anamorph-research.md` — "Anamorph has zero worker threads
  (THREAD_MODEL.md:13, FFT on GUI thread); Anabasis's linear-phase mode and 16x offline
  oversampling may not fit that constraint — a P0 decision (background FIR design thread vs
  precomputed kernels) with no Anamorph precedent" (`anabasis_notes`); "Meter/scope data crossing
  to the GUI thread" (SPSC ring + relaxed-atomic publish, `exchange` command channel);
  "Parameter snapshot pattern (APVTS -> audio thread)" (no message queue, no FIFO of parameter
  changes); "Denormal / FTZ and non-finite handling"; `patterns_to_copy` entries for the SPSC
  ring, the per-block atomic publish, the single `setLatencySamples` call site, and the
  pre-built/`initProcessing`'d oversamplers.
- Precedent [Verified — cited via `DESIGN.md` §1.4]:
  `Anamorph:docs/architecture/THREAD_MODEL.md` (zero worker threads),
  `Anamorph:src/PluginProcessor.cpp:109` (`ScopedNoDenormals` as the single FTZ/DAZ mechanism),
  `Anamorph:src/PluginProcessor.cpp:88-105` (const race-free `predictLatency`, single
  `setLatencySamples` call site),
  `Anamorph:src/dsp/AnamorphEngine.cpp:44-56` (all oversamplers pre-built and
  `initProcessing`'d at prepare, so a factor switch never allocates).
- Precedent [Verified — cited via `DESIGN.md` §1.1, §2.9, §3.3, §5.4, §6.5]:
  `Anamorph:src/PluginParameters.cpp:286-389` and `Anamorph:src/dsp/EngineParameters.h:28-103`
  (per-block POD snapshot instead of a parameter FIFO),
  `Anamorph:src/dsp/ScopeBuffer.h:21-91` (SPSC ring, one release-store per block, stateless
  acquiring peeks), `Anamorph:src/dsp/AnamorphEngine.cpp:290-307` (dry-fill gated on
  predicted == latched latency), `Anamorph:src/PluginProcessor.cpp:485-491` (sentinel-atomic
  per-slot inject consumed at the duck bottom), `Anamorph:src/PluginProcessor.cpp:338-421`
  (gesture-bracketed edits vs automation folded into the baseline — the message-thread signal
  §5.2's discriminator reuses), `Anamorph:src/gui/FrameClock.h:10-167` and
  `Anamorph:src/gui/LevelMeter.cpp:12-73` (vblank pacing, atomic meter sources, repaint gates).
- Policy: `docs/policies/THREADING_POLICY.md` (thread inventory, permitted-path table, forbidden
  cross-thread access, atomic ordering rules, adaptive-engine clause deferred to this ADR,
  enforcement); `docs/policies/REALTIME_AUDIO_POLICY.md` (lock-free/allocation-free rule,
  `ScopedNoDenormals` required, consequences 1–6); `docs/policies/ADR_POLICY.md` (threading model
  is ADR-mandatory); `docs/policies/ARCHITECTURE_REVIEW_GATE.md` and
  `docs/policies/AI_AGENT_POLICY.md` (thread-model change is a Hard Stop).
- Related ADRs: ADR-0001 (POD snapshot boundary, block-boundary parameter adoption), ADR-0004
  (latency contract — why OS factor/phase are the only PDC inputs), ADR-0005 (macro-layer
  contract whose threading half is settled here), ADR-0007 (per-slot state whose audio-side
  restore uses the inject atomic).
- Anabasis runtime claims are **Unverified** by construction: no `src/` exists at sign-off. Every
  statement about Anabasis behaviour above is the contract the P1+ code must satisfy, not a
  measurement (C2). Code evidence accrues from P1.
