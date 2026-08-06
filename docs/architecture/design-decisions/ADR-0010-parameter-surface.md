# ADR-0010 — Parameter surface: 49 APVTS parameters, exclusion tiers, and the lockable set

**Status:** Accepted (2026-07-31 — owner sign-off on `docs/DESIGN.md`)

## Context
`ADR_POLICY.md` makes parameter semantics ADR-mandatory (range/default/meaning of a host-visible
parameter, ID changes), and `PARAMETER_COMPATIBILITY_POLICY.md` rule 6 separately makes the
**exclusion lists** and the **lockable set** contract, because both change recall behaviour. Rule 1
freezes every ID permanently and rule 3 freezes every range, default and choice ordering on the first
shipped build — sharper here than for most plugins, since a Ceiling or Threshold range change
silently re-scales the normalised value in every saved session. That policy's own pre-0.1.0 section
therefore requires the complete table to be settled in `DESIGN.md` *before* `createAnabasisLayout`
exists (`DEVELOPMENT_BRIEF.md` §24). `DESIGN.md` §4.1–§4.3 settles it; this ADR ratifies it, which is
the point at which the surface becomes an Architecture Review Gate item rather than a draft.

The idiom is inherited from Anamorph under ADR-0009 (copy-and-adapt, provenance headers): a single
`pid::` ID namespace, `ParameterID{id, kVersion}`, formatter/parser lambdas, exact-normalised classes
for discrete parameters, an exclusion predicate shared by A/B, undo and presets, and a host-hidden
`InternalState` sibling of the APVTS. Two rows of the exclusion list are deliberate *departures* from
that precedent, and departures from a shipped product's answer are exactly what an ADR exists to
record.

## Problem
Four questions, none of them mechanical:

1. **Conventions.** What is the ID vocabulary, and how are discrete parameters declared so that a
   host session restores them to the same *index* rather than to a neighbouring one? JUCE's stock
   normalisation for choice/bool parameters is not exact under round-trip, which is a pluginval
   state-restoration failure, not a cosmetic one.
2. **Exclusion tiers.** Which parameters must not travel with an A/B compare, an undo step, or a
   preset? Anamorph ships one answer; Anabasis has a *sound-neutral* mode switch (§5.1) and an
   adaptive Freeze (§5.4) that Anamorph has no equivalent of, so its answer cannot simply be copied.
   Getting this wrong is not a preference: one of the candidates is a documented host-crash path.
3. **The lockable set.** The brief §9 requires Ceiling to be lockable at minimum. How wide is the set
   in v1, and where does the lock *state* live — is a lock itself a parameter?
4. **Membership of the APVTS.** Which controls belong in the tree at all, given that
   `withAutomatable(false)` is advisory (rule 5) and does not hide a parameter in every host?

## Options
- **A. Descriptive IDs coupled to the display wording (`pid::outputCeilingDbTp`).** Lost: rule 1
  freezes the ID forever while rule 2 leaves the display name revisable, so an ID that encodes the
  wording guarantees the two drift apart the first time copy is revised (C8 explicitly keeps launch
  wording revisable). `PARAMETER_COMPATIBILITY_POLICY.md`'s pre-0.1.0 guidance names this case.
- **B. Short camelCase IDs in one `pid::` namespace header, `ParameterID{id, kVersion=1}`, units via
  string-from-value lambdas with suffix-tolerant parsers. Chosen.** Follows
  `Anamorph:src/PluginParameters.h:14-88`, `.cpp:94-95`, `.cpp:97-103,153-194` [Verified].
- **C. Stock `AudioParameterChoice`/`AudioParameterBool` for the discrete rows.** Lost: their
  normalisation is not exact under a `getState → setState` round-trip, so a host session can restore
  `colourModel` or `limStyle` to an adjacent index. The exact-normalised `Raw*` classes
  (`Anamorph:src/PluginParameters.cpp:11-89` [Verified]) exist precisely to satisfy the pluginval
  state-restoration contract, and the registry-snapshot test makes the choice ordering enforceable.
- **D. One exclusion list for all three recall paths (A/B, undo, presets).** Lost: `freeze` must be
  excluded from presets but *present* in A/B and undo (option G), so a single list cannot express the
  surface. Two tiers computed by one shared predicate is the smallest structure that can.
