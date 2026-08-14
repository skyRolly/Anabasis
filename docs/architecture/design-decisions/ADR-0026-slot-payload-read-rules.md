# ADR-0026 — A `SLOT` without its parameter payload resolves to defaults, and metadata travels only with the parameters it describes

> **✅ RATIFIED — THE ARCHITECTURE REVIEW GATE IS CLEARED (2026-08-14).** The owner approved this
> decision. How it arrived stays in the record, because the record of a decision includes how it was
> nearly taken without one: the change was implemented and merged onto the 0.1.4 branch **without
> being flagged as gated**, and review caught it on 2026-08-13. Under `ARCHITECTURE_REVIEW_GATE.md`
> a "Serialization Registry change — any field add/remove/**semantic change**" must not merge on a
> green build, and `SESSION_COMPATIBILITY_POLICY.md` rule 1 says a field may not "have its meaning
> changed without an ADR + migration". The round had flagged only the ADR-0021 installer amendment
> and added ADR-0025 for the testing-policy exception; neither covered this.
>
> What the owner decided is the read rule below. The "Alternatives" section keeps its measured
> table — three states, each executed rather than predicted — because a ratified decision is worth
> more when the options it beat are on the record beside it.

**Status:** **Accepted — 2026-08-14**, on the owner's explicit approval of this record. It was NOT
covered by the standing blanket approval for the post-v0.1.0 rounds: that approval is what let
implementation proceed, and a gated change is precisely the class a green build does not clear, so
it needed a decision of its own and now has one.

## Context

Two `SLOT`-reading rules changed in 0.1.4, at `src/PluginProcessor.cpp:1802` (the stored slot) and
`src/PluginProcessor.cpp:1821` (the active slot). Both were made to close a real defect, pinned by
`testAMalformedStoredSlotCannotSplitSoundFromMetadata` and
`testARootlessSurfaceDropsTheActiveSlotsMetadataToo`, and both alter how a stored session is
INTERPRETED — which is the definition of a semantic change to the registry.

The defect. `applySlotToLive` adopts parameters only when the slot's payload is valid, but adopts
`presetName`, the ADR-0022 identity trio, `BASELINE`, `FROZEN_TRIMS` and `DETACH_MASK`
**unconditionally**. A `SLOT` node can be a perfectly valid `ValueTree` and still carry no
`ANABASIS` child — hand-edited, truncated, or a foreign tree that happens to use the type name.
Taken as it stood, such a slot put one session's preset NAME and identity on another session's
SOUND: the A/B switch would show "Ghost Session" over whatever the previous project left in
`storedSlot`, which is a processor member that survives across restores.

## Decision

1. **A stored `SLOT` carrying no `ANABASIS` child is declined outright**, keeping the `defaultSlot`
   that `resetSlotFieldsToDefaults()` planted. The whole slot resolves to defaults; no half of it
   is adopted.
2. **The ACTIVE slot's metadata is adopted only when the ROOT surface was restored.** The active
   slot needs no payload test for its SOUND — that comes from the root `ANABASIS`, not from the
   slot's redundant copy — so the gate is placed on the thing that actually decides whether the
   labels describe the installed surface.

One rule, stated per slot: **metadata is adopted only alongside the parameters it describes.**

## Consequences

- **A blob with a valid `AB` block under a root with no `ANABASIS` now loads with the DEFAULT
  name, identity, baseline, frozen trims and an EMPTY detach mask**, where it previously adopted
  all five.
- **The §5.3 detach mask is the case that must be looked at rather than waved through.**
  Detachment is not otherwise recoverable, so dropping it loses user intent, and it is dropped
  **silently** — there is no load-diagnostics channel to report it on. It is dropped anyway
  because the alternative loses more: a mask names parameters detached from the MACRO SURFACE, and
  the surface this restore installed came from defaults, so a kept mask would describe detachments
  from a mapping the session never had.
- **The exposure is bounded by there being no producer.** `getStateInformation` always emits the
  root `ANABASIS` child and always writes a payload into both slots, so no blob this plug-in has
  ever written takes either path. Reaching them needs a hand-edited, truncated or foreign blob.
  That bounds the compatibility risk to approximately nothing — **and does not clear the gate**,
  because the gate is about the class of change, not the size of its blast radius.
- **The two slots are governed by two different tests, on purpose.** A valid root whose ACTIVE slot
  has lost its payload still adopts that slot's metadata, since the sound came from the root and
  both halves came out of the same blob. `SERIALIZATION_REGISTRY.md` §2 states this asymmetry
  explicitly so it is not read as an oversight.

