# ADR Index

Mandatory registry of all Architecture Decision Records for **Anabasis**. An ADR not listed here
is invalid.

ADRs are created **only** when supported by repository evidence (constraint C1) — the table below
is the evidence-confirmed result, not a predefined quota. New decisions append the next number.

Numbering is **local to this repository**: Anabasis ADR-0001 is unrelated to Anamorph ADR-0001.

Status values: Proposed · Accepted · Deprecated · Superseded.

| ID | Title | Status | Evidence confidence |
|---|---|---|---|
| [ADR-0001](ADR-0001-format-agnostic-dsp-core.md) | Format-agnostic DSP core via a POD `EngineParameters` snapshot | Accepted | Verified — `AnabasisDSP` links only `juce_dsp`/`juce_audio_basics` (CMake) and the whole `AnabasisTests` target builds without a wrapper header |
| [ADR-0002](ADR-0002-serial-signal-chain-and-clamp-placement.md) | Fixed serial signal chain; EQ Pre/Post the only mobility; **ceiling clamp always last before dither** | Accepted | Verified — `testLimiterPushDoesNotDriveTheClipper`, `testEqPositionsAreDistinct`, `testOutputNeverExceedsCeiling` in both EQ positions |
| [ADR-0003](ADR-0003-oversampling-scope-and-true-peak-measurement-tap.md) | Oversampling wraps Clip/Sat + Limiter; **true peak is a measurement tap** at >= 4x total | Accepted | Verified — `testTruePeakAccuracy`, `testLimiterTruePeakMode`, and `testOsLatencyMatrix` (the tap adds no delay) |
| [ADR-0004](ADR-0004-latency-contract-constant-lookahead-allowance.md) | Latency contract: reported = **constant max-lookahead allowance** + OS; no zero-lookahead position | Accepted | Verified — `testReportedLatencyMatchesImpulse` (every lookahead value), `testOsLatencyMatrix` (whole factor × phase matrix incl. Force-Max) |
| [ADR-0005](ADR-0005-macro-layer-architecture.md) | Macro layer: message-thread mapper, non-automatable macros, detach/re-engage coexistence | Accepted | **Verified** (P5, 2026-08-02) — `testMacroDefaultIsFixedPoint`, `testModeSwitchIsSoundNeutral`, `testDrainInsideRestoreIsSuppressed` pin the mapper and the restore; `testDetachAndReengageGrammar` pins the detach/re-engage gesture grammar (all three discriminator conditions, the mapping skip, re-engage-on-gesture, reset-to-macro, and the two mid-gesture overlap cases — each condition killed by its own mutant) |
| [ADR-0006](ADR-0006-ceiling-guarantee.md) | Ceiling guarantee: separate final clamp; monitoring never in the render path | Accepted | Verified — `testOutputNeverExceedsCeiling`, `testLoudnessCompensationDoesNotAlterRender`, `testDeltaMonitor` |
| [ADR-0007](ADR-0007-state-schema-v1.md) | State schema v1: explicit `schemaVersion`, raw-exact sessions, **per-slot adaptive state** | Accepted | **Verified** (2026-08-02) — byte-identical round trip, the §4.4 read rules and the ADAPTIVE child are pinned by `AnabasisStateTests`; the FROZEN_TRIMS inject half is wired by ADR-0014 and pinned by `testFrozenTrimRestore` |
| [ADR-0008](ADR-0008-build-architecture-and-plugin-identity.md) | Build architecture + plugin identity: CMake graph, JUCE 9.0.0 SHA pin, C++20, `RTec`/`Anbs` | Accepted | Verified — the five-target graph configures and builds green at the pinned SHA on Linux, pluginval L5 ×3 both modes |
| [ADR-0009](ADR-0009-code-reuse-from-anamorph.md) | Code reuse from Anamorph: copy-and-adapt with provenance, no shared module for v1 | Accepted | Verified — the one adapted unit (K-weighting, provenance in `LoudnessMeter.h`) reproduces the standard's −3.01 LKFS vector in `testLufsCalibration`; no shared module exists |
| [ADR-0010](ADR-0010-parameter-surface.md) | Parameter surface: 49 APVTS parameters, exclusion tiers, lockable set | Accepted | Verified — the frozen registry snapshot plus the count checks in `AnabasisStateTests`, which assert **50**/9 today: this ADR froze the surface at 49 and **ADR-0019** added `compStereoLink` on top of it, so the title's 49 is the decision as taken and 50 is what the suite pins (`PARAMETER_REGISTRY.md` carries the live row set) |
| [ADR-0011](ADR-0011-threading-model.md) | Threading model: two threads, no workers, atomic + SPSC publication | Accepted | **Partially Verified** — every implemented edge is mapped in `THREAD_MODEL.md` and `REALTIME_SAFETY_AUDIT.md` audits the audio thread; the two questions it left open are OQ-014 and the off-thread `replaceState` race (KI-003), and ADR-0012 amends its permitted-path table |
| [ADR-0012](ADR-0012-staged-record-cross-thread-path.md) | GUI→Audio **bounded staged record** behind a release/acquire flag (ratifies the learned-target restore, OQ-015) | Accepted | Verified |
| [ADR-0013](ADR-0013-release-trim-reaches-auto-poles.md) | The §5.4 release trim scales the limiter's **AUTO release poles** by `2^octaves` (resolves OQ-016) | Accepted | Verified — `testAutoReleaseFollowsTheTrimScale`, mutation-verified against the fixed-constant alphas |
| [ADR-0014](ADR-0014-frozen-trim-restore.md) | Frozen trim vector **restored**: staged on ADR-0012's row, applied at the §2.8 duck's silent bottom (resolves OQ-013) | Accepted | Verified — `testFrozenTrimRestore`, every element killed by its own mutant (the ADR enumerates them) |
| [ADR-0015](ADR-0015-pre-ship-contract-refreeze.md) | **Pre-ship contract re-freeze**: the round-2 `ceiling`/`truePeakMode` defaults, the `int_meterTargets` removal, and the Ceiling's mode-aware unit | Accepted | Verified — `testTheCeilingAdvertisesTheUnitItEnforces`, the re-frozen `testRegistrySnapshot`, `testFactoryPresets` and the §4.4 read-rule checks |
| [ADR-0016](ADR-0016-spectrumon-becomes-the-graph-well-mode.md) | `int_spectrumOn` repurposed from "is the spectrum shown" to **which graph-well view is active** — a serialization **semantic** change; **gate cleared 2026-08-06** | Accepted | Verified — `testTheGraphWellViewsOnlyClaimTheirModeChips`; the pre-change behaviour is tabulated in the ADR from the commit that held it |
| [ADR-0017](ADR-0017-uiscale-ladder-narrowing.md) | `int_uiScale`'s legal value set narrows 7 steps → 5; out-of-set stored values converge at adoption — a serialization **domain** change, recorded at the gate's bar; **gate cleared 2026-08-06** | Accepted | Verified — `testAnOutOfListUiScaleClampsConsistently`; the pre-change ladder is quoted in the ADR from the commit that held it |
| [ADR-0018](ADR-0018-copy-and-advanced-join-the-undo-history.md) | Copy becomes a destination-slot undo step that KEEPS the destination's history; the Advanced toggle joins the undo history (A/B-pinned) — partially supersedes ADR-0010 option E; **gate cleared 2026-08-06** (owner's 0.1.1 directive item 4 + standing sign-off instruction) | Accepted | Verified — `testTeardownAndReengageInvariants` cases (3)/(5), `testAbSlotsAndTiers`'s ADR-0018 checks |
| [ADR-0019](ADR-0019-comp-stereo-link.md) | `compStereoLink` — the comp's stereo link becomes adjustable, the limiter's blend at the comp's detector; the 50th parameter, additive and backwards-inert (default 100 % = the shipped fully-linked glue, bit-for-bit); **gate cleared 2026-08-06** (owner's 0.1.1 directive item 12) | Accepted | Verified — `testCompStereoLink`, the re-frozen `testRegistrySnapshot` (50/9), `testCachedParamsMapping` |
| [ADR-0020](ADR-0020-waveform-statistics-panel.md) | The **Waveform Statistics** panel: eight rows in both views (new 50 ms Hann `RmsMeter`, LRA and the BS.1770-1 ungated integrated reading in `LoudnessMeter`, the sample-peak hold published); `int_tpMeterOn` **removed**, `int_integratedStd` / `int_rmsRef` **added** — a serialization change in both directions; **gate cleared 2026-08-06** (owner's 0.1.1 directive item 14) | Accepted | Verified — `testRmsMeterReadsTrueLevels`, `testLoudnessRangeAndTheUngatedReading` (two constants mutation-verified), `testTheWaveformStatisticsRowsReadTheirStandards` |
| [ADR-0021](ADR-0021-release-pipeline-and-artifact-parity.md) | The **release pipeline and installer set land at 0.1.1** (packaging/ ported, ISCC + `.pkg` steps in `build.yml`, `release.yml` with its fail-closed validate→build→draft chain); third-party attribution moves to **version-named release-page assets** as its sole carrier — amends `RELEASE_POLICY.md` and supersedes the OQ-007 deferral; **gate cleared 2026-08-07** (owner's 0.1.1 directive item 16) | Accepted | Verified — CI run 31135082913 built BOTH installers (ISCC produced `Anabasis-0.1.1-Windows-Installer.exe`; `build-pkg.sh` and its self-checks passed on the macOS runner). Two named gaps: the Linux scripts have not been run as root, and `release.yml` awaits the first tag |
| [ADR-0022](ADR-0022-preset-identity.md) | **Preset identity** (the ADR-0009 product-family port of Anamorph's ADR-0024 as amended): factory presets gain immutable internal ids, a user preset is identified by its file; the menu tick and `‹ ›` stepping resolve identity first — a known identity absent from the list ticks **nothing**, the name fallback covers only identity-less state — and the identity travels on the `SLOT` unit as **three additive serialized strings**, so undo, A/B, Copy and the session inherit it from the existing slot plumbing; user preset files are untouched; **gate cleared 2026-08-08** (owner's written approval of the migration plan, quoted in the Status banner) | Accepted | Verified — `testPresetIdentitySharedName`, `testFactoryPresetIdIntegrity`, `testPresetIdentityAcrossRestore` (fallback matrix, per-slot identity, bit-identical parameters on every path) |
| [ADR-0023](ADR-0023-012-field-fix-contracts.md) | The **0.1.2 field-fix contracts**: comp knee moves ABOVE the threshold (zero gain at or below `T` — the all-defaults null holds for any sub-ceiling input); the limiter's detector is **unfiltered** (`scHpfFreq` becomes comp-only, its filtered magnitude clamped to a raw-magnitude ceiling — a sidechain HPF may only deafen a detector); bus layouts gain **mono→mono**; the GR history draws at a fixed right-anchored scale with a zero-data unmeasured region and survives same-config re-prepares; the GR\|SPEC pill moves bottom-left as a whole-pill toggle with GR the default view; editor captions drop stage prefixes while the limiter's registry NAME becomes "Limiter Stereo Link" (ID unchanged, snapshot re-frozen); the Advanced macro row is removed (940×822); per-channel per-stage GR is published (the KI-009 disambiguator); **gate cleared 2026-08-09** (the owner's 0.1.2 release directive, quoted in the Status banner) | Accepted | Verified — `testNullWithDefaults` (bit-exact null + Delta silence at defaults), `testCompStaticCurve`, `testLimiterDetectorIsUnfiltered`, the fixed-scale `grBuckets` section, `testGrRingResetEpoch`, the graph-switch click tests, `testMeterPublication`'s per-channel lanes, `testBothChannelsCarryAudioThroughTheWrapper` (mono→mono + six diagnostic configurations); eight mutants across two control builds, each killed by its own assertion |
| [ADR-0024](ADR-0024-ceiling-two-decimal-precision.md) | The **Ceiling is quantised to two decimal places in the RANGE, not the label**: `interval = 0.01`, which is sufficient because `AudioParameterFloat::setValue` resolves `convertFrom0to1` to `RangedAudioParameter`'s SNAPPING wrapper (`range.snapToLegalValue (range.convertFrom0to1 (…))`), so host automation, state restore, preset apply, typed text, the editor knob and the value `EngineParameters::ceilingDbTp` carries all land on the same grid; display becomes `String (v, 2)`. Rule 3 of PARAMETER_COMPATIBILITY_POLICY applies (a stored off-grid value snaps on load; nothing has shipped, so no migration). The default -0.1 is already on the grid and the registry snapshot is unchanged; **gate cleared 2026-08-11** (the owner's directive, quoted in the Status banner) | Accepted | Verified — `testCeilingIsQuantisedToTwoDecimals` (a 998-step raw normalised sweep, the DSP-visible atomic, typed text in both rounding directions, a bit-identical save/load round trip, the default on the grid); mutation-verified against the original continuous range, which fails five of its assertions |
| [ADR-0025](ADR-0025-regression-test-exception.md) | A **narrow, disclosure-bound exception to TESTING_POLICY rule 1**: a fix may ship without a suite regression test ONLY where the suites structurally cannot produce the defect — real pointer/keyboard interaction inside a modal loop, an artifact a platform packaging tool writes, or behaviour owned by the OS — and only if its record states four things beside itself: the mechanism that makes a test impossible, what was verified instead, what is consequently unprotected, and when the exception lapses. Does NOT cover DSP, state, serialization or the parameter surface; a test that passes on both old and new code is explicitly not compliance (the INC-004 failure mode). Accepted 2026-08-13 under the standing blanket approval | Accepted | Not a code decision — compliance is checkable: INC-005, KI-013 and KI-014 each carry all four disclosures |
| [ADR-0026](ADR-0026-slot-payload-read-rules.md) | **A `SLOT` carrying no `ANABASIS` child resolves to defaults as a whole, and the ACTIVE slot's metadata is adopted only when the ROOT surface was restored** — one rule per slot, reached by two different routes because the slots are not symmetric: metadata is adopted only alongside the parameters it describes. Closes a defect where a payload-less stored slot put one session's preset name and identity on another session's sound. No producer exists for either shape (`getStateInformation` always writes the root surface and both payloads), so the compatibility exposure is bounded to hand-edited, truncated or foreign blobs — which does NOT clear the gate. **⊕ NOT RATIFIED — the ARCHITECTURE REVIEW GATE IS OPEN**: implemented in 0.1.4 without being flagged as gated, found by review 2026-08-13 | Proposed | Verified — `testAMalformedStoredSlotCannotSplitSoundFromMetadata` (mutant reproduces the defect verbatim), `testARootlessSurfaceDropsTheActiveSlotsMetadataToo` |

ADR-0015 was taken on **2026-08-06**, ratifying contract changes the owner's round-2 directive
(2026-08-05) had already landed in PR #8 — the same ratification shape as ADR-0012/0013/0014, and
carrying `Verified` confidence for the same reason. It is the first ADR whose Context records that
the code preceded its own authority; the ADR says why that is not a precedent. Two of its three
changes are `ARCHITECTURE_REVIEW_GATE.md` items (a Serialization Registry change and a Parameter
Registry change), and the **owner cleared that gate explicitly on 2026-08-06**, naming all three —
the `int_meterTargets` removal, the `ceiling` default and the `truePeakMode` default. The
sign-off is quoted in the ADR's own Status banner, which is the record of authority; this row
exists so the index answers "was the gate cleared?" without opening the file.

**ADR-0016 was cleared the same day, separately and on its own terms** — the owner's confirmation
names the semantic change, the decision to keep it a pre-1.0 migration change, and the acceptance
that stored values load with no migration path. It is a **separate** ADR rather than a fourth item
inside ADR-0015 for the reason this index exists to make visible: that record was signed off
naming three different changes, and widening a signed-off record after the fact is the failure
mode, not the shortcut.

**ADR-0017 was cleared the same day, and separately again.** `int_uiScale`'s ladder narrowed from
seven steps to five in the same batch, changing the field's accepted domain and therefore what its
documented read rule does to a stored 80/90/175/200. The owner's confirmation names the reduced
ladder, the acceptance that out-of-set stored values normalise on adoption, and that this is a
pre-1.0 decision with no released-session migration obligation.

**All three gated records of the round-2 batch are therefore cleared, and each on its own terms:
ADR-0015, ADR-0016, ADR-0017.** They were kept as three records rather than one precisely so that
sentence can be true — a single record would have made the second and third clearances look like
extensions of the first, which is the widening this index exists to make visible.

## ADRs amended by a later ADR

An ADR is a historical record and is **not** rewritten when a later decision moves one of its
numbers. Where the later ADR only supersedes *part* of an earlier one, the earlier record keeps its
`Accepted` status and its original text, and carries an amendment banner pointing forward. This is
the registry of those, so a reader auditing an ADR knows before reading it whether any of it has
moved:

| Amended | By | What moved |
|---|---|---|
| **0006** | **0015** | The `ceiling` (⊕ −1.0) and `truePeakMode` (⊕ on) defaults quoted in its Context and option E. The clamp mechanism, the stage placement and the monitoring decisions are untouched. |
| **0010** | **0015** | The ten-field host-hidden inventory in its Decision — `int_meterTargets` was removed, so the set is nine — and the same two defaults. The IDs, the exclusion tiers and the lockable set are untouched. |
| **0010** | **0018** | Option E's *undo* half: the Advanced toggle is now an undo step (owner's 0.1.1 directive). Its A/B half — a compare never resizes the editor — is reaffirmed, enforced by `applySlotToLive`'s pin instead of tier membership; `advancedMode` left `isViewTierParam` and is preset-excluded by name. Non-automatability untouched. |

ADR-0013 and ADR-0014 were taken on **2026-08-02** under the owner's v0.1.0 blanket approval
(every human-review/owner-decision gate pre-approved to unblock the complete first version, with
the item-by-item fine review owed afterward). Both are flagged ⊕ accordingly; both carry
`Verified` confidence at authoring for the same reason ADR-0012 did — they ratify mechanisms in
the tree with mutation-verified tests.

ADR-0012 was taken on **2026-08-01** (owner decision on OQ-015, during P4) and carries
`Verified` confidence at authoring — unusually for a fresh ADR, because it ratifies a mechanism
already in the tree with mutation-verified tests, which is precisely why ratification was the
cheap option. It is the first ADR authored after the sign-off batch.

The first eleven were authored on **2026-07-31**, the date of the owner's sign-off on `docs/DESIGN.md`
(the P0 exit criterion, `DEVELOPMENT_BRIEF.md` §11). They were authored `Unverified` by
construction — at that date Anabasis had no `src/` and no `tests/`, so every runtime claim was a
contract the P1+ code had to satisfy rather than an observation. **That is no longer the state:**
P1–P4 landed the full chain and its two suites, and the confidence column below is upgraded per
ADR **against a named test**, never wholesale. An ADR reads `Partially Verified` where part of its
decision is still unwired — the row says which part.

That includes ADR-0008: its JUCE pin and identity codes were read from the **sibling** repository,
which is evidence about how *Anamorph* builds, not about how Anabasis does. "This pin configures
and builds here" becomes `Verified` at the first green P1 build and not before
(`DOCUMENTATION_COVERAGE.md`: never upgrade a confidence level without the evidence that justifies
it).

**Five of them amended a Policy** (`ADR_POLICY.md` rule 5 — a policy change is enacted by an ADR).
Three carry the amendment inside a decision item, two in a dedicated
*Policy amendments enacted by this ADR* section; both forms are equally binding, and this list is
the registry of record for "which rules were rewritten, and by what authority":

| ADR | Policy amended | What changed |
|---|---|---|
| **0002** | `DSP_POLICY.md` invariant 1 | Chain order prints the ceiling clamp as always-last-before-dither, in both EQ positions. **Hard Stop**, ratified at sign-off. |
| **0003** | `DSP_POLICY.md` invariants 2 and 5 | Closes the open point: true-peak detection is a measurement tap, so "oversampling off ⇒ no oversampling latency" holds unconditionally. |
| **0004** | `DSP_POLICY.md` invariants 2 and 8 | Reported latency is the constant lookahead allowance + OS; the latch sentence names factor **and** phase mode; invariant 8's click-free enumeration gains the phase mode, the lookahead, the colour model and the three named bulk swaps (preset load / A/B switch / undo step, each owed its own click test). **Hard Stop**, ratified at sign-off. |
| **0005** | `MODE_AND_ADAPTATION_POLICY.md` invariants 1, 4, 5 and 6; `PARAMETER_COMPATIBILITY_POLICY.md` rule 7 | Invariant 1 gains a named guard with its binary; invariant 4's lookahead bar is re-grounded off the lapsed latency derivation; invariants 5 and 6 are rewritten from "open decision" wording to the settled coexistence strategy and macro non-automatability; rule 7's macro-curve freeze is re-grounded on **recall**, not automation. (Only 1 and 4 are carried as prescribed verbatim blocks — 5 and 6 reproduce this ADR's own decision text.) |
| **0011** | `THREADING_POLICY.md` | The adaptive-engine placement clause stops deferring to a future ADR; a seventh permitted-path row is added for single-scalar sentinel commands; the PDC rule is restated as *off the audio thread* rather than the unsatisfiable *on the message thread*. |

An earlier revision of this paragraph said "three", counting only the `DSP_POLICY.md` amendments and
missing ADR-0005's and ADR-0011's entirely — which would let a reader auditing the sign-off conclude
that two rewritten rule documents had been changed without authorisation.

## Expected first batch (from P0 `DESIGN.md`) — delivered 2026-07-31

Retained as the record of what P0 set out to decide. Every item below is now an Accepted ADR
above; the list is no longer forward-looking.

- The fixed serial signal-chain order and the EQ Pre/Post placement (`DEVELOPMENT_BRIEF.md` §3).
- Oversampling strategy: which stages are wrapped, filter type, and the exact latency contract
  (§2, §4.3, §7).
- The true-peak ceiling guarantee: detector oversampling, the final safety clamp, and its
  tolerance (§4.3, §10).
- The Simple macro-layer architecture and the Simple ⇄ Advanced coexistence strategy
  (§5.3 / OQ-004).
- The state-schema shape: versioned root, subtree split, host-hidden session state (§2).
- The JUCE pin (version + immutable commit SHA) and the C++20 baseline (§2, §2.1 / OQ-001).
- The plugin identity codes and format set (§2 / OQ-003).
- **Code reuse from Anamorph** — which first-party GUI, wrapper/state and DSP sources are copied
  and adapted, and whether a shared module is extracted (`CLAUDE.md` §3 makes this ADR-mandatory;
  OQ-005).
- **The parameter surface** — IDs, ranges, defaults, choice orderings, the exclusion tiers and the
  lockable set (`ADR_POLICY.md` parameter semantics; `PARAMETER_COMPATIBILITY_POLICY.md` rule 6).
- **The threading model** (`ADR_POLICY.md`).

`docs/DESIGN.md` §10 carries the same mapping. Since sign-off this index — not that table — is
the registry of record (`ADR_POLICY.md` rule 1).

## How to add an ADR

1. Confirm the decision is backed by code/test/commit/PR evidence.
2. Use the field structure: **Status**, Context, Problem, Options, Decision, Consequences,
   Related code, Evidence + confidence. **Record the rejected options and why they lost** —
   with measurements where the decision was empirical (`ADR_POLICY.md`).
3. Assign the next sequential number; add a row here.
4. If the ADR changes a Policy or another ADR, mark the superseded record `Superseded`/`Deprecated`
   and cross-link.

## Template

```markdown
# ADR-NNNN — <decision in one line>

**Status:** Proposed | Accepted | Deprecated | Superseded by ADR-MMMM

## Context
<the situation that forced a decision>

## Problem
<what specifically had to be decided>

## Options
- **A. <option>** — <consequence>
- **B. <option>** — <consequence>. Chosen.

## Decision
<what was decided, precisely enough to implement against>

## Consequences
<what this costs, what it forecloses, what now depends on it>

## Related code
- `src/...:NN-MM`

Evidence [Verified | Partially Verified | Unverified]:
- Source: <file:lines>
- Test:   <test name>
- Worklog: worklogs/<file>.md   # the measurement trail, if the decision was empirical
```