- **E. `advancedMode` merely preset-excluded — Anamorph's actual answer.** Lost, and this is the
  departure worth recording. In Anamorph the Advanced toggle is not a pure view switch: it gates
  parameters to neutral at snapshot time, so toggling it changes the sound
  (`Anamorph:src/PluginParameters.cpp:326-389` [Verified]), which gives it a defensible claim on A/B
  and undo *there*. In Anabasis the switch is sound-neutral by construction
  (`MODE_AND_ADAPTATION_POLICY.md` inv 1–2, §5.1), so it has nothing to contribute to a sound
  comparison — while letting it travel means **an A/B compare or an undo step resizes the editor**,
  which is the editor-resize path Anamorph documents as crashing X11 hosts (its KI-003,
  `Anamorph:src/PluginParameters.cpp:274-281` [Verified]). A/B exists to compare sound; taking a host
  down while doing it is the whole cost, and the benefit is zero.
- **F. Move `advancedMode` out of the APVTS into `ANABASIS_INTERNAL` entirely.** Lost. The hazard in
  option E is *travel*, and the view tier already neutralises it — membership of the tree is not what
  causes the resize. Keeping it in the APVTS keeps one serialization path for it (the tree, with its
  raw-exact restore) and keeps it reachable from generic host control surfaces, at the price of a row
  in the parameter list that `withAutomatable(false)` marks and some hosts show anyway. §4.3's
  host-hidden inventory is settled by the same sign-off and does not contain it.
- **G. Fold `freeze` into the view tier (exclude it from A/B and undo too).** Lost: unlike the view
  parameters it genuinely affects the rendered sound, so reproducing a slot means reproducing
  *whether adaptation was latched*. An A/B slot that silently un-freezes is not a comparison.
- **H. Let `freeze` into presets.** Lost: a preset is a settings document, not a capture of a
  moment's adaptation — and the latched trim vector it would need to be meaningful is per-slot state
  that presets do not carry (ADR-0007).
- **I. A wider lockable set in v1 (e.g. Ceiling plus output/dither rows).** Lost: the lockable set is
  contract under rule 6, so every speculative lock freezes on first ship and needs an ADR to remove.
  The brief requires Ceiling; the mechanism is written generically, so adding a second lock later is
  a registry entry and an ADR, not new machinery.
- **J. Make the lock itself an APVTS parameter.** Lost: a lock is non-musical UI state, an automation
  lane that toggles whether preset apply moves the ceiling is a footgun, and `withAutomatable(false)`
  would not reliably hide it (option K). It lives as `int_ceilingLock` in `ANABASIS_INTERNAL`.
- **K. Keep the non-musical session state in the APVTS, hidden with `withAutomatable(false)`.** Lost:
  the flag is advisory — REAPER lists non-automatable parameters anyway
  (`Anamorph:src/InternalState.h:10-29` [Verified], Anamorph ADR-0010). The only reliable hiding is
  keeping the field **out of the tree**, so OS factor/phase, offline quality, ceiling lock and the UI
  preferences live in `ANABASIS_INTERNAL`.
- **L. Automatable macros.** Lost (§5.2): replaying a macro lane means the plugin writes other
  parameters during host playback, hosts differ on recording/feedback semantics, and offline bounces
  would depend on message-thread timing. Reversible later by kVersion bump + ADR — Anamorph ADR-0014
  is the precedent for exposing parameters after v1.
- **M. Macros out of the host's view entirely (engine-internal, like the adaptive trims).** Lost: a
  macro is the most musical control on the surface and carries full recall obligations; §4.3's
  admission criterion for host-hidden state is *non-musical*. Non-automatable ≠ non-existent.
- **N. Default `colourModel` to `Clean`.** Lost: `Clean` is the null model (§2.4), so the factory
  patch would ship with the Character macro inert. `Tape` ⊕ is the default; `colourDepth`'s default
  of 0 keeps the default patch bit-identical either way.
- **O. Reproduce the 49 rows in this ADR.** Lost: two tables of record drift. `DESIGN.md` §4.2 is the
  table of record until `PARAMETER_REGISTRY.md` exists at P1, after which the registry plus its
  snapshot fixture is the ledger and this ADR carries only the rules that govern them.

