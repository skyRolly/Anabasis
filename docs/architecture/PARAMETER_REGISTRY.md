# PARAMETER_REGISTRY.md

The **ledger of the host-visible parameter surface** — the registry ADR-0010 designates as the
citation target from P1 onward (before this file existed, `DESIGN.md` §4.2 was the table of
record; from now on cite this file or the snapshot, not §4.2).

**Enforcement is mechanical, not editorial.** The table below is generated from
`tests/fixtures/parameter_registry.snapshot`, which `AnabasisStateTests` compares byte-wise against
the live `createAnabasisLayout()` on every run. A change to any ID, range, default, step count,
order or automation flag fails CI until the snapshot is deliberately re-frozen
(`AnabasisStateTests --write-snapshot`) — and re-freezing is a **Hard Stop** review item
(`PARAMETER_COMPATIBILITY_POLICY.md` rules 1/3/4). If this file and the snapshot ever disagree,
the snapshot is right; regenerate the table here.

**Frozen at v0.1.0:** IDs, ranges, defaults, choice orderings (rules 1 and 3). Display names are
launch wording, revisable under rule 2 with a deliberate snapshot re-freeze and a `Changed`
changelog entry. `kVersion = 1` on every parameter; a parameter-set change bumps it by ADR.

## Conventions (ADR-0010)

- Short camelCase IDs in the single `pid::` namespace (`src/PluginParameters.h`).
- Every discrete parameter (bool/choice) uses the `Raw*` exact-normalised classes so a host
  round-trip restores the same index — the pluginval state-restoration contract.
- Log tapers on every ⊕(log) row of DESIGN §4.2. Two defaults are knowingly the taper images of
  their round declared values — `limRelease` 100.000015, `eqBell2Freq` 2999.999756 — recorded in
  `docs/procedures/TESTING.md`; do not "fix" the snapshot to hide them.
- Units render through the shared `db`/`ms`/`hz`/`pct` formatter lambdas with suffix-tolerant
  parsers. Two of those parsers read a bare number by the knob's own range rather than literally,
  and both are owner directives rather than conveniences: the frequency parsers treat a bare number
  below the knob's Hz floor as kHz (0.1.1), and `pctFrom` treats a bare number in (0, 1] as a
  FRACTION of full scale — `0.5` is 50 %, `1` is 100 % — while an explicit `%` is always the
  literal percent, `0.1%` included (0.1.6). None of this touches a range, a default or an ID; the
  frozen snapshot carries no display text, so neither change re-froze it.

## The 50 rows

