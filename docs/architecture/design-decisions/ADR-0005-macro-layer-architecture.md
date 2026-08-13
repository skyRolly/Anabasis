# ADR-0005 — Macro layer: message-thread mapper, non-automatable macros, detach/re-engage coexistence

**Status:** Accepted (2026-07-31 — owner sign-off on `docs/DESIGN.md`)

## Context

`DEVELOPMENT_BRIEF.md` §5.3 names the Simple/Advanced relationship a *key architectural
constraint*; `MODE_AND_ADAPTATION_POLICY.md` binds it with invariants 1–6, and `ADR_POLICY.md`
makes any macro-layer decision (Simple → Advanced mapping, mode-switch semantics) ADR-mandatory.
Invariant 5 additionally requires the manual-edit coexistence strategy (OQ-004) to be argued in
`DESIGN.md` and recorded as an ADR **before** P4 implementation.

Anabasis exposes three macro controls — `loudness` (0…100, default 0), `character` (0…1, default 0)
and `tone` (−1…+1, default 0), rows 3–5 of the §4.2 surface — over 49 APVTS parameters, plus an
adaptive engine that applies bounded trims inside the DSP (§5.4). Anamorph offers **no usable
precedent**: its Advanced toggle *gates parameters to neutral at snapshot time*, so toggling the
view can change the sound (`Anamorph:src/PluginParameters.cpp:326-389` [Verified]) — the exact
inverse of invariant 2. The architecture is therefore new, argued in `DESIGN.md` §5, and ratified
here.

## Problem

Four questions, none of them obvious, and each one able to break a different invariant:

1. **Where does a managed parameter's value live?** If the macro is a knob that *computes* values,
   the natural implementation keeps macro-derived values inside the engine and leaves APVTS holding
   only what the user typed — which puts one value in two places and makes invariant 1's "the
   Simple knob and a host automating an Advanced parameter write to the same place" false.
2. **How does the mapper tell its own writes apart from a manual edit?** The detach rule of §5.3
   fires on manual edits. The MacroEngine writes the same parameters through the same interface, so
   without a discriminator the mapper's first gesture would detach every parameter it just wrote —
   the exact inverse of the re-engage contract.
3. **What happens to a manual Advanced edit when the user returns to Simple?** Invariant 5 forbids
   discarding it silently; invariant 2 forbids the switch itself from moving anything. Those two
   together do not pick a strategy — they only rule out the lazy ones.
4. **Are the adaptive engine's trims a violation of invariants 1 and 6?** They change the rendered
   sound without being Advanced-parameter values, which reads on its face like the "shadow state"
   invariant 1 bans and the §5.2 "internal derived values" option this ADR rejects.

## Options

**Where managed values live**

- **A. Internal derived values.** The engine computes the managed values from the macro position at
  snapshot time; APVTS holds only manually set values. *Lost:* it creates two places a value lives,
  makes the Advanced view display numbers the DSP is not using, muddies automation and undo
  semantics, and directly weakens invariant 1's same-place consequence.
- **B. Macros as real, host-visible, non-automatable APVTS parameters, mapped by a message-thread
  MacroEngine that writes the managed Advanced parameters through `setValueNotifyingHost`.**
  **Chosen.** One value model; the Advanced view always shows what the DSP is using.
- **C. Automatable macros.** *Lost:* replaying a macro lane means the plugin writes other
  parameters during host playback; hosts differ on recording/feedback semantics, and an offline
  bounce would depend on message-thread timing. Revisitable later via a kVersion bump + ADR
  (Anamorph precedent for exposing parameters after v1: its ADR-0014).

**Telling a macro write from a manual edit**

- **D. Compare the incoming value against the expected curve value.** *Lost:* it is a float
  comparison against a value the engine is mid-glide toward, so it misfires on both sides — a
  manual edit that happens to land on the curve would not detach, and a rounding difference would
  detach spuriously.
- **E. Message-thread re-entrancy flag AND gesture bracket, both required.** **Chosen.** The flag
  is exact rather than approximate, and the gesture bracket is the same signal the undo coalescer
  already needs.

**Manual-edit coexistence (OQ-004)**

- **F. Discard manual edits on return to Simple.** *Lost:* barred outright by
  `MODE_AND_ADAPTATION_POLICY.md` invariant 5 — a manual edit is user intent.