## Decision
**Surface.** Exactly **49 APVTS parameters**, as enumerated in `DESIGN.md` §4.2 — that table is the
table of record for IDs, display names, types, ranges, defaults, tapers and choice orderings, and
this ADR does not restate it. At P1 `docs/architecture/PARAMETER_REGISTRY.md` becomes the ledger and
`tests/fixtures/parameter_registry.snapshot` (behind the `--write-snapshot` gate) becomes its
enforcement; from that point the registry, not `DESIGN.md`, is cited for a row.

**Conventions** (§4.1). Short camelCase ID strings in a single `pid::` namespace header; every
parameter constructed with `ParameterID{id, kVersion}` where **`kVersion = 1`**, bumped only on a
deliberate parameter-set change (rule 4). Units are rendered by string-from-value lambdas
(`db`/`ms`/`hz`/`pct`) with suffix-tolerant parsers. Every discrete parameter — bools and choices —
uses the `Raw*` exact-normalised classes, so index round-trip is exact; this is the pluginval
state-restoration contract, not a style preference. Host-hidden fields use `int_`-prefixed
identifiers in `ANABASIS_INTERNAL` and are **not** `pid::` parameters.

**IDs are frozen at v0.1.0**, along with ranges, defaults and choice orderings (rules 1, 3). Display
names are ratified as launch wording but stay revisable under rule 2 (registry + a `Changed` CHANGELOG
entry); a rename re-freezes the snapshot deliberately.

