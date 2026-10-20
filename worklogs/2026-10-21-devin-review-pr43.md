# 2026-10-21 — PR #43: Devin review and merge reconciliation

Merge order remains **#43 then #42, both with merge commits**, with no tag between
them. Starting heads: #43 `d2c1c59`, #42 `cee7bdd`, main `4252f35`.
Findings are judged against the composed result, not Phase 0 alone.

## DSP findings

| Finding on #43 | Repository evidence in #42 | Disposition |
|---|---|---|
| Falling Ceiling uses an old limit | `dce072b`, completed by `4ff71bd` / `f03d673`: `AnabasisEngine::process` predicts the emission ceiling and revises in-flight requirements through `CeilingClamp::lowerInFlightCeilings`; ADR-0045 / ratified ADR-0046. Guards: `testTruePeakModeHoldsTheCeilingUnderAutomation`, `testTruePeakModeLagsAnAscentByTheEntryCeiling`, `testTruePeakModeBoundsTheStepAtACeilingCut`. | Keep the fix in #42; do not duplicate the stacked change. |
| Offline entry inherits `engageTail` | `74c114e`: direct adoption resets the tail before stage E and resets the output estimator when a tail is cut. Guard: `testOfflineEntryDropsTheEngagementTail`. | Keep the fix in #42. |
| Low-rate static true peaks overshoot | `4ff71bd` / `f03d673`: eased attack, 16-sample floor and bounded release rise. Ratified ADR-0046 engages the dBTP path from 12 kHz; lower rates use the sample clamp and display `dB`. Guards: `testTruePeakModeHoldsTheCeilingBelow44k`, `testTheCeilingUnitFollowsTheRateTheTruePeakPathEngagesAt`. | Keep the accepted design in #42. The earlier assumption of a dBTP guarantee at 4 or 11.025 kHz is superseded by that explicit rate contract. |
| TP-only offline-entry latch discards a bulk-swap duck | `a43094b` / `f03d673`: any offline-entry latch, including TP-only with Oversampling Off, clears EQ history, the dry ring, engagement tail and output TP history. `AnabasisEngine::process` intentionally adopts directly at the render boundary. | Keep direct adoption: a host-selected stream boundary is the start of the file, and a transition fade would write an unwanted envelope into the bounce. General no-re-prepare residuals remain disclosed in KI-004 / KI-024. |

## The state-restoration crash remains unattributed

`2026-09-29-pr43-phase0-verification.md` §7 records seed `0x6036105`, exit 139
during **Plugin state restoration**, before `SUCCESS`. Its retry used
`0x7a99ff4`, so it does not reproduce the failing case. No core or stack was
retained. Log position cannot establish the cause of this individual crash.

PR #42's `2026-09-29-pr42-round5-contract-ki028-clock.md` §6.2 and KI-028
contain actual stacks for two external validator faults, including an in-test
XEmbed use-after-free, and a causal upstream-fix experiment for the teardown
fault. They establish those signatures; they do not establish that #43's
crash without a stack has either signature. The Phase 0 worklog already states
this limit. No speculative plug-in change or validator retry change is justified.
Reproduction of the exact failure needs the same seed and a stack from the
failing process before it can be classified.

## Conflicts and validation scope

Main was merged into #43 in `d36f453`. The only conflict was the introduction
to `docs/DOCUMENTATION_COVERAGE.md`: both branches independently migrated the
contributor guide to AGENTS.md. The development version was retained, including
all Phase 0 evidence. Main's nonconflicting action-pin updates were retained.
No source or test change was made by that merge. #42 must then merge this updated
#43, retaining its Phase 1 records and later DSP implementations.

This branch changes merge ancestry and records only. DSP regressions are run
on the composed #42 tree; this record does not claim that #43 alone contains
those later fixes. The companion #42 worklog records current validation and
the limits of the validator investigation.