- **G. Carry-over offsets** — preserve manual edits as deltas riding the macro curve. *Lost:* the
  knob's sound at a given position becomes history-dependent and untestable (the mapping stops
  being a pure function), offsets compound silently with the adaptive trims, and an A/B between two
  slots carrying different offset sets is unexplainable to the user.
- **H. Macro-latch with re-engage on touch** — the switch moves nothing, edited parameters detach
  per-parameter, the next macro gesture re-engages them. **Chosen.** It keeps one source of truth
  and implements the brief's first suggested direction, with the re-engage gesture as the "clear
  notice" moment.

**Adaptive trims**

- **I. Trims as APVTS parameter writes.** *Lost:* second-scale modulation sprayed at the host
  pollutes automation lanes and the undo stack, and the host would record machine-generated motion
  as user intent.
- **J. Engine-internal bounded trims with a display-only delta overlay.** **Chosen** — see the
  invariant 1/6 reconciliation in the Decision.

## Decision

1. **Macro surface.** `loudness`, `character` and `tone` are real, host-visible APVTS parameters
   declared `withAutomatable(false)`. That flag is *advisory* — some hosts expose the parameter
   regardless (`PARAMETER_COMPATIBILITY_POLICY.md` rule 5) — so the MacroEngine consumes macro
   changes **exclusively** through an async message-thread listener. A host that writes a macro
   anyway gets the mapping applied at message-thread rate; offline-render determinism for that
   (unsupported) usage is explicitly not promised. The automation surface is the managed Advanced
   parameters themselves (invariant 6).

2. **Mapper.** `MacroEngine` runs on the message thread only. The mapping is a **pure function**
   `managedTargets = M(loudness, character, tone)` — no history, no hidden state — written to APVTS
   via `setValueNotifyingHost`, gesture-bracketed per knob drag and rate-limited to control rate;
   the engine's 20 ms parameter smoothing makes the glide click-free. The managed set is the nine
   parameters of §5.5: `limGain`, `compThreshold`, `compRatio`, `clipDrive`, `clipShape`,
   `colourDepth`, `dynTilt`, `eqTilt`, `colourTone`. `clipMix` and `compMix` are **not** managed and
   stay manual controls.

3. **Macro-write vs manual-edit discriminator.** A parameter change counts as a *manual edit* only
   when **both** conditions hold:
   - **Not macro-originated.** The MacroEngine raises a re-entrancy flag around its whole write
     burst; the listener that sets detach bits ignores every change seen while the flag is raised.
     Message-thread-only by construction, so the flag needs no atomics and cannot interleave with a
     user gesture.
   - **Gesture-bracketed.** The change arrived inside a `beginChangeGesture`/`endChangeGesture`
     pair — a real UI drag or a host-side automation *write* gesture. Ungesture-d writes
     (automation playback, preset apply, A/B, undo) never detach, matching Anamorph's rule that host
     automation folds into the baseline rather than becoming an edit
     (`Anamorph:src/PluginProcessor.cpp:338-489` [Verified]).

   This also settles the "host writes the macro anyway" case: such a write is macro-originated, the
   flag suppresses detaching, and the managed parameters simply follow — the mapping stays a
   function of the macro position.

4. **Mode switch.** Switching Simple ⇄ Advanced moves **nothing**; no value path exists across the
   switch by construction (invariant 2). `testModeSwitchIsSoundNeutral` guards the implementation.

5. **Detach.** A manual edit sets a per-parameter detach bit. The mask is **per-A/B-slot** state and
   therefore also per-undo-step (the StateSet unit is widened accordingly, §7); presets carry it.
   Its schema home is ADR-0007's business — this ADR fixes the semantics, not the layout. The mask
   restores on the **message thread** with the rest of the slot, since its only consumer is the
   MacroEngine and nothing on the audio thread reads it. The Simple view badges the macro knob as
   *edited* while any bit is set (wording is owner-supplied product text, C8).

6. **Re-engage.** The **next macro-knob gesture re-engages every detached parameter**: targets come
   from the curve at the new macro position and are reached through the normal rate-limited glide.
   A "reset to macro" affordance re-engages without moving the macro position.

7. **Preset / A-B / undo apply is never a macro move.** Those writes are ungesture-d *and* are
   applied inside the MacroEngine's re-entrancy flag, so the listener neither detaches anything nor
   re-maps: stored managed values land exactly as saved. Both conditions are required — without
   them a preset holding off-curve managed values could not be recalled faithfully at all, because
   the mapper would immediately overwrite them from the restored macro position.

