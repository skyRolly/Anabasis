# PARAMETER_COMPATIBILITY_POLICY.md

Subset of `COMPATIBILITY_POLICY.md`. Governs the host parameter surface and automation. Ledger:
`docs/architecture/PARAMETER_REGISTRY.md` (written at P1).

## Rules

1. **Parameter IDs are immutable.** The string `id` in `ParameterID { id, kVersion }` is the
   persistent contract. No rename, no removal without an ADR + migration.
2. **Display names may change.** Renaming the user-facing name is allowed while the ID stays
   fixed. Update `PARAMETER_REGISTRY.md` + `CHANGELOG.md` (a rename is a `Changed` entry, never a
   `Removed` one).
3. **Ranges, defaults, and choice orderings of host-visible params are semantic.** Changing them
   in a way that alters recall or automation playback requires an ADR. This is sharper for a
   maximizer than for most plugins: a Ceiling or Threshold range change silently re-scales every
   saved session's normalised value.
4. **`kVersion` is bumped only on a deliberate parameter-set change**, so hosts re-scan automation.
5. **Automation-flag changes** (`withAutomatable`) are allowed but must be recorded — note that
   `withAutomatable(false)` does **not** hide a parameter in all hosts (REAPER shows it anyway);
   true hiding means moving it out of the APVTS into host-hidden session state.
6. **Exclusion lists are part of the contract.** Which parameters are excluded from A/B compare,
   undo, and preset recall — and which are **lockable** (§9 requires Ceiling to be lockable at
   minimum) — affect recall behaviour, so changing them requires an ADR.
7. **The macro layer is part of the parameter surface.** If the Simple-mode Loudness/Push knob is
   host-visible, it is a parameter with all the obligations above, *and* its mapping onto the
   Advanced parameters is itself semantic. **Macro-curve changes after the first shipped build
   require an ADR.**

   **The reason is recall, not automation** (ADR-0005, 2026-07-31). This rule was written for a
   host-visible, automatable macro; under ADR-0005/ADR-0010 the macros are **non-automatable**, so
   no macro automation lane can exist, and a lane on a *managed* parameter writes that parameter
   directly without ever consulting the mapping — changing a curve does **not** change how such a
   lane sounds. What a curve change breaks is **recall**: every saved session and preset stores a
   macro *position*, and the next macro gesture maps that stored position through the *new* curve,
   so the same patch produces a different sound and a user's saved master no longer reloads as they
   left it. That is a `COMPATIBILITY_POLICY.md` violation on its own terms. The obligation is
   unchanged and applies in full — only its justification is corrected, so that a maintainer who
   notices the automation argument does not hold cannot conclude the freeze does not apply.
   `MODE_AND_ADAPTATION_POLICY.md` invariant 6 states the same thing at greater length.

## Getting it right the first time (pre-0.1.0)

No parameter has shipped, so none of the above has bitten yet. The cheapest possible moment to
settle the surface is **now**:

- Settle the complete parameter table in `DESIGN.md` before writing `createAnabasisLayout`
  (`DEVELOPMENT_BRIEF.md` §24).
- Prefer **generous ranges chosen once** over ranges that "look right today" — widening a range
  later re-scales saved normalised values.
- Prefer a **stable ID vocabulary** decoupled from the display wording (`pid::ceiling`, not
  `pid::outputCeilingDbTp`), so display copy can be revised freely under C8.
- Freeze `tests/fixtures/parameter_registry.snapshot` as soon as the layout exists — the snapshot
  test is what makes rule 1 automatic rather than aspirational.

## Required verification before release

- `[ ] Parameter IDs unchanged` (diff the registry / the snapshot test is green).
- `[ ] Automation playback verified`.

## Enforcement

A Parameter Registry change is an **Architecture Review Gate** item and an **AI Agent Hard Stop**.
Changing this policy requires an ADR.
