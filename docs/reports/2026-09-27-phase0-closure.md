# Anabasis — Phase 0 closure: decisions accepted, the PR #42 review closed

**Date:** 2026-09-27 · **Version:** 0.2.13 (unreleased) · **Class:** dated record in `docs/reports/` —
a snapshot, superseded by a later record rather than edited in place (`docs/SOURCE_OF_TRUTH.md`).

This record closes Phase 0 of the 2026-09-26 product / UX audit
([`2026-09-26-anabasis-product-ux-audit.md`](2026-09-26-anabasis-product-ux-audit.md)). It supersedes
the round-one outcome table in [`2026-09-27-phase0-follow-up.md`](2026-09-27-phase0-follow-up.md) and
the open questions in [`2026-09-27-phase0-owner-decisions.md`](2026-09-27-phase0-owner-decisions.md)
where they differ; neither of those, nor the audit, is edited. The owner's instruction of record for
this round was to accept the Phase 0 decisions, settle the delivery-meter definition from the existing
evidence, and close the PR #42 review.

## 1. What was decided

| Record | Decision | Where |
|---|---|---|
| [ADR-0041](../architecture/design-decisions/ADR-0041-clamp-true-peak-path-inside-the-latency-allowance.md) — the clamp's true-peak path inside the constant allowance | **Accepted 2026-09-27**, as revised in the PR #42 review; decision 8's prescribed policy text completed at acceptance with invariant 8's decay sentence (it had been in `DSP_POLICY.md` without being prescribed) | ADR banner and status; `ADR_INDEX.md`; ADR-0004/0006/0015 banners; `DSP_POLICY.md`, `LATENCY_MODEL.md`, `FUTURE_RISKS.md` |
| [ADR-0042](../architecture/design-decisions/ADR-0042-a-frozen-latch-survives-a-host-re-prepare.md) — a frozen latch survives a host re-prepare in the audio | **Accepted 2026-09-27** — option C as written; option A (Freeze OFF across a re-prepare) and `KNOWN_ISSUES.md` KI-007 item 10 stay separate open decisions | ADR banner and status; `ADR_INDEX.md`; ADR-0014 banner; `MODE_AND_ADAPTATION_POLICY.md`; KI-006 → `POSTMORTEMS.md` INC-007 |
| [ADR-0043](../architecture/design-decisions/ADR-0043-dbtp-is-defined-on-the-product-meter-and-the-annex-2-filter.md) — the delivery-meter definition | **Accepted 2026-09-27 — Option 1 preserved**: "dBTP" is defined on the product meter and the BS.1770 Annex 2 filter; libebur128 and the long-kernel reference are reference/compatibility measurements | `DSP_POLICY.md` invariant 4 (prescribed text); KI-020; `TEST_REPORT.md`; `USER_MANUAL.md`; the two guards check each meter separately |

**Why Option 1 and not another.** It is the definition the accepted architecture already names —
ADR-0003 and ADR-0006 make the clamp's estimate a tap on the shared BS.1770 estimator, the meter the
plug-in displays — and the Annex 2 filter is the Recommendation's own example; both are implemented
and pinned inside the build, and it changes nothing audible. No alternative removes the underlying
fact that finite interpolators disagree near Nyquist: 2a moves the residual from libebur128 onto the
reference at +30 % detector CPU and a library dependency, 2b shrinks the TP-mode limiter window for a
residual that still does not reach zero, and 2c changes what the Ceiling control means. The
libebur128 (up to +0.18 dB, 130 of 2736) and long-kernel (up to +0.98 dB, hot subset) residuals are
disclosed as reference behaviour, with KI-020's workaround, not hidden.

## 2. The PR #42 review, item by item

**A — "True-peak ceiling leaks during engagement" (`AnabasisEngine.cpp` TP latch): `FIXED — verified`.**
Implementation: commit `8e8882a` — the TP-on latch happens at the toggle and a checked decay of the
last emitted frame (`EngagementTail`, `CeilingClamp.h`) replaces the duck's out-leg; ADR-0041
decision 5 (revised). Evidence, re-run on the accepted tree this round unless marked:

| Check | Result |
|---|---|
| Reproducer on the review's starting head (review round) | 186 of 248 transition configurations over, worst +4.66 dB (product meter) / +5.46 dB (Annex 2), from the first sample after the toggle |
| Regression test | `testTruePeakEngagementHoldsTheCeiling` — fails on the review's starting head in 13 of 13 configurations (+2.43 to +4.68 dB); passes now, and since this round checks the product meter and the Annex 2 filter separately |
| Transition sweep, 248 configurations | **0 over**, worst +0.001 dB (product) / +0.003 dB (Annex 2) |
| Post-EQ boosted case | 228 of the 248 carry a +12 dB Post shelf; all inside the tolerance |
| Ardour 8.4, TP switched on mid-export | toggled render −0.047 dB (product) / +0.000 dB (Annex 2) against the ceiling after the toggle, 0 samples over +0.1 dB; the same window TP-off read +2.95 / +3.15 dB. Re-exported this round after a VST3 rescan; bit-identical to the review-round render |
| TP-OFF equivalence | 222 of 222 TP-off configurations hash-identical to `main`; the Ardour TP-off export identical to the PR-head build's |
| Steady state | 2718 + 240 matrix renders hash-identical to the review round; the 56-configuration hot subset identical |
| Latency | reported latency `maxLookahead(10 ms) + osLatency` in both modes — `testReportedLatencyMatchesImpulse`, `testOsLatencyMatrix`, `testTruePeakModeCapsTheWindowNotTheLatency` green in this round's suite run; D = 41/42/52/54/74/78 samples at 44.1–192 kHz (review round, unchanged code) |

**B — ADR-0041's status: Accepted**, history preserved (the gate items stay in the ratified banner in
past tense; the review-round revision banner is kept and re-tensed; nothing in Decision rewritten). The
text matches the code on each point the acceptance covers: the latency allocation (decision 4 and the
"What the owner was asked to decide" item 1), the reported latency, TP-off identity and the engagement
fix. Stale "owner decision required" / "not to be merged" wording was removed from every living
document; the worklogs and the two earlier reports keep theirs as history.

**C — the delivery-meter definition: decided, Option 1 (ADR-0043).** The durable guards now name it:
`testTruePeakModeHoldsTheCeiling` and `testTruePeakEngagementHoldsTheCeiling` count the product meter
and the Annex 2 filter separately, so a failure names its meter; `testClampTruePeakDetector` gains the
missing lower bound against the Annex 2 filter's reading of the same segment (it had one only against
the product meter).

**PREfast threads.** C6011 (null `w[c]` when `numCh ≤ 0`) fixed in the review round (thread resolved
and outdated). The C6262 "function uses N bytes of stack" threads are all in `tests/dsp_tests.cpp`
(engine fixtures on the test stack) — test-only, the scanner audit's G2 disposition; left open as
**Warnings**, not dismissed.

## 3. Phase 0 status matrix

Outcomes: Fixed · Accepted-preserved · Deferred · Rejected · Investigate further.