8. **Fixed-point rule.** For every managed parameter, `M(loudness=0, character=0, tone=0)` **must
   equal that parameter's declared default** in §4.2, or the first macro gesture jumps the factory
   patch instead of gliding from it. This is why the colour amount is `colourDepth` (default 0) and
   not `clipMix` (default 100% wet, so it can never be a fixed point of a curve starting at zero
   colour). Guarded by `testMacroDefaultIsFixedPoint`, registered against
   `MODE_AND_ADAPTATION_POLICY.md` invariant 1 and in `procedures/TESTING.md`. A P4 curve revision
   that breaks the rule is a defect, not a taste choice.

9. **Audibility rule.** The arithmetic fixed point is necessary but not sufficient: **a managed
   target's audibility must not be gated by an unmanaged discrete parameter at the factory
   default.** `Clean` is the null colour model, so with `colourModel = Clean` raising `colourDepth`
   applies more of nothing and the Character macro is *inert* — correct behaviour, fatal as a
   default. The factory default model is therefore **`Tape`**, not `Clean`; `colourDepth = 0` keeps
   the default patch bit-identical either way (DSP_POLICY invariant 7).

10. **Adaptive trims are engine-internal and are not parameter writes.** Release, stereo-link,
    sidechain-HPF and dynamic-tilt trims are bounded deltas applied inside the engine around the
    current parameter values, slewed on second-scale constants with hysteresis; the host never sees
    them, undo never records them, and the Advanced view shows them as a display-only delta
    overlay. Bounds are hard: a trim cannot exceed a parameter's declared range, cannot touch
    lookahead or the oversampling controls (invariant 4), and cannot move a locked parameter.
    **Invariant 1/6 reconciliation:** invariant 1 governs the *macro knob*, and everything the knob
    does routes through real Advanced parameters — including `dynTilt` (row 48), which exists as a
    real parameter precisely so the macro's HF tame is expressible as parameter values and reachable
    by host automation. The trims are the *adaptive engine*, governed by invariants 3 and 4, which
    explicitly permit adaptation to move parameters within their declared ranges. The §5.2 rejection
    of "internal derived values" does not transfer: there the *user's own edit* would live in two
    places; here the delta is machine-generated, bounded, displayed, deterministic in the input
    programme (same audio + same parameters ⇒ same trims), zeroed when adaptation is idle, and
    latched-and-serialized under Freeze. The invariant-6 caveat is stated rather than hidden: while
    adaptation runs, identical Advanced values do not *alone* determine the processing — the
    programme does too. That is the definition of the brief's adaptive engine, and **Freeze restores
    strict value-determinism** by latching the trim vector (bit-repeatable output thereafter), with
    the frozen vector **serialized per A/B slot** (ADR-0007). *(Corrected 2026-07-31, same day: this
    clause named "the sentinel-atomic inject pattern" as the restore transport. That reads as a
    settled mechanism, and an ADR outranks the policy — so it would have licensed wiring the very
    path `OPEN_QUESTIONS.md` **OQ-013** blocks. The `abMatchGain` precedent moves one float; the trim
    vector is four scalars, so the transport is an open thread-model decision and an AI-agent **Hard
    Stop**. What this item actually needs is only that the vector is per-slot, which is ADR-0007's to
    fix — the transport it travels by does not affect any claim made here.)*

## Policy amendments enacted by this ADR

`ADR_POLICY.md` rule 5 makes an ADR the instrument that changes a Policy, and this change set treats
an ADR/policy divergence as a defect — so the two `MODE_AND_ADAPTATION_POLICY.md` edits this
decision requires are carried here as prescribed blocks, matching the enacted text verbatim.

- **Invariant 1 gains a named guard, with its binary.** Appended:

  > Guarded by: `testMacroDefaultIsFixedPoint` in **`tests/state_tests.cpp` (`AnabasisStateTests`)** —
  > for every managed parameter, the mapping evaluated at the default macro position must equal that
  > parameter's declared default, or the first macro gesture jumps the factory patch instead of gliding
  > from it (ADR-0005). The **binary is named deliberately**: this is a behavioural guard living in the
  > state suite because only that target compiles the wrapper sources (ADR-0008), and the DSP core
  > cannot see the APVTS or the MacroEngine (ADR-0001). Moving it to `dsp_tests.cpp` does not compile.
  > Same for `testModeSwitchIsSoundNeutral` under invariant 2.

