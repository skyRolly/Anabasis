# Worklog — 0.1.6: the GR history's vertical scale and percent text entry (2026-08-21)

Session-local evidence trail for version 0.1.6. Raw investigation material, NOT architecture
documentation — `docs/SOURCE_OF_TRUTH.md`: worklogs are never cited as policy. What is binding is
`CHANGELOG.md`, the code and the tests; this file carries the measurements and the alternatives
that were rejected, because a decision without its rejected alternatives is not reviewable.

Two owner reports, taken verbatim:

1. *"The GR line in the scrolling GR history can only reach halfway down the screen and then
   sticks — but the real GR may be more than that. Looking at the limiter's GR meter, the actual
   GR is more than this. The history view sticks at half the height and becomes a flat line, it
   does not extend further down."*
2. *"For percentage parameter boxes: entering a value between 0 and 1 should mean 0.5 → 50 %, 1 →
   100 %. If the entry carries a percent sign, it is the literal percentage: 0.1% → 0.1 %."*

---

## Item 1 — the GR trace

### The report is a complete description of the code

No reproduction was needed to locate this one, and that is worth saying plainly rather than
dressing it up as an investigation: the reported behaviour — a hard stop at *half the height*, a
*flat* line beyond it, and a *deeper* reading on the meter beside it — is the literal reading of
one expression in `GrHistoryView::paintHistory`:

```cpp
const float grSpan = 12.0f;                     // dB of visible reduction
...
const float gy = area.getY()
               + area.getHeight() * juce::jlimit (0.0f, 1.0f, -grDb / grSpan) * 0.5f;
```

`jlimit` saturates at `grDb == -12`, and the `* 0.5f` means that saturation lands at `y0 + h/2`.
Every reduction past 12 dB therefore drew the same horizontal line across mid-panel. Meanwhile
`GrMiniMeter` (`CurveView.h`), the per-stage meter the reporter compared against, divides by
**24 dB** — so with 18 dB of reduction the meter reads three-quarters full while the trace, on the
same screen and describing the same quantity, claims it has stopped moving.

Two facts were checked rather than assumed:

- **The data is not the limit.** The ring stores what the publisher writes, and the publisher
  writes `Decibels::gainToDecibels (engine.lastBlockMinGain(), -60.0f)`
  (`PluginProcessor.cpp:968`). The clamp was purely in the view.
