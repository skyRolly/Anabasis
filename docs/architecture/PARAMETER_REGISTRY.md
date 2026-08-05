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
  parsers.

## The 49 rows

Range/default/steps/automation are the snapshot's columns verbatim ("cont." = a continuous range,
`getNumSteps()`'s sentinel). Tier: **view** = excluded from A/B, undo *and* presets;
**preset-excl** = excluded from presets only (travels in A/B and undo). Both are computed by the
one shared predicate pair `isViewTierParam` / `isPresetExcludedParam`
(`src/PluginParameters.cpp:236-245`) — a preset skips the union, view ∪ {freeze}.

| ID | Name | Range | Default | Steps | Auto | Tier |
|---|---|---|---|---|---|---|
| `bypass` | Bypass | 0 … 1 | 0 | 2 | yes | view |
| `advancedMode` | Advanced | 0 … 1 | 0 | 2 | **no** | view |
| `loudness` | Loudness | 0 … 100 | 0 | cont. | **no** | — |
| `character` | Character | 0 … 1 | 0 | cont. | **no** | — |
| `tone` | Tone | -1 … 1 | 0 | cont. | **no** | — |
| `ceiling` | Ceiling | -20 … 0 | -0.1 | cont. | yes | — |
| `freeze` | Freeze | 0 … 1 | 0 | 2 | **no** | preset-excl |
| `loudnessComp` | Loudness Comp | 0 … 1 | 0 | 2 | yes | view |
| `deltaMonitor` | Delta | 0 … 1 | 0 | 2 | yes | view |
| `inputGain` | Input Gain | -12 … 24 | 0 | cont. | yes | — |
| `scHpfFreq` | SC HPF | 20 … 300 | 20 | cont. | yes | — |
| `compRatio` | Comp Ratio | 1.1 … 4 | 1.5 | cont. | yes | — |
| `compThreshold` | Comp Threshold | -40 … 0 | 0 | cont. | yes | — |
| `compAttack` | Comp Attack | 5 … 100 | 30 | cont. | yes | — |
| `compRelease` | Comp Release | 50 … 1000 | 200 | cont. | yes | — |
| `compAutoRelease` | Comp Auto Rel | 0 … 1 | 1 | 2 | yes | — |
| `compKnee` | Comp Knee | 0 … 12 | 6 | cont. | yes | — |
| `compDetector` | Comp Detector | 0 … 1 | 0 | 2 | yes | — |
| `compMix` | Comp Mix | 0 … 100 | 100 | cont. | yes | — |
| `clipShape` | Clip Shape | 0 … 1 | 0.5 | cont. | yes | — |
| `clipDrive` | Clip Drive | 0 … 24 | 0 | cont. | yes | — |
| `clipMix` | Clip Mix | 0 … 100 | 100 | cont. | yes | — |
| `colourModel` | Colour | 0 … 3 | 1 | 4 | yes | — |
| `colourBalance` | Odd/Even | -1 … 1 | 0 | cont. | yes | — |
| `colourTone` | Colour Tone | -1 … 1 | 0 | cont. | yes | — |
| `dynTilt` | Dynamic Tame | 0 … 2 | 0 | cont. | yes | — |
| `colourDepth` | Colour Depth | 0 … 100 | 0 | cont. | yes | — |
| `limGain` | Limiter Gain | 0 … 18 | 0 | cont. | yes | — |
| `lookahead` | Lookahead | 0.5 … 10 | 2 | cont. | **no** | — |
| `limRelease` | Lim Release | 1 … 1000 | 100 | cont. | yes | — |
| `limAutoRelease` | Lim Auto Rel | 0 … 1 | 1 | 2 | yes | — |
| `limStyle` | Style | 0 … 2 | 0 | 3 | yes | — |
| `stereoLink` | Stereo Link | 0 … 100 | 100 | cont. | yes | — |
| `transientPreserve` | Transients | 0 … 100 | 50 | cont. | yes | — |
| `truePeakMode` | True Peak | 0 … 1 | 0 | 2 | **no** | — |
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
`int_ceilingLock`, `int_uiScale` (80–200 %), `int_tooltipsOn`, `int_uiAnimations`,
`int_spectrumOn`, `int_meterTargets` (bitmask), `int_tpMeterOn`. The first three are the latency
inputs (ADR-0004); their change callbacks are three of the five PDC recompute triggers.

## Changing anything here

| Change | Cost |
|---|---|
| Display name | Snapshot re-freeze + `Changed` changelog entry (rule 2) |
| Range / default / choice order / ID / removal | **Hard Stop** — compatibility break (rules 1/3) |
| Automation flag | kVersion bump + ADR (rules 4/5) |
| Tier membership / lockable set | ADR (rule 6 — recall behaviour is contract) |