- **Invariant 4's lookahead bar is re-grounded.** Its clause derived the bar from the latency clause
  ("…change reported latency — **so** it must not switch the oversampling factor or the lookahead
  time"). ADR-0004 made a lookahead change move no reported figure, so that derivation lapsed while
  the bar itself must stand. Prescribed replacement:

  > - change reported latency (`DSP_POLICY.md` invariant 2) — so it must not switch the oversampling
  >   factor. **It must not move the lookahead either**, and since ADR-0004 that bar no longer follows
  >   from the latency clause (a lookahead change moves no reported figure): it stands on its own
  >   ground — the engaged lookahead is a read offset into a live delay line, so slewing it drags the
  >   tap through the buffer, and adaptation is barred from time-varying delays for the same reason it
  >   is barred from the oversampling factor,

  Invariants 5 and 6 are also rewritten from "open decision" wording to the settled rules this ADR
  establishes (coexistence strategy; macro non-automatability) — those are this ADR's own decision
  text, reproduced in the policy rather than prescribed separately.

- **`PARAMETER_COMPATIBILITY_POLICY.md` rule 7's rationale is corrected** *(added 2026-07-31, same
  day)*. Invariant 6 was re-grounded from automation to recall, but rule 7 — a policy at the **same**
  authority level (`SOURCE_OF_TRUTH.md` level 4, so the corrected record does not outrank the
  uncorrected one) — still justified the macro-curve freeze by "changing the mapping curve changes
  what a recorded automation lane sounds like". Under this ADR the macros are non-automatable, so
  that scenario cannot occur in this product, and a maintainer who noticed could conclude the freeze
  does not apply and wave a post-release curve change through. **The obligation does not change**;
  only its reason does. Rule 7's second sentence onward is replaced by:

  > **Macro-curve changes after the first shipped build require an ADR.**
  >
  > **The reason is recall, not automation** (ADR-0005, 2026-07-31). This rule was written for a
  > host-visible, automatable macro; under ADR-0005/ADR-0010 the macros are **non-automatable**, so
  > no macro automation lane can exist, and a lane on a *managed* parameter writes that parameter
  > directly without ever consulting the mapping — changing a curve does **not** change how such a
  > lane sounds. What a curve change breaks is **recall**: every saved session and preset stores a
  > macro *position*, and the next macro gesture maps that stored position through the *new* curve,
  > so the same patch produces a different sound and a user's saved master no longer reloads as they
  > left it. That is a `COMPATIBILITY_POLICY.md` violation on its own terms. The obligation is
  > unchanged and applies in full — only its justification is corrected, so that a maintainer who
  > notices the automation argument does not hold cannot conclude the freeze does not apply.
  > `MODE_AND_ADAPTATION_POLICY.md` invariant 6 states the same thing at greater length.

  Rule 7's opening sentence ("The macro layer is part of the parameter surface…") is unchanged
  except for ending at "is itself semantic", the clause that carried the falsified reason having
  moved into the block above.

## Consequences

- **One value model.** The Advanced view always shows the numbers the DSP is using; a host
  automating a managed parameter and the Simple knob write to the same place. Invariant 1 holds
  structurally rather than by discipline.
- **The knob is testable.** Because `M` is pure, the sound at a given macro position is
  history-independent: `testMacroDefaultIsFixedPoint` plus a curve-evaluation test fully describe
  Simple mode. Carry-over offsets would have made this untestable.
- **A macro gesture is a burst of parameter writes (seven for a `loudness` drag, two for `tone`, one for `character` — the managed set splits by driver, it is not nine every time).** They appear in the host's parameter history and in
  the plugin's undo as one coalesced step per knob drag. Users who want a single automatable
  "performance lane" do not get one in v1; that is the price of option C's rejection, and reversing
  it is a kVersion bump + a superseding ADR.
- **Accepted residue — off-curve but engaged.** Host automation writes managed parameters
  ungesture-d, so it never sets a detach bit. A preset saved during such a session stores off-curve
  values with a clear mask, and the Simple view briefly describes a sound the macro does not
  produce. This is accepted rather than patched: making ungesture-d writes detach would mean
  automation playback silently detaching parameters, which is worse. The consequence is bounded —
  the next macro gesture re-engages them.