Range/default/steps/automation are the snapshot's columns verbatim ("cont." = a continuous range,
`getNumSteps()`'s sentinel). Tier: **view** = excluded from A/B, undo *and* presets;
**preset-excl** = excluded from presets only (travels in A/B and undo);
**adv** (`advancedMode` alone, since ADR-0018) = travels with **undo** but is pinned across
A/B/Copy (`applySlotToLive`'s adopt-flag) and excluded from presets by name. The predicates are
`isViewTierParam` / `isPresetExcludedParam` (`src/PluginParameters.cpp`) — a preset skips
view ∪ {freeze, advancedMode}.

| ID | Name | Range | Default | Steps | Auto | Tier |
|---|---|---|---|---|---|---|
| `bypass` | Bypass | 0 … 1 | 0 | 2 | yes | view |
| `advancedMode` | Advanced | 0 … 1 | 0 | 2 | **no** | adv ¹⁸ |
| `loudness` | Loudness | 0 … 100 | 0 | cont. | **no** | — |
| `character` | Character | 0 … 1 | 0 | cont. | **no** | — |
| `tone` | Tone | -1 … 1 | 0 | cont. | **no** | — |
| `ceiling` | Ceiling | -20 … 0 | -0.1 ¹⁵ | 0.01 ¹⁶ | yes | — |
| `freeze` | Freeze | 0 … 1 | 0 | 2 | **no** | preset-excl |
| `loudnessComp` | Loudness Comp | 0 … 1 | 0 | 2 | yes | view |
| `deltaMonitor` | Delta | 0 … 1 | 0 | 2 | yes | view |
| `inputGain` | Input Gain | -12 … 24 | 0 | cont. | yes | — |
| `scHpfFreq` | SC HPF ²³ | 20 … 300 | 20 | cont. | yes | — |
| `compRatio` | Comp Ratio | 1.1 … 4 | 1.5 | cont. | yes | — |
| `compThreshold` | Comp Threshold | -40 … 0 | 0 | cont. | yes | — |
| `compAttack` | Comp Attack | 5 … 100 | 30 | cont. | yes | — |
| `compRelease` | Comp Release | 50 … 1000 | 200 | cont. | yes | — |
| `compAutoRelease` | Comp Auto Rel | 0 … 1 | 1 | 2 | yes | — |
| `compKnee` | Comp Knee | 0 … 12 | 6 | cont. | yes | — |
| `compDetector` | Comp Detector | 0 … 1 | 0 | 2 | yes | — |
| `compMix` | Comp Mix | 0 … 100 | 100 | cont. | yes | — |
| `compStereoLink` | Comp Stereo Link | 0 … 100 | 100 | cont. | yes | — ¹⁹ |
| `clipShape` | Clip Shape | 0 … 1 | 0.5 | cont. | yes | — |
| `clipDrive` | Clip Drive | 0 … 24 | 0 | cont. | yes | — |
| `clipMix` | Clip Mix | 0 … 100 | 100 | cont. | yes | — |
| `colourModel` | Color | 0 … 3 | 1 | 4 | yes | — |
| `colourBalance` | Odd/Even | -1 … 1 | 0 | cont. | yes | — |
| `colourTone` | Color Tone | -1 … 1 | 0 | cont. | yes | — |
| `dynTilt` | Dynamic Tame | 0 … 2 | 0 | cont. | yes | — |
| `colourDepth` | Color Depth | 0 … 100 | 0 | cont. | yes | — |
| `limGain` | Limiter Gain | 0 … 18 | 0 | cont. | yes | — |
| `lookahead` | Lookahead | 0.5 … 10 | 2 | cont. | **no** | — |
| `limRelease` | Lim Release | 1 … 1000 | 100 | cont. | yes | — |
| `limAutoRelease` | Lim Auto Rel | 0 … 1 | 1 | 2 | yes | — |
| `limStyle` | Style | 0 … 2 | 0 | 3 | yes | — |
| `stereoLink` | Limiter Stereo Link ²³ | 0 … 100 | 100 | cont. | yes | — |
| `transientPreserve` | Transients | 0 … 100 | 50 | cont. | yes | — |
| `truePeakMode` | True Peak | 0 … 1 | 0 ¹⁵ | 2 | **no** | — |
| `eqTilt` | Tilt | -3 … 3 | 0 | cont. | yes | — |
| `eqLowShelfFreq` | LS Freq | 20 … 500 | 100 | cont. | yes | — |
| `eqLowShelfGain` | LS Gain | -12 … 12 | 0 | cont. | yes | — |
| `eqHighShelfFreq` | HS Freq | 1000 … 20000 | 8000 | cont. | yes | — |
| `eqHighShelfGain` | HS Gain | -12 … 12 | 0 | cont. | yes | — |
| `eqBell1Freq` | Bell 1 Freq | 20 … 20000 | 300 | cont. | yes | — |
| `eqBell1Gain` | Bell 1 Gain | -12 … 12 | 0 | cont. | yes | — |
| `eqBell1Q` | Bell 1 Q | 0.3 … 8 | 1 | cont. | yes | — |
| `eqBell2Freq` | Bell 2 Freq | 20 … 20000 | 3000 | cont. | yes | — |
| `eqBell2Gain` | Bell 2 Gain | -12 … 12 | 0 | cont. | yes | — |
| `eqBell2Q` | Bell 2 Q | 0.3 … 8 | 1 | cont. | yes | — |
| `eqPosition` | EQ Position | 0 … 1 | 0 | 2 | yes | — |
| `dither` | Dither | 0 … 2 | 0 | 3 | **no** | — |
| `ditherShaping` | Noise Shaping | 0 … 1 | 0 | 2 | **no** | — |

¹⁵ **Re-frozen by [ADR-0015](design-decisions/ADR-0015-pre-ship-contract-refreeze.md)**
(2026-08-06, owner round-2 directive): `ceiling` −1.0 → **−0.1**, `truePeakMode` on → **off**.
Both are `PARAMETER_COMPATIBILITY_POLICY.md` rule 3 changes, taken while nothing has shipped and
recorded with the condition that closes that window. The snapshot fixture was re-frozen with
them. A Parameter Registry change is an `ARCHITECTURE_REVIEW_GATE.md` item and a Hard Stop:
**the owner cleared that gate on 2026-08-06**, signing off both defaults by name (quoted in
ADR-0015's Status banner). Neither is ⊕ any longer. A consequence worth carrying at the row: with `truePeakMode` off the ceiling is a
**sample-peak** limit (`DSP_POLICY.md` invariant 3, ADR-0006 item 3), so `ceiling`'s value text
prints `dB` and switches to `dBTP` only while the mode is engaged —
`testTheCeilingAdvertisesTheUnitItEnforces`. The **text** is not part of the snapshot (ID · name ·
range · default · steps · automatable), and both spellings parse back identically, so it is
display-only.

¹⁶ **Quantised to two decimals by [ADR-0024](design-decisions/ADR-0024-ceiling-two-decimal-precision.md)**
(2026-08-11, owner directive): `ceiling` gains `interval = 0.01` and prints `String (v, 2)`, so the
control reads `-0.10 dB` and no third-decimal value is reachable — by host automation, state
restore, preset apply, typed text or the knob alike, because `AudioParameterFloat::setValue`
resolves `convertFrom0to1` to `RangedAudioParameter`'s SNAPPING wrapper. The **steps** column above
carries the interval rather than the host-visible step count, which is unchanged:
`AudioParameterFloat::getNumSteps()` does not derive from the range, so the parameter stays
continuous to a host and `tests/fixtures/parameter_registry.snapshot` — which pins the step count —
did not need re-freezing. A range change is a rule 3 item and a Hard Stop; the owner's directive
cleared that gate, and the default −0.1 was already on the grid, so nothing moved.
`testCeilingIsQuantisedToTwoDecimals`.

²³ **Amended by [ADR-0023](design-decisions/ADR-0023-012-field-fix-contracts.md)** (2026-08-09,
owner 0.1.2 directive; gate cleared in the ADR's Status banner) — two changes, neither touching
an ID, range or default. `stereoLink`'s display NAME becomes **"Limiter Stereo Link"** (rule 2:
snapshot re-frozen, `Changed` changelog entry): beside ADR-0019's "Comp Stereo Link" a bare
"Stereo Link" was the ambiguous automation lane of the pair; the editor caption reads
"Stereo Link" in both modules — the panel says which stage. `scHpfFreq`'s SCOPE narrows to the
**comp's detector only**: the limiter's detector is unfiltered since 0.1.2 (its threshold is the
Ceiling, so its detector must track the actual peak — the shared filter both under-read bass
overs into the clamp and over-read LF transients into false reduction), and the comp's filtered
magnitude is clamped to a raw-magnitude ceiling. The §5.4 scHpf trim follows the parameter's scope.

¹⁹ **Added by [ADR-0019](design-decisions/ADR-0019-comp-stereo-link.md)** (2026-08-06, owner
0.1.1 directive item 12) — the 50th row, the first parameter ADDED since the surface froze at 49.
The comp's own stereo link: the limiter's blend applied at the comp detector
(linked = link·max + (1−link)·own, before the RMS integrator), named "Comp Stereo Link" so the
two automation lanes cannot be confused with the limiter's (which 0.1.2 renamed to
"Limiter Stereo Link" for the same reason from the other side — footnote ²³). Default 100 % IS the
fully linked single-gain glue the stage always had, so the addition is backwards-inert — an old
session or preset simply loads the default (§4.4 missing-field rule). The snapshot fixture was
re-frozen with the row; `PARAMETER_COMPATIBILITY_POLICY` rule 1 freezes the new ID from here on.

¹⁸ **Re-tiered by [ADR-0018](design-decisions/ADR-0018-copy-and-advanced-join-the-undo-history.md)**
(2026-08-06, owner 0.1.1 directive item 4, gate cleared in the ADR's Status banner): the Advanced
toggle is an **undo step** now — `advancedMode` left `isViewTierParam`, the undo/redo restore
adopts it, and every other adoption path (A/B switch, Copy) pins it to live, so a compare still
never resizes the editor. Preset exclusion is by name in `isPresetExcludedParam`. Automation and
the snapshot columns are untouched — the tier is not a snapshot column.

## The nine non-automatable rows

`advancedMode` (the X11 editor-resize crash path if it travelled), the three macros
`loudness`/`character`/`tone` (ADR-0005: replaying a macro lane would have the plugin writing
other parameters during playback), `freeze`, `lookahead` (a live read offset — sweeping it drags
the detector tap), `truePeakMode`, `dither`, `ditherShaping` (conservative v1 freezes).
`withAutomatable(false)` is **advisory** (rule 5): hosts may expose them anyway, and each behaves
sanely when written regardless. Loosening any of these is a kVersion bump + ADR.

Two automatable discrete rewires — `eqPosition` and `colourModel` — are duck-routed through the
§2.8 transition layer (landed with P2: the rewire executes at the silent bottom,
`src/dsp/AnabasisEngine.cpp` block top): a stepped automation lane produces a repeated ~34 ms
dip, which is the click-free mechanism working, not a defect (ADR-0010's same-day note).

## The lockable set

`{ceiling}` in v1. The lock state is `int_ceilingLock` in `ANABASIS_INTERNAL` — a lock is
non-musical UI state, not a parameter (ADR-0010 option J). A locked ceiling is **skipped** on
preset apply, never written-then-reverted (`src/PresetManager.cpp`). Widening the set is a
registry entry + ADR (rule 6).

## Host-hidden state (NOT parameters)

`ANABASIS_INTERNAL` fields, `int_`-prefixed, serialized with the session, invisible to host
automation by construction (out of the tree entirely — the only reliable hiding, ADR-0010 option
K). Inventory as implemented (`src/InternalState.h`): `int_oversample` (0–4 = Off/2×/4×/8×/16×),
`int_osPhase` (0 min / 1 linear), `int_offlineQuality` (0 Follow / 1 Force Max),
`int_ceilingLock`, `int_uiScale` (a percent from the XS–XL ladder, `ui_scale::steps` — the ladder narrowed from seven steps to five in round 2, which changes the field's accepted DOMAIN and so what its read rule does to a stored 80/90/175/200; **ADR-0017**, gate cleared 2026-08-06), `int_tooltipsOn`, `int_uiAnimations`,
`int_spectrumOn` (since 2026-08-05 the graph-well MODE flag — true = spectrum, false = GR history; switched by the corner chips on the graph itself, no Settings toggle. That is a **semantic change** to a serialized field, not just a UI move: it used to mean "does the spectrum take half of the Advanced strip", and the Simple view did not read it. **ADR-0016** carries the before/after; its gate was cleared 2026-08-06), `int_integratedStd` and `int_rmsRef` (**added 0.1.1**, [ADR-0020](design-decisions/ADR-0020-waveform-statistics-panel.md): which STANDARD the statistics panel's INTEGRATED row follows — BS.1770-2+ gated, the default, or BS.1770-1 ungated — and which reference its RMS row uses, AES-17 by default or mathematical. Both are display-side: the processor publishes every reading and the view selects. `int_tpMeterOn` was **removed** by the same record — the true peak is shown unconditionally now, so a field whose only job was hiding one row had nothing left to gate; `int_meterTargets` was removed 2026-08-05 with the streaming-target display under ADR-0015. An old session carrying either is ignored by the §4.4 unknown-field rule). The first three are the latency
inputs (ADR-0004); their change callbacks are three of the five PDC recompute triggers.

## Changing anything here

| Change | Cost |
|---|---|
| Display name | Snapshot re-freeze + `Changed` changelog entry (rule 2) |
| Range / default / choice order / ID / removal | **Hard Stop** — compatibility break (rules 1/3) |
| Automation flag | kVersion bump + ADR (rules 4/5) |
| Tier membership / lockable set | ADR (rule 6 — recall behaviour is contract) |