**Automation.** Nine parameters are non-automatable: `advancedMode` (option E's crash path),
the three macros `loudness`/`character`/`tone` (option L, §5.2), `freeze`, `lookahead` (a live read
offset into the delay line — sweeping it drags the tap and produces pitch/comb artefacts; ADR-0004),
`truePeakMode`, `dither` and `ditherShaping` (conservative v1 freezes; loosening any of them is a
kVersion bump + ADR). The remaining forty are the automation surface, which is where host automation
rides in the macro architecture (`MODE_AND_ADAPTATION_POLICY.md` inv 6, ADR-0005). **The
`withAutomatable(false)` flag is advisory** (rule 5): a host may expose these anyway. Each of the nine
must therefore behave sanely when written by such a host — for the macros that means the MacroEngine
consumes changes through its async message-thread listener and offline-render determinism is not
promised for that unsupported usage (§5.2).

**Two automatable parameters are duck-routed, and that is supported-but-audible — stated so a P4
implementer does not file it as a defect** *(note added 2026-07-31, same day)*. `eqPosition` (row 45)
and `colourModel` (row 23) are `Auto: yes`, but both are genuine discrete **rewires** and are applied
through the §2.8 asymmetric raised-cosine duck (~6 ms out / ~28 ms in; ADR-0002 item 5 confirms the
position switch requests a forced duck). A host lane that *steps* either one therefore produces a
repeated ~34 ms level dip. That is not a violation of `DSP_POLICY.md` invariant 8: the invariant
requires each transition be free of **clicks, pops and level jumps**, and the duck exists precisely
to deliver that — a smooth, deliberate dip is the click-free mechanism, not a failure of it. Nor does
it make these parameters candidates for the non-automatable set: unlike `lookahead`, whose engaged
value is a live read offset that *cannot* be swept without pitch/comb artefacts, a stepped rewire is
well-behaved at every value it visits and only its **rate** is unflattering. Sweeping either at
audio rate is therefore a musical mistake, not an unsafe operation, and the honest response is to
document it rather than to remove the capability. Changing either to non-automatable after v0.1.0
would be a `kVersion` bump + ADR (rules 4–5).

**Exclusion tiers** (§4.2), computed by a **single shared predicate** (pattern
`Anamorph:src/PluginParameters.h:66-88` [Verified]):

| Tier | Members | Excluded from | Serialized |
|---|---|---|---|
| view | `bypass`, `loudnessComp`, `deltaMonitor`, `advancedMode` | A/B, undo **and** presets | with the session |
| preset-excluded | `freeze` | presets | with the session; travels in A/B and undo |

The rows list **tier membership**, which is disjoint. The **predicate** the code computes for preset
exclusion is the union `view ∪ {freeze}` — a preset skips both tiers — exactly as `DESIGN.md` §4.2
and ADR-0004 phrase it ("*Preset-excluded* adds `{freeze}`"). An earlier revision of this table gave
the second row's members as "view tier **+** `freeze`", merging the two, which made its "travels in
A/B and undo" column false for the four view-tier members it swept in: those are excluded from A/B
and undo by row 1. Only `freeze` travels. A reader building the shared predicate from the merged row
alone could have let `advancedMode` into A/B and undo — the exact X11 editor-resize crash path the
paragraph below spends its length excluding.

`advancedMode` sits in the **view** tier — the departure from Anamorph — because the switch is
sound-neutral here and letting it travel would make an A/B compare or an undo step resize the editor,
the X11 host-crash path of Anamorph's KI-003. It is still session-serialized, so a reopened session
shows the view the user left.

`freeze` stays in **A/B and undo** and is only preset-excluded, because it changes the rendered sound.
The obligation that makes that safe is discharged by ADR-0007: the **frozen trim vector travels
per-slot inside the `AB` slot** (StateSet `{params, presetName, baseline, frozenTrims, detachMask}`),
restored **at the forced duck's silent bottom** — by a transport OQ-013 has not yet fixed, **not** by
the singular "sentinel-atomic inject" an earlier revision of this sentence named *(corrected
2026-07-31, same day: the precedent carries one float, the vector is four scalars, and an ADR
outranks the policy, so that wording would have licensed the path the OQ-013 Hard Stop blocks)* — so
switching to a frozen slot restores *that slot's* latched trims instead of re-latching whatever the
engine holds. What this ADR needs is the **per-slot** property, which holds under either candidate
transport. A global trim vector would make `freeze`-in-A/B unsound and is rejected there.

**Lockable set = `{ceiling}`** in v1; the mechanism is **generic** (a per-parameter lock consulted by
preset apply, not a `ceiling` special case). When engaged, preset apply captures and re-asserts the
locked parameter exactly as it does a view parameter, so browsing presets never moves a locked
ceiling. **Lock state is host-hidden**: `int_ceilingLock` in `ANABASIS_INTERNAL`, session-persistent,
never in A/B, undo or presets.

> **Amended by [ADR-0015](ADR-0015-pre-ship-contract-refreeze.md) (2026-08-06).** The inventory
> below is the surface **as decided on 2026-07-31** and is left standing as the record of that
> decision. Two of its numbers no longer describe the code: `int_meterTargets` was **removed**
> from the schema with the streaming-target display, so the host-hidden set is **nine** fields,
> not ten; and the `ceiling` / `truePeakMode` defaults quoted in §Decision moved to −0.1 and
> **off**. `docs/architecture/PARAMETER_REGISTRY.md` and
> `docs/architecture/SERIALIZATION_REGISTRY.md` are the descriptive ledgers of what the code
> holds today; ADR-0015 carries the reasoning and the migration.

**Host-hidden state** (§4.3) is the ten `int_` fields listed there — `int_oversample`, `int_osPhase`,
`int_offlineQuality`, `int_ceilingLock`, `int_uiScale`, `int_tooltipsOn`, `int_uiAnimations`,
`int_spectrumOn`, `int_meterTargets` (removed 2026-08-05, ADR-0015), `int_tpMeterOn`. They are out of the APVTS **because
`withAutomatable(false)` does not hide a VST3 parameter in every host** (REAPER lists them all), so
the only reliable hiding is absence from the tree (Anamorph ADR-0010). OS factor and phase drive the
DSP through an atomic mirror plus an `onChanged` → PDC-recompute callback (ADR-0003/ADR-0004); all ten
persist with the session so offline renders reproduce, and none participates in A/B, undo or presets.

## Consequences
- The surface is contract from this date. An ID rename or removal, a range/default/choice-ordering
  change, a move between exclusion tiers, or a change to the lockable set is a **Hard Stop** under
  `ARCHITECTURE_REVIEW_GATE.md` and needs a superseding ADR — a green build does not clear it.
- The registry snapshot test is what makes rule 1 automatic rather than aspirational, so it is a P1
  deliverable alongside `createAnabasisLayout`, not a later hardening step.
- An A/B compare and an undo step can never resize the editor, and can never silently change which
  view is on screen. The cost is that A/B does not carry the view, which is deliberate.
- A frozen A/B slot is bit-repeatable across switches only because ADR-0007's per-slot `frozenTrims`
  exists; the two ADRs are a matched pair, and reversing either breaks
  `MODE_AND_ADAPTATION_POLICY.md` inv 3.
- Presets never capture a latched adaptation state, so a preset loaded onto running material adapts
  from that material rather than from the preset author's.
- Ranges were chosen generously once (rule 3's advice) rather than to look right today; the price is
  coarser knob resolution on some controls, which is the cheaper error.
- With all defaults **and no processing engaged** the chain is bit-exact identity (`DSP_POLICY.md`
  inv 7 as conditioned): near-full-scale material engages the ⊕ −1 dBTP ceiling and the comp knee by
  design, so `testNullWithDefaults` must choose its stimulus level accordingly.
- Forecloses: descriptive IDs, stock discrete-parameter classes, a single exclusion list, automatable
  macros, an automatable or APVTS-resident lock, non-musical state inside the tree, and a wider v1
  lockable set. Each needs a superseding ADR.
- `lookahead` has no zero/off position and the reported latency does not follow it — both settled by
  ADR-0004, and the non-automatable flag here rests on the delay-line argument, not on latency.

## Related code
None yet — P1 onward. Planned: `src/PluginParameters.{h,cpp}` (`pid::` IDs, layout, `Raw*` classes,
formatters/parsers, exclusion predicate, `toEngine()` snapshot builder), `src/InternalState.h`
(`ANABASIS_INTERNAL`, `int_ceilingLock`), `src/PluginProcessor.{h,cpp}` (APVTS ownership, A/B, undo,
state), `src/PresetManager.{h,cpp}` (lock-aware apply, preset exclusion), `src/MacroEngine.{h,cpp}`
(non-automatable macro consumption), `src/AbSlotIndex.h`, `src/gui/PluginEditor.{h,cpp}` (`advancedMode`
view switch), `src/dsp/EngineParameters.h` (POD snapshot), `src/dsp/AdaptiveEngine.{h,cpp}`
(`freeze`, frozen trims).

Evidence [Unverified] — no `src/` exists; every Anabasis runtime claim above is the contract the code
must satisfy, not an observation (C2: no number here is a measurement):
- Design: `docs/DESIGN.md` §4.1 (conventions), §4.2 (the 49-row table of record, exclusion tiers,
  ceiling lock, footnotes 1–6), §4.3 (`ANABASIS_INTERNAL` inventory and rationale); supporting §5.1
  (mode switch is sound-neutral), §5.2 (macro automatability, advisory flag), §5.4 (Freeze latches
  the trim vector), §4.4 + §7 (per-slot StateSet, preset content), §1.3 (planned module inventory),
  §10 row 0010.
- Research: `worklogs/2026-07-30-p0-anamorph-research.md`
- Anamorph precedent [Verified]: `Anamorph:src/PluginParameters.h:14-88` (`pid::` namespace, layout),
  `:66-88` (shared exclusion predicate); `Anamorph:src/PluginParameters.cpp:94-95` (kVersion),
  `:97-103,153-194` (unit formatters and suffix-tolerant parsers), `:11-89` (`Raw*` exact-normalised
  discrete classes), `:274-281` (view toggle non-automatable, KI-003 editor-resize crash),
  `:326-389` (Anamorph's Advanced-mode gating — why its `advancedMode` affects sound and Anabasis's
  does not); `Anamorph:src/InternalState.h:10-29` (REAPER shows non-automatable parameters; host
  hiding requires absence from the tree); Anamorph ADR-0010 (host-hidden `InternalState`), Anamorph
  ADR-0014 (parameters exposed later via kVersion bump).
- Related Anabasis ADRs: ADR-0003 (host-hidden OS controls), ADR-0004 (`lookahead` non-automatable,
  no zero position), ADR-0005 (macro layer, detach/re-engage), ADR-0007 (per-slot `frozenTrims` and
  `detachMask`; preset content), ADR-0009 (copy-and-adapt provenance for these idioms).