- **Factory presets are an authoring constraint, not a fact.** A factory patch ships an all-clear
  mask **wherever it is reachable from a single `(loudness, character, tone)` triple**; where it is
  not, the mask records the off-curve parameters, and that preset is correct rather than defective.
  Unreachable patches are not hypothetical: the §5.5 curves couple `colourDepth` and `limGain`
  through `l`, so "Tape Glue" (heavy colour, gentle limiting) cannot be produced by any single
  triple and ships a non-clear mask. P6 preset authoring checks every patch against the frozen
  curves and records which ones needed a mask, rather than assuming curve-consistency.
- **The state unit widens.** `{params, presetName, baseline, frozenTrims, detachMask}` is the A/B
  slot *and* the undo step; anything per-slot must be per-undo-step, or undoing a manual edit
  restores the value and strands its detach bit. This ADR depends on ADR-0007 for the schema home
  of the mask and the frozen trim vector.
- **Forecloses:** carry-over offsets, internal derived values, and automatable macros for v1 — each
  reversible only through a superseding ADR (and, for the macro flag, a kVersion bump).
- **Open at P4:** the §5.5 curve table is a *draft* tuning starting point, tuned by ear against the
  Master Plan benchmark and frozen before v0.1.0. Every revision before the freeze is recorded in
  the worklog, and the frozen curves become part of this ADR's acceptance evidence.

## Related code

None yet — P1 onward. Planned: `src/MacroEngine.{h,cpp}` (the mapper, re-entrancy flag, detach
listener), `src/PluginParameters.{h,cpp}` (`pid::` macro IDs, `withAutomatable(false)`,
`toEngine()`), `src/PluginProcessor.{h,cpp}` (gesture-bracketed undo coalescing, A/B and undo slot
unit), `src/PresetManager.{h,cpp}` (mask-carrying preset apply), `src/dsp/AdaptiveEngine.{h,cpp}`
(bounded trims, Freeze, Learn), `src/dsp/EngineParameters.h` (POD snapshot), and
`src/gui/PluginEditor.{h,cpp}` (Simple/Advanced views, detach badges, adaptive delta overlay) — per
`docs/DESIGN.md` §1.3.

Evidence [Unverified]:
- Design: `docs/DESIGN.md` §5.1 (invariant frame), §5.2 (mapper + discriminator), §5.3 (OQ-004
  coexistence, detach mask, preset semantics), §5.4 (adaptive trims, invariant 1/6 reconciliation,
  Freeze/Learn), §5.5 (managed set, draft curves, fixed-point rule); §2.4 (audibility rule,
  `colourModel` default `Tape`); §4.2 rows 3–5, 23, 48, 49 and footnote ²; §4.4 and §7 (per-slot
  mask + widened state unit); §10 row 0005.
- Research: `worklogs/2026-07-30-p0-anamorph-research.md`
- Policy: `MODE_AND_ADAPTATION_POLICY.md` invariants 1–6; `PARAMETER_COMPATIBILITY_POLICY.md`
  rule 5 (`withAutomatable(false)` is advisory).
- Tests (planned, P4): `testMacroDefaultIsFixedPoint`, `testModeSwitchIsSoundNeutral`, Freeze
  bit-repeatability.
- Anamorph precedent [Verified] — read during the P0 research pass against commit `b6a3db8`:
  - `Anamorph:src/PluginParameters.cpp:326-389` — Advanced-mode gating at snapshot time; the
    anti-precedent that makes this ADR necessary.
  - `Anamorph:src/PluginProcessor.cpp:338-489` — host automation folds into the baseline instead of
    counting as a user edit.
  - `Anamorph:src/PluginProcessor.cpp:553-559` — `abMatchGain` sentinel-atomic inject pattern, the
    **starting point** for the per-slot frozen trim vector: it carries one float where the vector is
    four scalars, so the transport is OQ-013, not a settled reuse.
  - `Anamorph:src/PluginProcessor.cpp:178-202` and `:33-38,402-421` — single-step gesture/undo
    bracketing of a multi-target commit, reused for Learn.
  - `Anamorph:src/dsp/AnamorphEngine.cpp:58-81` — the 20 ms standard parameter-smoothing constant
    that makes the macro glide click-free.
  - `Anamorph:src/PluginParameters.cpp:274-281` — KI-003, host-automated view toggles drive editor
    resizes that crash X11 hosts; the precedent behind the non-automatable posture for view and
    macro controls.

All Anabasis runtime claims above are **Unverified**: no `src/` exists at P0, and every statement
here is the contract the P1–P4 code must satisfy. No number in this ADR is a measurement (C2).
