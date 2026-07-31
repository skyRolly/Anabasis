# ADR-0007 — State schema v1: explicit schemaVersion, raw-exact sessions, per-slot adaptive state

**Status:** Accepted (2026-07-31 — owner sign-off on `docs/DESIGN.md`)

## Context
`ADR_POLICY.md` makes state serialization ADR-mandatory, and `COMPATIBILITY_POLICY.md` freezes every
field the moment it ships — the schema must therefore be settled before the first `setStateInformation`
is written, not discovered during P1. `DESIGN.md` §7 copies the Anamorph state machinery wholesale
(StateSet A/B + undo, `PresetManager`, raw-exact round-trip), but Anabasis adds two pieces of state
Anamorph has no equivalent for: the adaptive engine's **frozen trim vector** (§5.4) and the macro
layer's **detach mask** (§5.3). Both are produced per A/B slot and both carry obligations —
`MODE_AND_ADAPTATION_POLICY.md` invariant 3 (Freeze is bit-repeatable) and the §5.3 detach/re-engage
rule — that are unimplementable unless the schema gives them a home in the right place.

## Problem
Three questions, none of them obvious:

1. **Versioning.** Anamorph carries no version number and detects format generations *structurally*
   in `setStateInformation` (root tag, missing child, legacy slot keys, `raw`-vs-`value` fallback)
   — `Anamorph:src/PluginProcessor.cpp:535-613` [Verified]. It works, and it is a shipped product's
   accumulated evidence. Does a greenfield repository copy that, or write a version field it does
   not yet need?
2. **Placement of the new adaptive/macro state.** The trim vector and the detach mask look global —
   there is one adaptive engine and one macro layer — so the cheap schema puts them beside the
   learned targets in a single global child.
3. **The unit of a slot.** Anamorph's StateSet is `{params, presetName, baseline}` and is both the
   A/B slot and the undo entry. Two new per-slot fields make it unclear whether they belong in that
   unit or beside it.

## Options
- **A. No version field; structural detection only (Anamorph's design).** Proven, and the read rules
  are needed regardless. Lost because the structural rules are *recovery* heuristics retrofitted to
  files that were written without a label; a greenfield writer can label its output for free, and
  the first genuine migration then has an explicit discriminator instead of another shape probe.
- **B. Explicit `schemaVersion`, reads gated on it (reject unknown/newer, migrate on exact match).**
  Lost: it trades tolerance for tidiness. A forward or hand-edited blob must leave state untouched
  rather than fail loudly, and a version gate would reject files whose *shape* is perfectly readable
  — throwing away the property Anamorph's read path actually earns.
- **C. Explicit `schemaVersion` integer = 1 written from day one, **plus** the structural-tolerance
  read rules (unknown fields ignored, missing fields defaulted, indices clamped at the boundary).
  Chosen.** The label costs one property; the tolerance is what keeps old sessions loadable.
- **D. Trim vector and detach mask in the global `ADAPTIVE` child, beside the learned targets.**
  Lost, and it is the option worth recording. One global trim vector means slot A frozen with trims
  Tₐ and slot B frozen with T_b share one storage location: an A/B switch restores the wrong latched
  trims and a frozen slot stops being bit-repeatable — exactly the property
  `MODE_AND_ADAPTATION_POLICY.md` invariant 3 requires. One global mask means slot A's detached
  `clipDrive` describes slot B, so the next macro gesture re-engages parameters the user never
  detached *here* and leaves detached ones they did.
- **E. Halfway: per-slot trims (the audible one), global mask (merely a badge).** Lost. The mask is
  not merely a badge: it is what the Simple view claims about the sound and what "reset to macro"
  acts on (§5.3). A global mask is wrong about one of the two slots at all times.
- **F. Keep the narrow StateSet `{params, presetName, baseline}`; store trims and mask per slot but
  outside the undo entry.** Lost. Undo is a per-slot stack, so anything per-slot must also be
  per-undo-step: undoing a manual edit would restore the value and strand its detach bit (the
  parameter returns to its curve value yet stays badged *edited* and out of the macro), and undoing
  a `freeze` toggle would restore the parameter without the trim vector it latched.
- **G. Presets exclude the detach mask, cleared on load** (the earlier position, reversed by §5.3).
  Lost. It does not change any value trajectory either way — rule 3 re-engages on the next gesture
  and preset apply lands stored values exactly as saved — but without the mask a preset saved after
  manual edits reloads with off-curve values silently marked *engaged*, so the Simple view claims
  the macro describes a sound it does not, and the reset affordance has nothing to reset.
- **H. Raw-exact presets (drop the snapped-value contract, carry `raw` in `.anabasis` too).** Lost:
  snap-equivalence is only observable for discrete/stepped parameters, and raw-exactness exists to
  satisfy the host-session restoration contract, not file interchange. Two explicitly different
  fidelity contracts, both tested, is the inherited design and nothing in Anabasis's parameter
  surface argues against it.

## Decision
**Schema v1.** Root `AnabasisRoot`, property `schemaVersion` (int) = **1**, written by every build.
Read rules are structural and tolerant: unknown properties/children ignored, missing fields taken at
their default, indices clamped at the read boundary, a foreign root tag or an undecodable blob leaves
state untouched. A `schemaVersion` greater than 1 is **not** a rejection reason — the reader falls
back to shape. Remaining root properties follow the copied machinery (live preset name + dirty-star
baseline, `Anamorph:src/PluginProcessor.cpp:535-613`).

Children:

| Child | Contents | Scope |
|---|---|---|
| `ANABASIS` | APVTS tree; each `PARAM` keeps APVTS's denormalised `value` **plus** an additive exact `raw` attribute; restore prefers `raw`, falls back to `value` (Anamorph ADR-0013) | session, raw-exact |
| `ANABASIS_INTERNAL` | host-hidden session state (§4.3); never in A/B, undo or presets (Anamorph ADR-0010) | session |
| `AB` | `active` index (clamped) + per slot the full StateSet (below) | per slot |
| `ADAPTIVE` | **only** the learned reference targets (§5.4). Absent = never learned | global |

**The `AB` child holds, per slot: parameter tree, preset name, baseline, frozen trim vector, and
macro detach mask.** The last two are per-slot state, not global. `ADAPTIVE` holds neither.

**StateSet is widened to `{params, presetName, baseline, frozenTrims, detachMask}`** and remains
**both** the A/B-slot unit and the undo unit. Every path that copies a slot — A/B switch,
copy-to-other, undo push/pop, session restore — carries all five fields or none.

Restore routing differs by consumer, and this is part of the decision:
- `params` restore through `replaceState` + a synchronous re-assert that prefers `raw`.
- `frozenTrims` is **audio state**: it restores through the engine-side inject-at-the-duck-bottom
  path, a sentinel-valued atomic consumed at the forced duck's silent bottom
  (`Anamorph:src/PluginProcessor.cpp:485-491` [Verified], the `abMatchGain` pattern).
- `detachMask` restores **on the message thread** with the rest of the slot: its only consumer is
  the MacroEngine (§5.2) and nothing on the audio thread reads it.

**Presets.** `.anabasis` XML in `<userAppData>/RollyTech/Anabasis/Presets`; snapped denormalised
values only (no `raw` attribute) — snap-equivalence is the preset contract, raw-exactness is the
host-session contract; **plus the detach mask**. A preset file with no mask reads as all-clear, by
the same missing-fields-default rule as the session path; factory presets ship an all-clear mask and
are compiled-in override tables. Presets carry no frozen trims, no `ANABASIS_INTERNAL`, no A/B
structure — §4.4 enumerates preset content and the trim vector is not in it.

**Not serialized:** the per-slot undo/redo stacks (cap 128, cleared on every session restore).

## Consequences
- Freeze is bit-repeatable *per slot*: a session reload and a switch back to a frozen slot both
  reproduce that slot's latched trims exactly (`MODE_AND_ADAPTATION_POLICY.md` inv 3 becomes
  implementable rather than aspirational).
