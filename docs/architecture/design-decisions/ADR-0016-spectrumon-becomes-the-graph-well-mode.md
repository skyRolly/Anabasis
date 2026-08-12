# ADR-0016 — `int_spectrumOn` is repurposed from "is the spectrum shown" to "which graph-well view is active"

**Status:** Accepted (2026-08-06 — implementation decision taken under the owner's round-2
directive of 2026-08-05, which specified the combined graph well; the field choice was the
autonomous half)

> **ARCHITECTURE REVIEW GATE — CLEARED (2026-08-06).** `ARCHITECTURE_REVIEW_GATE.md` lists
> "**Serialization Registry change** — any field add/remove/**semantic change**", and
> `SESSION_COMPATIBILITY_POLICY.md` rule 1 covers a field "removed **or have its meaning
> changed**". This is the meaning-change case, so it needed the same explicit owner clearance as
> ADR-0015's `int_meterTargets` removal — and it has one, granted **separately and on its own
> terms**. The owner's confirmation covers three things by name:
>
> 1. **the semantic change** of `int_spectrumOn`, from the previous graph/spectrum visibility
>    meaning to the current graph-well mode meaning;
> 2. **keeping it a pre-1.0 migration change**, taken inside the window rather than deferred;
> 3. **that existing stored values continue loading with no migration path**, because that
>    window is still open.
>
> Item 3 is the one worth reading twice: it is an explicit acceptance of the read delta this ADR
> tabulates below — a stored `true` now shows the spectrum where it used to show the GR trace
> beside it — not a statement that no delta exists.
>
> **This clearance is ADR-0016's alone.** It does not widen ADR-0015, whose own sign-off named
> three different changes, and it does not extend to `int_uiScale`'s ladder narrowing — that is a
> third record, [ADR-0017](ADR-0017-uiscale-ladder-narrowing.md), which was cleared later the same
> day on its own separate confirmation.

## Context

The owner's round-2 directive (item 9) combined the spectrum and the scrolling GR history into
"two switchable modes within the same view/page", with the switch on the graph itself. The
directive settled the *product*; it did not say which session field should carry the choice.
`int_spectrumOn` already existed and already held a related boolean, so the implementation reused
it. That reuse is a change of the field's meaning, which the ledger governs.

## Problem

The well needs one persisted bit: which of the two views is showing. Either the existing field
takes the new meaning, or a new field is added and the old one is retired.

## What the field used to mean, precisely

Read from the pre-change tree (`7686204:src/gui/PluginEditor.cpp:1204-1207, 1214-1217`):

| | `int_spectrumOn = true` | `int_spectrumOn = false` |
|---|---|---|
| **Advanced** | metering strip **split**: GR on the left half, spectrum on the right — **both** visible | GR full width; spectrum hidden |
| **Simple** | GR only — `spectrumView->setVisible (false)` unconditionally, the field **not read** | GR only (same) |

So the old meaning was narrow: *does the spectrum take half of the Advanced metering strip?* The
GR trace was never hidden by it, in either view, and Simple ignored the field entirely.

## What it means now

*Which one of the two views fills the graph well* — in **both** editor modes. `true` selects the
spectrum, `false` selects the GR history; the two views hold identical bounds and only visibility
flips.

## Options

- **A. Repurpose `int_spectrumOn`.** One bit, one field, no schema growth, and the name still
  reads correctly at the call sites (`true` does mean "the spectrum is what you see"). **Chosen.**
- **B. Add `int_graphView` and stop reading `int_spectrumOn`.** Leaves the old field in the
  schema with no reader — the tombstone shape ADR-0015 option C already rejected, and for the
  same reason: a permanent obligation bought to protect zero sessions, since no build has
  shipped. It would also make the ledger carry two fields for one bit.
- **C. Add `int_graphView` and remove `int_spectrumOn`.** Honest, but it is a removal *and* an
  addition where a meaning change does the same work, and it spends a second field name on the
  same concept. Rejected as the more expensive route to the identical end state — with the note
  that after the first shipped build the calculus reverses, because then a meaning change is the
  expensive one.

## Decision

`int_spectrumOn` carries the graph-well mode. Type (`bool`) and default (`true`) are unchanged, so
no read path changes and no old session fails to load.

**Migration: none, and none is owed — but the read is not value-identical.** The type is the same,
so the §4.4 defaults-first overlay handles an old blob unchanged. What differs is what the same
stored value *shows*:

- **`false`** — what a user who had dismissed the spectrum stored. Displays exactly what it did
  before: the GR history, full width, in both views. **No visible change.**
- **`true`** — the default, and therefore the ordinary case. The GR trace is now **hidden** where
  it previously was not: Advanced goes from GR-and-spectrum side by side to spectrum only, and
  Simple goes from GR to spectrum. Reachable in one click on the corner chip, and the manual
  (§2.4, §3.4, §8) tells the user so.

This is the whole cost, it is display-only, and it is free while the pre-ship window ADR-0015
describes is open.

## Consequences

- **The name is now slightly narrower than the concept.** `int_spectrumOn` names one of the two
  views rather than the choice between them; `int_graphView` would read better. Not renamed,
  because a rename is a schema change with no behavioural gain, and the call sites all state the
  mapping. Recorded so the next reader does not mistake the name for the full meaning.
- **One shared bit, not one per view.** Simple and Advanced cannot show different views. That is
  the directive's own framing ("the same view/page"), and a per-view preference would need a
  second field — a schema addition, and therefore a decision to take before the window closes,
  not after.
- **The DESIGN §6.2 "GR-only Simple strip" wireframe is superseded**, as the layout site already
  records.
- **Doc-sync:** `DESIGN.md` §4.3's row for this field carries a per-row forward pointer to THIS
  record. The banner there names all three of ADR-0015/0016/0017 against the rows each one
  actually supersedes — it credited everything to ADR-0015 for one commit, which is the
  widening the three-record split exists to prevent.
- **Forecloses:** reading `int_spectrumOn` as "is the spectrum visible" anywhere — with the well
  always showing one of the pair, visibility of one view is the negation of the other's.

## Related code

- `src/InternalState.h` — the field and its `true` default
- `src/gui/PluginEditor.cpp` — `layoutAdvanced` / `layoutSimple` (identical bounds for both
  views, visibility split) and the 24 Hz tick's mode follow
- `src/gui/SpectrumView.cpp` · `src/gui/GrHistoryView.cpp` — the two corner chips that write it

Evidence [Verified]:
- Source: the files above; the pre-change behaviour tabulated above was read from
  `7686204:src/gui/PluginEditor.cpp`
- Test: `AnabasisStateTests` `testTheGraphWellViewsOnlyClaimTheirModeChips` — both chips flip the
  field their way and neither claims a click over its trace
- Directive: the owner's round-2 instruction of 2026-08-05, item 9
