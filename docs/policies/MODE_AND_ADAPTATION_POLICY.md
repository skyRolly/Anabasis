# MODE_AND_ADAPTATION_POLICY.md

**Priority: 4.** System Policy — **Anabasis-specific** (Anamorph has no equivalent). Governs the
Simple/Advanced relationship and the adaptive engine.

Derived from `docs/DEVELOPMENT_BRIEF.md` §5, which names §5.3 a *key architectural constraint*.

## Invariants (binding)

### 1. One parameter model

Simple mode is a **macro layer on top of the Advanced parameters**. There is exactly one
parameter model; Simple does not own a private parameter set, a shadow state, or a second code
path through the DSP. Everything the primary Loudness/Push knob does is expressible as
Advanced-parameter values.

*Consequence:* a host automating an Advanced parameter and a user turning the Simple knob are
writing to the same place, and the DSP cannot tell them apart.

Guarded by: `testMacroDefaultIsFixedPoint` — for every managed parameter, the mapping evaluated at
the default macro position must equal that parameter's declared default, or the first macro gesture
jumps the factory patch instead of gliding from it (ADR-0005).

### 2. Switching modes must not change the sound

At the instant the user switches Simple ⇄ Advanced, the rendered output is unchanged. Not
"approximately", not "after a smoothing window" — the parameter values in force before and after
the switch are identical, and the switch itself is click-free (`DSP_POLICY.md` invariant 8).

A mode switch is a **view** change. Any implementation in which it is also a *value* change is a
defect, and any deliberate change to this contract is an **Architecture Review Gate** item and an
**AI Agent Hard Stop**.

Guarded by: `testModeSwitchIsSoundNeutral` — render across a switch in both directions, at
multiple macro positions and after manual Advanced edits; output must be sample-identical.

### 3. Adaptation is slow, smooth, and inaudible as modulation

The adaptive engine (§5.2) reacts on **second-scale time constants with hysteresis**. There must
be **no audible modulation** attributable to adaptation — no pumping, no breathing, no sweeping
tonality on static material.

- Adaptation state changes are rate-limited, not stepped.
- A **Freeze** control locks the current adaptive state; while frozen, the adaptive layer
  contributes a constant, and the plugin behaves as a static processor.
- `Learn` (§5.2) fixes the internal reference targets from an analysed passage. It is an explicit
  user action with an explicit end; it never runs silently in the background.

Guarded by: a static-input adaptation test (a steady programme must converge and then hold, with
the residual modulation below a stated bound) + a Freeze test (frozen ⇒ bit-identical repeats).

### 4. Adaptation never breaks a hard guarantee

The adaptive layer may move parameters within their declared ranges. It may **not**:

- exceed or bypass the ceiling clamp (`DSP_POLICY.md` invariant 4),
- change reported latency (`DSP_POLICY.md` invariant 2) — so it must not switch the oversampling
  factor. **It must not move the lookahead either**, and since ADR-0004 that bar no longer follows
  from the latency clause (a lookahead change moves no reported figure): it stands on its own
  ground — the engaged lookahead is a read offset into a live delay line, so slewing it drags the
  tap through the buffer, and adaptation is barred from time-varying delays for the same reason it
  is barred from the oversampling factor,
- alter the signal-chain order,
- write to a parameter the user has locked (§9 parameter lock; **Ceiling is lockable at minimum**).

### 5. Manual Advanced edits are user intent

If the user edits a parameter manually in Advanced, that edit is not silently discarded when they
return to Simple. The coexistence strategy — macro-takes-precedence with a clear notice, versus a
"carry over" option — is **an open decision** (`docs/OPEN_QUESTIONS.md` OQ-004) that must be
argued in `DESIGN.md` and recorded as an ADR **before** P4 implementation. Whatever is chosen must
still satisfy invariant 2.

### 6. Automation and recall see the real parameters

Because Simple is a macro layer, host automation, preset recall and A/B compare operate on the
underlying Advanced parameters. Whether the macro knob is itself host-visible and automatable —
and if so, how a recorded macro automation lane interacts with recorded per-parameter lanes — is
part of the parameter surface and therefore governed by `PARAMETER_COMPATIBILITY_POLICY.md`. Decide
it in `DESIGN.md` and record it in `PARAMETER_REGISTRY.md`; it is a contract from the first
shipped build.

## Current implementation

**TODO (no code yet).** Populated at P4 with evidence citations.

## Enforcement

- Any change to the macro-layer contract, the mode-switch neutrality guarantee, or the adaptation
  rate/Freeze semantics is an **Architecture Review Gate** item and an **AI Agent Hard Stop**.
- Changing this policy requires an ADR.