- The Simple view's *edited* badge and the "reset to macro" affordance describe the slot the user is
  looking at, and survive undo of the edit that created them.
- Undo entries grow by a trim vector and a mask each; with a 128-entry cap per slot that is the
  accepted cost of making undo consistent with A/B.
- Every field above is frozen on the first shipped build (`COMPATIBILITY_POLICY.md`): additions must
  tolerate absence, removal is prohibited, deprecation requires a retained read path. A later change
  is a **Hard Stop** under `ARCHITECTURE_REVIEW_GATE.md`, not a refactor.
- `schemaVersion` is a label, not a gate. Writing it forecloses nothing; *reading* against it would
  have, which is why option B is rejected explicitly — a future contributor adding a version check
  to the read path is reversing this ADR.
- Forecloses: a single global adaptive-state blob, a global detach mask, raw-exact preset files, and
  a narrow three-field StateSet. Each would have to be reopened by a superseding ADR.
- Test obligations, reproduced from day one (`DEVELOPMENT_BRIEF.md` §20.3, `DESIGN.md` §4.4):
  parameter-registry snapshot behind a `--write-snapshot` gate, byte-identical `getState → setState
  → getState` round-trip with every field off-default and the two slots made genuinely to differ,
  corrupt/truncated/foreign-blob robustness plus index clamping, and preset round-trip in the real
  user preset folder. The round-trip fixture must include a frozen slot and a non-clear mask, or the
  two fields this ADR adds are exercised vacuously.

## Related code
None yet — P1 onward. Planned: `src/PluginProcessor.{h,cpp}` (schema read/write, A/B, undo,
StateSet), `src/InternalState.h` (`ANABASIS_INTERNAL`), `src/AbSlotIndex.h` (slot clamp),
`src/PresetManager.{h,cpp}` (`.anabasis`, lock-aware apply, factory override tables),
`src/PluginParameters.{h,cpp}` (`pid::` IDs, exclusion tiers), `src/MacroEngine.{h,cpp}` (detach-mask
consumer), `src/dsp/AdaptiveEngine.{h,cpp}` (frozen trim vector, learned targets).

Evidence [Unverified] — no `src/` exists; every Anabasis runtime claim above is the contract the code
must satisfy, not an observation (C2: no number here is a measurement):
- Design: `docs/DESIGN.md` §4.4 (schema v1), §5.3 (presets carry the mask; per-slot mask), §7
  (StateSet widening, A/B + undo unit), §4.3 (`ANABASIS_INTERNAL` inventory), §5.4 (frozen trims per
  slot, learned targets global), §1.3 (planned module inventory), §10 row 0007.
- Research: `worklogs/2026-07-30-p0-anamorph-research.md`
- Anamorph precedent [Verified]: `Anamorph:src/PluginProcessor.cpp:535-613` (structural generation
  detection, no version field, `raw`-then-`value` fallback); `Anamorph:src/PluginProcessor.cpp:485-491`
  (sentinel-atomic inject consumed at the duck's silent bottom); `Anamorph:src/InternalState.h:10-29`
  (host-hidden state rationale); Anamorph ADR-0013 (additive exact `raw` attribute); Anamorph ADR-0010
  (host-hidden `InternalState`); Anamorph ADR-0008 (custom per-A/B-slot undo).