## Migration

None is owed, and the reason is the pre-ship window rather than a read path: no build carrying
either behaviour has left this repository (`COMPATIBILITY_POLICY.md` §"When the contract starts"),
and no blob this plug-in writes reaches either rule. The §4.4 defaults-first read rules ARE the
migration for anything that does, which is the same mechanism ADR-0015 used.

`SESSION_COMPATIBILITY_POLICY.md` rule 4 (a save → load round-trip reproduces the sound, name,
identity, dirty marker, both slots, the active slot and any locks) is untouched for every blob the
plug-in produces, which is what that rule is about.

## Alternatives, and exactly what reverting costs

**Each option below was EXECUTED and MEASURED (2026-08-13, re-measured against the current suite), not reasoned about**, so that the
decision in front of the owner is a choice between three known states rather than three
predictions. Each was applied to the tree, built, and run against the full state suite:

**The check TOTAL below is 839 because that is what the suite held on 2026-08-13, the day these
three states were measured**, and this table is deliberately NOT bumped as the suite grows:
re-writing a figure that was not re-measured turns a record of an experiment into a claim about
today. What the decision rests on is the FAILURE column, which is a property of these two
expressions and did not move. Re-run the three options if a fresh total is needed.

Today's total is deliberately NOT restated here either. This paragraph carried it once — as a
parenthetical, kept current by hand — and it went stale within the day, on the round that added
this ADR's own guard. `HANDOVER.md`'s Test Status row is the count's home, for the same reason
`CLAUDE.md` quotes no pluginval strictness: a second copy of a moving number is a second thing to
get wrong, and a dated measurement beside an undated one invites exactly the "these cannot both be
a re-count" reading it just drew.

| Option | Result | What the suite says |
|---|---|---|
| Keep both (the tree as it stands) | 839 checks, **0 failures** | — |
| Revert BOTH decisions | 839 checks, **5 failures** | 3 × `malformedSlot` (including `a payload-less slot lent its preset name to another state ('Ghost Session')`) + 2 × `rootlessActive` |
| Keep decision 1, revert decision 2 | 839 checks, **2 failures** | 2 × `rootlessActive` only — the `malformedSlot` trio passes, confirming the two decisions are independently revertible and that decision 1 alone closes the reported defect |

The failing assertion names are the specification of what each option gives up. Nothing else in
either suite moves, which is the other half of the measurement: neither decision is load-bearing for
anything outside the read rules it states.

- **Revert both gates.** Two expressions carry the decision:
  `stored.getChildWithName ("ANABASIS").isValid()` at `src/PluginProcessor.cpp:1802`, and
  `live.isValid() && liveSurfaceRestored` at `src/PluginProcessor.cpp:1821`. Removing them restores
  the pre-0.1.4 reading and re-opens the split-sound-from-metadata defect;
  `testAMalformedStoredSlotCannotSplitSoundFromMetadata` and
  `testARootlessSurfaceDropsTheActiveSlotsMetadataToo` are the two tests that would then fail, and
  the mutant for the first reproduces the defect verbatim (`'Ghost Session'` on another state's
  sound).
- **Keep decision 1, revert decision 2.** Coherent: decision 1 closes the reported defect, and
  decision 2 is the generalisation of the same principle to the other slot. The cost is that the
  rule stops being one rule — the stored slot would gate on payload and the active slot would gate
  on nothing — and `SERIALIZATION_REGISTRY.md` would have to describe two unrelated behaviours
  instead of one principle.
- **Adopt metadata but mark the session damaged.** Rejected for now only because there is no
  load-diagnostics channel to mark it ON; if one is ever added, this is the first case that
  deserves it, and this ADR should be revisited then.

## Verification

- `testAMalformedStoredSlotCannotSplitSoundFromMetadata` — the malformed-slot mutant reproduces the
  defect verbatim.
- `testARootlessSurfaceDropsTheActiveSlotsMetadataToo` — the detach mask, name and surface together;
  the test states why it stops at those three.

## Related code
- `src/PluginProcessor.cpp:1802` (the stored-slot guard)
- `src/PluginProcessor.cpp:1821` (the active-slot metadata gate)
- `src/PluginProcessor.cpp:1758` (the `liveSurfaceRestored` flag both read)

Evidence [Verified]:
- Source: `src/PluginProcessor.cpp:1802`, `src/PluginProcessor.cpp:1821`
- Test:   `testAMalformedStoredSlotCannotSplitSoundFromMetadata`,
  `testARootlessSurfaceDropsTheActiveSlotsMetadataToo`
