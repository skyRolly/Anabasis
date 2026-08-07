# ADR-0018 — Copy becomes an undo step that keeps the destination's history, and the Advanced toggle joins the undo history

**Status:** Accepted (2026-08-06 — owner directive of 2026-08-06, 0.1.1 round item 4: Copy must
be undoable and history-preserving, and the Advanced-mode toggle must be recorded in undo,
"per the sibling's implementation")

> **ARCHITECTURE REVIEW GATE — CLEARED (2026-08-06).** Two gate categories are touched:
> **conflict with an Accepted ADR** (ADR-0010 option E explicitly rejected an undoable Advanced
> toggle) and the **Simple/Advanced macro-layer contract** family (the toggle's tier membership
> moves). The clearance rests on the owner's 0.1.1 directive, which names both changes
> explicitly ("A/B Copy 应该……按照 Anamorph 的方式保留/可撤销……还有 Advanced Mode 的切换也应该
> 被记录在撤销里。研究 Anamorph 的实现。") and carries the standing instruction that anything
> needing human confirmation in this round is confirmed by the owner directly ("如果有需要人工
> 确认的，你就直接帮我确认或者签字就可以。如果需要开 ADR 的，直接就开 ADR"). The conflict with
> ADR-0010 is therefore resolved BY the owner, not around them; ADR-0010 option E is
> **partially superseded** by this record — the undo half only. Its A/B half stands (see
> §Decision 3).

## Context

Three behaviours shipped in 0.1.0 that the sibling product resolves differently, and the owner's
0.1.1 directive orders the sibling's answers adopted:

1. **Copy was not an undo step.** `copySlotToOther()` snapshotted the live state into the
   inactive slot and **cleared the destination's entire undo AND redo history**, on the argument
   that the cleared entries "describe states the slot no longer has … because the copy itself is
   not an undo step" — a self-fulfilling justification: making the copy a step makes those
   entries reachable again. The sibling (`AnamorphAudioProcessor::abCopyToOther`, its #12 rule)
   pushes the destination's pre-copy StateSet onto the destination's undo stack, keeps
   everything beneath it, and clears only the redo line.

2. **An Advanced-mode toggle was not (effectively) undoable.** `advancedMode` sat in the view
   tier, so `applySlotToLive` overwrote its entry from live on every restore path — no undo
   could move it. Worse, the toggle's `ButtonAttachment` click IS gesture-bracketed and the
   gesture path had no view-tier filter, so each click pushed a **dead step**: an entry whose
   restore changed nothing, eating one Undo press. The same dead-step defect applied to
   `bypass`, `loudnessComp` and `deltaMonitor` clicks.

3. In the sibling, `advancedMode` is a full non-view parameter: it travels with undo AND with
   A/B slots, excluded only from presets.

## Options

- **A. Full sibling parity: `advancedMode` travels with undo and with A/B.** Rejected — for the
  A/B half only. ADR-0010 option E's surviving argument is Anabasis-specific and still correct:
  the Anabasis mode switch is **sound-neutral by construction** (MODE_AND_ADAPTATION_POLICY
  invariants 1–2), so an A/B compare is a *sound* compare, and letting the compare resize the
  editor adds a visual discontinuity that tells the user nothing about the sound. The sibling's
  toggle is *not* sound-neutral, which is why its A/B travel is coherent there.
- **B. Undoable but A/B-pinned — split the two exclusions.** **Chosen.** The owner's directive
  names *undo* recording; the A/B-compare principle survives. One shared predicate cannot
  express "undoable but not A/B-travelling" (the ADR-0010 option D argument), so the pin moves
  out of the predicate into the one function that applies slots.
- **C. Keep 0.1.0's behaviour.** Contradicts the directive.

## Decision

1. **Copy** (`copySlotToOther`, body moved to the .cpp so it reaches `pushCapped`): the
   destination's pre-copy `{slot, presetBaseline}` is pushed onto the **destination's** undo
   stack through `pushCapped` (the one bounded push), its older entries are **kept**, and only
   its redo stack is cleared — and only when the Copy actually changes that slot (see the second
   amendment). The entry pairs the destination's own dirty datum
   (`storedPresetBaseline`), not the active slot's — `pushUndoStep` cannot be reused, it
   targets the active stack. Undoing on the copied-into slot reverts the Copy; further undos
   walk the pre-copy history, coherent because entries are absolute SLOT snapshots. The source
   slot's history is untouched. Still no duck: nothing audible changes at Copy time.

2. **`advancedMode` leaves `isViewTierParam`** and is added to `isPresetExcludedParam` **by
   name** (without this, browsing a factory preset would slam the editor back to Simple — the
   defaults-pass hazard that predicate's comment documents). It stays **non-automatable**
   (ADR-0010's X11 editor-resize crash path is about automation threads and is untouched).

3. **`applySlotToLive` grows an `adoptAdvanced` flag.** The undo/redo restore passes `true` —
   the ONE path that adopts `advancedMode` from the slot tree. The A/B switch, Copy and every
   other adoption pin it to live, preserving ADR-0010's A/B behaviour exactly. A session load
   is unaffected either way: `setStateInformation` adopts the full ANABASIS tree directly, so
   a project reopens in the view mode it was saved in, as before.

4. **The gesture path gains the view-tier filter the dead steps demanded.**
   `audioProcessorParameterChangeGestureBegin` skips undo arming for view-tier ids (bypass and
   the two monitor toggles — their diffs are unrestorable), and the gesture-end change test
   compares **what an undo could restore** (`strippedForUndoCompare` normalises the pinned
   view-tier entries on both sides), so a monitor toggle clicked mid-drag cannot mint a dead
   step either. `advancedMode` deliberately passes the filter: its step is real now.

## Consequences

- One ADV click costs one undo step **that works** — Undo returns the editor to the previous
  view — instead of one that silently did nothing.
- A/B compare behaviour is unchanged: switching slots never resizes the editor, and Copy never
  moves the view — **including the undo of a Copy**, which is part of Copy's behaviour and which
  the first implementation of this record got wrong. See the amendment note below.
- The undo duck: an undo whose only diff is `advancedMode` still requests the §2.8 forced duck.
  Sibling parity (it ducks every undo), accepted here for uniformity — the restore path cannot
  know the diff is sound-neutral without diffing, and the duck is inaudible-by-design.
- `setStateInformation` still clears all four stacks (the `historyEpoch` mechanism). A load is
  a session boundary; a Copy no longer claims to be one.
- **Supersedes:** ADR-0010 option E's *undo* half. Its A/B half (the compare never resizes the
  editor) is reaffirmed, now enforced by `applySlotToLive`'s default rather than by tier
  membership. The 0.1.0 clear-on-copy rationale (previously quoted in `copySlotToOther`'s
  header comment) is superseded in full.
- **Forecloses:** re-merging the two exclusions into one predicate. "Travels with undo" and
  "travels with A/B" are now distinct questions with distinct answers, and any future view or
  monitor parameter must answer both explicitly.

## Related code

- `src/PluginProcessor.h` — `copySlotToOther` declaration, `applySlotToLive(slot, adoptAdvanced)`
- `src/PluginProcessor.cpp` — `copySlotToOther` body, `slotWithLiveAdvancedMode` (the amendment
  below), `strippedForUndoCompare`, the gesture begin filter, the gesture-end compare,
  `undo()`/`redo()` passing `adoptAdvanced=true`
- `src/PluginParameters.cpp` — `isViewTierParam` (minus `advancedMode`), `isPresetExcludedParam`
  (plus `advancedMode` by name)

## Amendment — the Copy entry pins `advancedMode` at PUSH time (2026-08-07)

**Owner-confirmed 2026-08-07: "Copy undo should keep the current view mode."** No part of this
record's Decision changes; the amendment is an implementation correction that makes §Consequences
true, and it is recorded here rather than in a new ADR because it *restores* the stated contract
rather than moving it.

**What was wrong.** Decision item 3 pins `advancedMode` to live on every adoption path except
undo/redo, which is correct for every undo entry BUT ONE. Every other entry is a
`saveSlotFromLive()` taken at the moment of the step it records, so its `advancedMode` is by
construction the view the user had then — which is exactly what makes adopting it on undo the
right thing. The **Copy** entry is different: it is `storedSlot`, captured by the last
`switchToSlot` and frozen since. Toggle ADV after that switch and then Copy, and the
destination's undo entry carries the *pre-toggle* view mode; undoing the Copy — the one path with
`adoptAdvanced = true` — writes it back, and the editor changes size for a reason the user never
took. The adopt-side pin cannot cover it, because the entry itself predates the toggle.

**The correction.** `copySlotToOther` now pushes its entry through
`slotWithLiveAdvancedMode (storedSlot)`, which overwrites that one PARAM node's `value`/`raw`
from the live parameter before the entry is stored — the same pin, applied at push time because
that is where this entry's staleness enters. Deliberately not generalised into a helper shared
with `applySlotToLive`: there the rule is "do not adopt what the tree carries", here it is "do
not store what the tree carries", and they read alike only because they name the same parameter.
Nothing else moves — the audio half of the Copy undo, A/B behaviour, ordinary view switching and
serialization are untouched.

Evidence [Verified]:
- Test: `AnabasisStateTests` `testTeardownAndReengageInvariants` cases (3) — Copy undoable on
  the destination, history kept beneath, source untouched, redo walk — **(3b)** — the ADV toggle
  landing between the freezing A/B switch and the Copy, after which undoing the Copy reverts the
  sound and leaves the view alone (**mutation-verified**: removing the push-time pin fails that
  assertion and no other) — and (5) — ADV toggle mints a real step, undo restores the view, a
  bypass click mints nothing and stays pinned across a restore; `testAbSlotsAndTiers` —
  `advancedMode` preset-excluded by name, pinned across an A/B switch
- Sibling reference: `Anamorph/src/PluginProcessor.cpp` `abCopyToOther` (#12), read under
  ADR-0009's copy-and-adapt licence
- Directive: the owner's 0.1.1 instruction of 2026-08-06, item 4

## Amendment — a Copy that changes nothing records nothing (2026-08-07)

Decision 1 made Copy an undo step. It did so unconditionally, and after the first Copy the
destination already holds the live state: press Copy again with no edit between and the entry
pushed restores exactly what it replaces, so one Undo press on that slot visibly does nothing.
That is the **dead step** §Decision 4 set out to remove from the gesture path, reaching the stack
by a different route — and every other push in the file is already guarded by a change test.

`copySlotToOther` now applies the same one: `strippedForUndoCompare` on the entry's slot against
the slot the Copy is about to store, which normalises the view-tier parameters an undo could not
restore anyway, plus an equivalence test on the dirty datum — the other half of what the entry
would restore, and a real difference even when the parameter surface is identical. When the test
says nothing changes, **the redo line is left alone too**: redo is invalidated by a new action,
and a Copy that changes nothing is not one, which is exactly how the gesture path behaves when
its diff is empty.

Copy's semantics, the A/B behaviour, the restore path and the push-time `advancedMode` pin of the
first amendment are all untouched — the change is only *whether* an entry is recorded.

Evidence [Verified]: `AnabasisStateTests` `testTeardownAndReengageInvariants` cases **(3c)** —
three Copies in a row leave exactly one step, one undo returns the destination to its pre-copy
state and nothing remains — and **(3d)** — a no-op Copy leaves the destination's redo line intact
and that redo still re-lands the edit it belonged to. **Mutation-verified**: removing the change
test fails (3c), and clearing redo unconditionally fails (3d) alone.

## Review confirmation — an undo may adopt an `advancedMode` set in the OTHER slot (2026-08-07)

**Owner-reviewed 2026-08-07: behaviour confirmed, no change requested.** Recorded because it is a
near neighbour of the first amendment and would otherwise be re-derived by the next audit as the
same defect.

The sequence: in slot A (Simple) drag a knob — entry E is captured with `advancedMode` off;
switch to B, which pins the view (Decision 3), so ADV stays off; toggle ADV **on in B**, minting a
step on **B's** stack; switch back to A, pinned again, so ADV stays on; undo in A. The restore is
the one path with `adoptAdvanced = true` and E carries off, so the editor returns to Simple for a
toggle taken in the other slot.

This is **not** the first amendment's defect. E is a `saveSlotFromLive()` taken at the moment of
its own step, so its `advancedMode` is exactly the view the user had *in A* when they made that
edit — the property Decision 3 relies on. The Copy entry was the exception only because it is
`storedSlot`, frozen by an earlier switch and therefore contemporaneous with nothing. Undoing in A
back to a state whose view was Simple is the contract working; §Consequences promises only that a
*compare* never resizes, and this is an undo.

What it costs is a surprise when the two slots' views have diverged, and both alternatives cost
more: pinning `advancedMode` on the undo path as well would make this record's own second half
unreachable (an ADV toggle is a real step *whose undo restores the view*), and letting
`advancedMode` travel with A/B would reintroduce the editor resize on compare that ADR-0010
option E's surviving half forbids. Left as it stands, deliberately, on the owner's review.