| Item | Outcome | Engineering | Decision | Record |
|---|---|---|---|---|
| **DSP-001** (P0) TP mode does not hold its dBTP ceiling | **Fixed** | complete — 0 of 2736 over on both defining meters; TP-off bit-identical | complete — ADR-0041 Accepted | `testTruePeakModeHoldsTheCeiling` |
| PR #42 review: TP engagement leak | **Fixed** (`FIXED — verified`) | complete — §2 A | complete — ADR-0041 decision 5 accepted | `testTruePeakEngagementHoldsTheCeiling` |
| **STATE-004** a re-prepare drops a frozen latch | **Fixed** | complete — engine and processor level | complete — ADR-0042 Accepted | INC-007; not observed in a DAW (TEST-002) |
| **DSP-004** Clip Drive at OS Off softens the top octave | **Accepted-preserved** | complete — first-order ADAA kernel, by design; figures disclosed and pinned | complete — no change | KI-005, `USER_MANUAL.md` |
| **STATE-002** a factory preset resets TP/Dither/Shaping; LOCK holds the number only | **Deferred** | not started — the fix is known (a `{ceiling, truePeakMode}` lockable set through one predicate) | **owner decision required** — widening ADR-0010's Accepted lockable set is a hard stop, plus the dither rule and the LOCK tooltip copy | KI-021 |
| **UX-003** Save overwrites silently | **Deferred** | not started — the narrowed prompt rule is designed (audit `findings-ux.md`) | **owner decision required** — a brand-checklist §A deviation (Anamorph overwrites silently and documents it) and the dialog strings | KI-022 |
| ADR-0041 | **Accepted** | — | complete | ADR_INDEX |
| ADR-0042 | **Accepted** | — | complete | ADR_INDEX |
| Delivery-meter definition | **Accepted-preserved** (Option 1) | complete — no code change; guards name the meters | complete — ADR-0043 | KI-020 (documented limitation) |
| `realtime` CI job red | **Fixed** (review round, `e36890f`) | complete — green on GitHub at the review head | — | — |
| VIS-002 (TP row warns at the ceiling) | **Deferred** (outside Phase 0; made more visible by DSP-001) | not started | an ADR-0020 amendment | KI-020 |

**Phase 0 is complete in engineering and in decisions for every item it gated.** STATE-002 and UX-003
are carried as deferred owner decisions with records; they are not Phase 0 gate items and are not
forced into Phase 1.

## 4. STATE-002 and UX-003 under the new ADR state

Re-checked against the accepted tree. Neither acceptance touches them: the preset code, the parameter
tiers and `InternalState` are unchanged since the audit commit. **STATE-002 is more exposed, not less**
— with ADR-0041 accepted, TP mode is a real dBTP guarantee, so a locked factory browse now drops a
guarantee that holds rather than one that leaked. The only fix that leaves no conflicting decision on
record is the extension path ADR-0010 itself names ("adding a second lock later is a registry entry
and an ADR"), which the owner has to accept first. **UX-003** needs a deliberate deviation from the
brand checklist's must-match save flow and new dialog copy; the one string-free part (Save inert
while the cleaned name is empty) is a family deviation too. Both stay deferred.

## 5. Checks

| Check | State |
|---|---|
| GitHub, review head `2071294` | 32 check runs: every run that executed passed (build ×4 platforms, sanitizers, LTO GCC/Clang, `realtime`, docs, source-lint, preflight, CodeQL ×3, PREfast, dependency-review, merge-check); the "skipped" rows are the duplicate push/PR runs |
| Review threads | 9 open, all PREfast C6262 on `tests/dsp_tests.cpp` — **Warning**, test-only (above); 1 resolved (C6011) |
| This round's commits | re-run locally before the push (suites, clang warning gate, `realtime` steps, docs/citation/portability checks, pluginval); GitHub's run on the new head is the evidence of record — **not** claimed clean from local results |
| pluginval teardown segfault (review round) | re-checked this round — see the 2026-09-27 Phase 1 worklog |

## 6. Evidence

- Re-verification commands and outputs: `worklogs/2026-09-27-phase1-match-statistics-observability.md`
  §Phase 0 re-verification.
- The review round's measurements: [`worklogs/2026-09-27-pr42-review-closure.md`](../../worklogs/2026-09-27-pr42-review-closure.md).
- The first round's: [`worklogs/2026-09-27-phase0-product-correctness.md`](../../worklogs/2026-09-27-phase0-product-correctness.md).

*Pointer (2026-09-29, PR split review; this record is otherwise unedited).* PR #42 was split into
stacked PRs, and this record ships in PR #43. The Phase 1 worklog cited above is not part of PR #43,
and its re-verification ran on the review head's code; the pluginval re-check in §5 ran on PR #42's
`11c9482`. The Phase 0 re-verification was re-run on PR #43's own head `2a5f8a8` —
[`worklogs/2026-09-29-pr43-phase0-verification.md`](../../worklogs/2026-09-29-pr43-phase0-verification.md),
which also re-runs the pluginval item (two external crash positions occurred there, both retried and
passed) and the ADR-0042 ordering mutation.
