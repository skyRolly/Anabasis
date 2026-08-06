# ADR-0017 — `int_uiScale`'s legal value set narrows from seven steps to five, and out-of-set values converge on load

**Status:** Accepted (2026-08-06 — implementation decision taken under the owner's round-2
directive of 2026-08-05 item 12, which specified the sibling's XS…XL scale; the field treatment
was the autonomous half)

> **ARCHITECTURE REVIEW GATE — CLEARED (2026-08-06).** `ARCHITECTURE_REVIEW_GATE.md` lists
> "**Serialization Registry change** — any field add/remove/**semantic change**". This is the
> third member of that class in the round-2 batch, and it now has its own explicit clearance,
> granted **separately** from ADR-0015's and ADR-0016's. The owner's confirmation covers three
> things by name:
>
> 1. **the reduced ladder itself**, accepted before the first shipped build;
> 2. **that stored values outside the new set may normalise on adoption** — which is the
>    acceptance of §Decision item 3's two concrete outcomes (80/90 reopen one step away;
>    175/200 reopen at 150), not a statement that they do not happen;
> 3. **that this is a pre-1.0 contract decision**, with **no released-session migration
>    obligation** — because no session written by a released build exists to owe one to.
>
> **The clearance is ADR-0017's alone.** It does not widen ADR-0015 or ADR-0016, each of which
> was signed off naming its own changes; all three records stay separate.

## Context

Round-2 item 12 adopted the sibling product's UI-scale control: five steps shown as **XS / S / M /
L / XL**, M the original size. The visible half is a display change. The half this record exists
for is that `int_uiScale` stores a **percent**, and the set of percents the schema accepts changed
with it.

`SERIALIZATION_REGISTRY.md` already treats the ladder as part of this field's read contract — §1.6
names "the `int_uiScale` ladder normalisation applied at adoption", and §2's read-rules table lists
`int_uiScale` under "Out-of-range values → clamped at the read boundary … to the ladder". Narrowing
the ladder therefore changes what that documented read rule does to a stored value, which is the
ledger's own definition of a change that must be recorded.

## What changed

| | Legal set | Default |
|---|---|---|
| Before (`7b38d1e:src/InternalState.h:28`) | 80, 90, 100, 125, 150, 175, 200 | 100 |
| After (`src/InternalState.h`, `ui_scale::steps`) | **75, 85, 100, 125, 150** | 100 |

The field's **type and meaning are unchanged** — it is a percent, and 100 still means the original
size. What changed is the accepted domain, and therefore which stored values survive a load
unmodified:

| Stored | Loads as | Note |
|---|---|---|
| 100 · 125 · 150 | unchanged | the three steps common to both ladders — the overwhelming majority |
| 80 | **75** | |
| 90 | **85** | |
| 175 · 200 | **150** | both converge on the new top step |

**The convergence is persisted.** `InternalState::replaceFrom` normalises at adoption rather than
at read, so the corrected percent is what the next `getStateInformation` writes and the original
value is unrecoverable after one save. That behaviour is not new and is not part of this change —
`ui_scale::nearest` has always normalised out-of-list values, and round 63 moved it to adoption
precisely so the tree could not keep re-serialising an illegal percent. What this change does is
alter *which* values are out-of-list.

## Options

- **A. Narrow the ladder, keep the field a percent, let `nearest` converge stored values.**
  **Chosen.** The normalisation path already existed and already had this exact job; no new code,
  no new field, and the three most-used steps are untouched.
- **B. Store an INDEX into the ladder instead of a percent.** Would make the display and the
  storage the same thing. **Lost:** it is a genuine meaning change — the same stored `3` would
  mean 125 % on one ladder and something else on the next — where a percent stays interpretable
  against any ladder. It also throws away the property that makes option A safe: a percent from an
  unknown build still lands somewhere sensible.
- **C. Add a second field and leave `int_uiScale` alone.** Rejected for the reason ADR-0015 option
  C and ADR-0016 option B were: a live field with no reader is a permanent obligation bought to
  protect zero sessions.
- **D. Keep the seven-step ladder and re-label only.** Would preserve every stored value.
  **Lost:** it contradicts the directive, which specified the sibling's *values* (its
  `applyUiScale` scales ×100), not merely its labels — and two ladders' worth of steps behind five
  labels is the ambiguity the labels exist to remove.

## Decision

1. **The ladder is `ui_scale::steps` = {75, 85, 100, 125, 150}**, with `ui_scale::names` index-
   locked to it by a `static_assert`, and 100 remains the default.

2. **No migration path, and none is written.** `nearest` is the migration: it is total over the
   integers, it is the rule the field already carried, and it is applied at adoption so the result
   is consistent everywhere afterwards. A session from any build loads and renders at a legal
   scale.

3. **What a reviewer is actually being asked to accept** — stated plainly, because "no migration"
   reads as "no consequence" and here it is not:
   - a user who had chosen **80 %** or **90 %** reopens one step away, at 75 or 85, and cannot get
     the old value back;
   - a user who had chosen **175 %** or **200 %** reopens at **150 %**, which is a visibly smaller
     window and the largest single jump in the table.

   Both are recoverable in one Settings click *to a neighbouring legal step*, never to the
   original. Free while the pre-ship window is open; after the first shipped build the same change
   would need a real migration or would simply be refused.

4. **This is a domain change, not a semantic one — and it is recorded anyway.** The unit,
   the interpretation and the default are all untouched, so a reader who wants to argue this is
   below the gate's bar has a case. It is recorded at the gate's bar regardless, because the
   ledger names the ladder as part of the field's read contract and the cost of over-recording a
   pre-ship change is a paragraph, while the cost of under-recording one is a user's window size.

## Consequences

- **The three common steps make the blast radius small**: 100/125/150 are unchanged, and 100 is
  the default, so the ordinary session is untouched.
- **The seven-step ladder is gone from the schema's accepted set**, so a future contributor
  restoring 175 or 200 is making an addition, not a repair.
- **`nearest`'s worked examples are load-bearing documentation**, not decoration — they were left
  quoting the old ladder's outputs for a round, which is how the narrowing first became visible in
  review. Both copies now quote the current ladder.
- **Forecloses:** treating the ladder as a display concern. It is the field's accepted domain, and
  changing it is a ledger change under this record.
- **Doc-sync:** `DESIGN.md` §4.3's row for this field carries a per-row forward pointer to THIS
  record. The banner there names all three of ADR-0015/0016/0017 against the rows each one
  actually supersedes — it credited everything to ADR-0015 for one commit, which is the
  widening the three-record split exists to prevent.

## Related code

- `src/InternalState.h` — `ui_scale::steps` / `names` / `nearest` / `nearestIndex`, and
  `replaceFrom`'s normalisation at adoption
- `src/gui/PluginEditor.cpp` — `nearestScaleIndex`, the Settings combo built **from** the ladder
  rather than beside it, and `applyUiScale`

Evidence [Verified]:
- Source: the files above; the pre-change ladder was read from `7b38d1e:src/InternalState.h:28`
- Test: `AnabasisStateTests` `testAnOutOfListUiScaleClampsConsistently` — an out-of-list stored
  value converges to its nearest step and the displayed step agrees with the applied transform,
  exercised with the new ladder's own numbers (92 → 85), plus the §4.4 missing-field case
- Directive: the owner's round-2 instruction of 2026-08-05, item 12
