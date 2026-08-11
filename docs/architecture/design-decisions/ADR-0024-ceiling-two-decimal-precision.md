# ADR-0024 — The Ceiling is quantised to two decimal places, in the range rather than in the label

**Status:** Accepted — gate cleared 2026-08-11 (the owner's directive, which specifies the
behaviour and rules out the display-only reading by name: *"The Ceiling control currently allows
values such as `-0.1` but it must become `-0.10` … This is NOT only a display formatting change.
The actual parameter value used by DSP must also be limited to two decimal places … No third
decimal place exists internally … Do not solve this only through UI text formatting."*)

## Context

`ceiling` is registered as a continuous `AudioParameterFloat` over `{-20, 0}` dB with the default
`-0.1`, printed to one decimal. It is the one control a mastering user dials to a *number* rather
than by ear: a delivery spec says −1.0 dBTP or −0.3 dBTP, and the value the plugin holds is the
promise it makes about what leaves it (ADR-0002/0006 — the limiter's threshold *is* the ceiling,
and `CeilingClamp` is the unconditional backstop). A knob that reads `-0.1 dB` while holding
−0.14 is therefore not a cosmetic mismatch; it is the guarantee misreported at the one place the
guarantee is stated.

The one-decimal display made that reachable in normal use. Any knob drag, any host automation
write, any typed value with more precision landed on an arbitrary float and was then rounded for
display only.

## Problem

Where does the quantisation have to live so that it is true of the **value** — the number the DSP
compares against, the number saved into a session, the number a host automation lane plays back —
rather than of the string beside the knob?

## Options

1. **Format the display to two decimals.** One line. Rejected outright by the directive, and
   correctly: it widens the lie rather than removing it. The stored value keeps its third decimal
   and the label now claims two.
2. **Give the `NormalisableRange` an `interval` of 0.01.** Chosen.
3. **Supply custom `convertFrom0to1` / `convertTo0to1` / `snapToLegalValue` functions that round.**
   Rejected as redundant — see the correction below.
4. **Round at the point of USE, in the engine.** Rejected. It would make the DSP right and leave
   the parameter, the saved session, the automation lane and the host's own generic editor all
   holding a value the plugin does not honour — the same class of divergence, moved one layer down
   and made harder to see.

## Decision

`ceiling` is registered with `twoDecimalRange (-20.0f, 0.0f)` — `NormalisableRange<float> { lo, hi,
0.01f }` — and the display becomes `String (v, 2)`.

**Why an interval is sufficient, which is not obvious from the call site.** The snap is applied by
`RangedAudioParameter`, not by the caller. `AudioParameterFloat::setValue` — the entry point a host
uses for every automation write, and the one `AudioProcessorValueTreeState` drives on state restore
— reads `value = convertFrom0to1 (newValue)` (`juce_AudioParameterFloat.cpp:98`), and *that*
`convertFrom0to1` is the parameter's, which is
`range.snapToLegalValue (range.convertFrom0to1 (…))` (`juce_RangedAudioParameter.cpp:54-58`; its
header says "Denormalises and snaps"). So every path lands on the grid:

| Path | Where it lands |
|---|---|
| Host automation | `AudioParameterFloat::setValue` → the parameter's snapping `convertFrom0to1` |
| State restore, preset apply | `AudioProcessorValueTreeState` → `setValueNotifyingHost` → `setValue` → same |
| The DSP's own read | the APVTS adapter publishes `getParameter().convertFrom0to1 (normalised)` into the atomic `CachedParams` resolves and `EngineParameters::ceilingDbTp` carries |
| The editor knob | `SliderParameterAttachment` copies the range's functions **and** its interval into the slider |
| Typed text | `getValueForText` snaps on the way in; `getText` formats through the same wrapper, so label and value cannot disagree |
| Factory preset overrides | `PresetManager` already routes overrides through `snapToLegalValue` |

**A correction, recorded because the wrong reading is the natural one.** This ADR's first draft
rejected option 2 and chose option 3, asserting that `setValue` reached the *range's* raw
`convertFrom0to1` and therefore bypassed the interval. That is what
`juce_AudioParameterFloat.cpp:98` looks like in isolation, and it is wrong — the name resolves to
the parameter's snapping wrapper. A mutation run settled it: with the custom lambdas replaced by a
bare `interval`, `testCeilingIsQuantisedToTwoDecimals` cannot tell the two implementations apart,
while the original continuous range fails five of its assertions. The custom functions were
redundant, so they are gone; the test that proved it is the test that stays.

**The tie rule is JUCE's.** `snapToLegalValue` computes
`start + interval * floor ((v - start) / interval + 0.5)`, which rounds a halfway value toward
+infinity — so −0.125 lands on −0.12. The directive leaves the tie open ("−0.12 or −0.13 depending
on the chosen rounding rule"); every non-tie case rounds to nearest, so −0.123 → −0.12 and
−0.129 → −0.13 as specified.

**`interval` does not make the parameter discrete to a host.**
`AudioParameterFloat::getNumSteps()` returns the base default rather than deriving from the range
(`juce_AudioParameterFloat.cpp:100`), so the automation surface stays continuous while every value
it can land on is on the grid — and `tests/fixtures/parameter_registry.snapshot`, which pins the
step count among other fields, is unchanged.

**"Two decimal places" in binary floating point** means the nearest `float` to a two-decimal
decimal (−0.12 is stored as −0.119999886 at this range's grid). The reachable set is what the
requirement is about, and no third-decimal step is reachable.

## Consequences

- **Rule 3 of `PARAMETER_COMPATIBILITY_POLICY.md` applies, which is why this ADR exists.** The
  range is semantic, and quantising it alters recall for any stored value that is off the new grid:
  such a value snaps on load. Nothing has left this repository (`COMPATIBILITY_POLICY.md`
  §"When the contract starts"), so there is no released session to migrate, and the snap is the
  same shape as ADR-0017's "out-of-set stored values normalise on adoption".
- **The default does not move.** −0.1 is already on the grid, so `testRegistrySnapshot`'s frozen
  default and every all-defaults null are untouched, and no factory preset overrides `ceiling`.
- **The reachable resolution is coarser**: 0.01 dB steps over a 20 dB range. That is finer than any
  delivery spec is written to and finer than the 0.005 dB tolerance `LoudnessMeterView`'s
  over-ceiling comparison already carries, so nothing downstream needs a matching change.
- **The pattern is available but is deliberately NOT applied elsewhere.** `twoDecimalRange` is a
  general helper; only `ceiling` uses it, because only `ceiling` is a control users set to a
  specified number. Applying it to the rest of the surface would be a parameter-surface change
  nobody asked for and rule 3 would bind on every one of them.

## Verification

`testCeilingIsQuantisedToTwoDecimals` (`tests/state_tests.cpp`) exercises each entry point
separately rather than trusting one to stand for the rest: a 998-step sweep of **raw normalised
host writes**, the **DSP-visible atomic** after an off-grid write, **typed text** in both rounding
directions with the trailing zero, a **save/load round trip** asserted bit-identical, and the
**default** asserted on the grid.