- **No document specified 12 dB.** `DESIGN.md` §2.9 and §6.2/§6.3 describe the strip as
  "10–30 s scrolling waveform + GR trace" with no dB span, and ADR-0023 item 6's "fixed scale" is
  the *horizontal* right-anchored pitch (that ADR's own words: "pitch = width over one full
  window's bucket count"). So this is not an ADR conflict and not a hard stop — a display span with
  no record anywhere is an implementation choice, which is the only reason it could drift from the
  meter's in the first place.

### Why 24 dB over the full height, and not something else

| Option | Verdict |
|---|---|
| **24 dB over the full panel height** | **Taken.** It is the span the other GR readout already uses, which is the comparison the report was made from — and it *preserves the existing pixels-per-dB exactly*: 24/full-height is the same slope as 12/half-height, so 12 dB still lands at half the panel and every reduction that was already visible is drawn at the y it has always been drawn at. The change is purely additive: the bottom half of the panel becomes reachable |
| Keep 12 dB, spend the full height | Rejected. It doubles the depth of every trace already on screen — every screenshot and every user's muscle memory re-scales — and it still goes flat at 12 dB, which *is* the complaint. It fixes the half-panel half of the report and leaves the "sticks and goes flat" half |
| 30 dB / 36 dB / "a bit more headroom" | Rejected. Any span the meter does not share re-creates the defect in a milder form: the two readouts of one quantity would still disagree, just less obviously, and the next report would be harder to diagnose than this one |
| Auto-scale the axis to the window's deepest reduction | Rejected. A reference that moves cannot be read against itself between two moments, which is exactly what a *history* view is for, and it would put the same programme at a different depth depending on what else is in the window. It is also the shape ADR-0023 item 6 already rejected on the horizontal axis, for the same reason |
| Add dB gridlines / an axis label to make the span legible | Rejected. `AI_AGENT_POLICY.md` C8: user-visible text and interface furniture are the maintainer's to specify, and a behaviour fix does not license announcing itself in the UI. The manual states the span instead |

### The structural half of the fix

The mapping moved out of `paintHistory` into a pure static, `GrHistoryView::grY`, joining
`windowEntries`, `buckets` and `drawsZeroRegion`. That header has said since 0.1.1 that "a version
reachable only from `paint` is a version no test can pin", and it has now been the mechanism of
three separate defects in the same function (0.1.1 shimmer, 0.1.3 left edge, and this one). This
expression had survived two rounds of deliberate work on the very lines around it. The zero-line
constant in the unmeasured-region branch was routed through the same function (`grY (0.0f, …)`)
rather than left as a bare `area.getY()`, so the trace's zero and the trace's scale cannot be
edited apart.

The 24 dB constant itself is now `abgui::meters::grSpanDb` in `LookAndFeel.h`, read by both
readouts. Sharing it is the part that makes the two agree by construction rather than by two
authors happening to type the same number.

---

## Item 2 — percent text entry

### What the parser did

```cpp
auto pctFrom = [] (const juce::String& t) { return t.removeCharacters ("% ").getFloatValue(); };
```

Every bare number was a literal percent. All seven percent parameters span 0…100 (`loudness`,
`compMix`, `compStereoLink`, `clipMix`, `colourDepth`, `stereoLink`, `transientPreserve`), so
typing `0.5` — the natural spelling of "half" for a mix control — landed on **half of one
percent**: inaudible, and visually indistinguishable from zero at the knob.

### The rule, and the boundary

| Typed | Reads | Why |
|---|---|---|
| `0.5` | 50 % | bare, in (0, 1] → fraction of full scale |
| `1` | 100 % | the owner's second example; the top of the fraction window |
| `0` | 0 % | zero under either reading |
| `1.5` | 1.5 % | above the window → literal, as it always was |
| `50` | 50 % | unchanged |
| `0.1%` | 0.1 % | explicit `%` → literal, always |
| `1 %` | 1 % | the other reading of the boundary, reachable on demand |

The discontinuity at 1 is real: `1` is 100 % and `1.5` is 1.5 %. One of the two readings has to win
there and the directive names which. Rejected alternatives:

- **A `< 1` window, so `1` means one percent.** Rejected: it contradicts the directive's own
  example. (The frequency parsers next door took the opposite decision on their boundary for the
  same kind of reason — `hzKhzFrom` uses `<` because 20 Hz is a *legal* value on that knob, while
  `khzFrom` uses `<=` because nothing on a 1–20 kHz knob is. Each pivot follows the range it
  serves; neither is a house style to copy blindly.)
- **Treat a number below 1 as a fraction even with `%`.** Rejected: it makes the entire sub-1 %
  region untypeable, which is precisely what the second half of the directive exists to prevent.
- **A percent parser per parameter class.** Rejected: all seven share one range and one meaning.
  Nothing to discriminate.

### The display half, which is not optional

`pctText` printed `roundToInt (v)`, so the moment `0.1%` was accepted the box showed `0 %`. Two
consequences, and the second is a defect in its own right:

- the box denies the value it has just accepted, so the directive's second half is unobservable
  from the UI it was reported against;
- `getValueForText (getText (0.1f))` returned **0**, so any consumer that round-trips a parameter
  through its own displayed text destroys the value. That is measured, not argued: the assertion
  that catches it is the one the integer-only mutant kills alongside the display assertion.

So the formatter now prints one decimal when the value is not a whole percent, and prints exactly
what it always printed when it is (`50 %`, `100 %`). Rejected:

- **Always one decimal (`50.0 %`).** Rejected: it changes every percent readout in the product to
  fix a case that only arises below the whole-number grid.
- **Quantise the percent ranges to 0.1 steps**, the way `twoDecimalRange` fixes the ceiling's
  display/value disagreement at the source. Rejected as out of scope and over-licensed: an
  `interval` on the range changes `getNumSteps()` (the frozen snapshot's column would move from
  `cont.` to `1001`), quantises host automation, and re-scales stored normalised values — a rule-3
  parameter-surface change requiring an ADR. The residual it would close is small and is written
  down instead: a value below 0.05 % reads `0.0 %`, distinguishable from the `0 %` that only an
  exact zero prints.

The `%` suffix is now load-bearing rather than decorative, and that is stated at both lambdas:
because `pctText` *always* emits it, every string the product itself displays takes the literal
branch, so `getValueForText(getText(v)) == v` holds across the whole range including the fraction
window. A future edit that drops the suffix would silently multiply small values by a hundred on
every host round-trip.

---

## Verification

Linux x86-64, GCC 13.3, Release, JUCE 9.0.1 `e18f7f5…`, the existing build tree.

- **Suites:** `AnabasisTests` 296 + `AnabasisStateTests` 873 = **1169 checks, 0 failures** (was
  1141; +28, all in the two new tests).
- **Mutation runs — four, each killing a disjoint set**, which is the evidence that the assertions
  measure what they name:

  | Mutant | Killed |
  |---|---|
  | `grY` restored to `jlimit (0, 1, -grDb / 12) * 0.5` | 4 of the 9 GR assertions. The two scale-preservation assertions SURVIVE, exactly as they must — 12 dB lands at half height under both forms, which is the property that says the fix re-scales nothing |
  | `GrMiniMeter`'s divisor changed to 12 | ONLY the rendered-lane assertion. So that assertion is measuring the meter, not re-reading the shared constant |
  | `pctFrom` restored to the pre-0.1.6 body | Exactly the 5 fraction assertions. None of the literal-`%`, already-worked, display or round-trip assertions — the old parser handled those correctly and the test says so |
  | `pctText` restored to `roundToInt` | The fractional-display assertion **and** the `getValueForText(getText(v)) == v` round-trip |

- **pluginval** at the `build.yml` strictness, both modes ×3, editor under xvfb.
- `check-docs.py` (self-test + corpus), `check-portability.py`, `check-citations.py` against both
  `origin/main` and the previous head.

## What this round did NOT do

- No parameter added, renamed or removed; no range, default, choice ordering or automation flag
  touched; `tests/fixtures/parameter_registry.snapshot` unchanged and deliberately not re-frozen
  (it carries id/name/min/max/default/steps/automation and no display text, so nothing in this
  round is in it).
- No DSP source changed. The GR ring, its publisher and the engine are untouched — both fixes are
  on the presentation side of the boundary.
- No ADR. Neither change meets an `ARCHITECTURE_REVIEW_GATE.md` item: the GR span was recorded
  nowhere and contradicts nothing (checked in DESIGN §2.9/§6.2/§6.3 and ADR-0023 item 6, which is
  the horizontal pitch), and text parsing is not the parameter surface rule 3 governs.
