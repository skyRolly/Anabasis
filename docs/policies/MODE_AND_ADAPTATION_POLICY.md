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
return to Simple.

**Settled by ADR-0005** (Accepted 2026-07-31; OQ-004 `Resolved`) — **macro-latch with re-engage on
touch**, which is binding, not a suggestion:

1. Returning to Simple moves nothing (invariant 2 holds by construction — there is no value path
   at the switch).
2. Each manually edited managed parameter is **detached** from the macro, tracked by a
   per-parameter detach mask that is **per-A/B-slot** state and travels with presets (ADR-0007).
3. The **next macro gesture re-engages every detached parameter** through the normal rate-limited
   glide. That gesture *is* the "clear notice" the brief asks for — the user is explicitly choosing
   the macro over their edits.
4. A "reset to macro" affordance re-engages without moving the macro position.

Carry-over offsets were **rejected**: they make the knob's sound at a given position
history-dependent, so the mapping stops being a pure function and stops being testable.

Two mechanisms this invariant depends on, both binding: a change counts as a *manual edit* only if
it is **not macro-originated** (the MacroEngine raises a message-thread re-entrancy flag around its
own write burst) **and** is **gesture-bracketed** — so automation playback, preset apply, A/B and
undo never detach anything. And a preset apply must satisfy both conditions, or a preset holding
off-curve managed values could not be recalled faithfully at all.

### 6. Automation and recall see the real parameters

Because Simple is a macro layer, host automation, preset recall and A/B compare operate on the
underlying Advanced parameters.

**Settled by ADR-0005 and ADR-0010** (Accepted 2026-07-31): the three macro controls (`loudness`,
`character`, `tone`) are real, host-visible APVTS parameters but are **not automatable**. The
automation surface is the *managed Advanced parameters* themselves. A recorded macro lane
therefore cannot exist, which removes the question of how it would interact with per-parameter
lanes.

Two consequences that are part of the contract, not implementation detail:

- `withAutomatable(false)` is **advisory** — some hosts expose the parameter regardless
  (`PARAMETER_COMPATIBILITY_POLICY.md` rule 5). The MacroEngine therefore consumes macro changes
  only through an async **message-thread** listener; a host that automates a macro anyway gets the
  mapping applied at message-thread rate, and offline-render determinism is explicitly not promised
  for that unsupported usage.
- Making a macro automatable later is a parameter-surface change: `kVersion` bump + ADR
  (`PARAMETER_COMPATIBILITY_POLICY.md` rules 4–5).
- The macro **mapping curves are semantic from the first shipped build** — but be precise about
  why, because the obvious argument is wrong under this architecture. A recorded automation lane
  on a managed parameter (say `limGain`) writes that parameter **directly** on playback and never
  consults `M`, so changing a curve does **not** change how that lane sounds. What a curve change
  *does* break is **recall**: every saved session and preset stores a macro position, and the next
  macro gesture maps that stored position through the *new* curve — so the same patch produces a
  different sound, and a user's saved master no longer reloads as they left it. That is a
  `COMPATIBILITY_POLICY.md` violation on its own terms, independent of
  `PARAMETER_COMPATIBILITY_POLICY.md` rule 7's automation framing (which is written for the
  host-visible/automatable macro this product does not have). A post-release curve change is
  therefore an **Architecture Review Gate** item and needs an ADR.

Recorded in `PARAMETER_REGISTRY.md` when it is written at P1; `DESIGN.md` §4.2 is the interim table
of record.

## Current implementation

**TODO (no code yet).** Populated at P4 with evidence citations.

## Enforcement

- Any change to the macro-layer contract, the mode-switch neutrality guarantee, or the adaptation
  rate/Freeze semantics is an **Architecture Review Gate** item and an **AI Agent Hard Stop**.
- Changing this policy requires an ADR.
