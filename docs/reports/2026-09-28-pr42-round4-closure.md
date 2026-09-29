# Anabasis — PR #42, fourth review round: the true-peak contract at every engaged rate, KI-024, the statistics RESET

**Date:** 2026-09-28 · **Version:** 0.2.16 (unreleased) · **Class:** dated record in `docs/reports/` —
a snapshot, superseded by a later record rather than edited in place (`docs/SOURCE_OF_TRUTH.md`).

> **Followed by [`2026-09-29-pr42-round5-closure.md`](2026-09-29-pr42-round5-closure.md) (0.2.17).**
> ADR-0046 is ratified; OQ-020 is resolved (option 2); KI-028 is dispositioned pre-existing / external
> (Apple's AudioToolboxCore under pluginval's AU host teardown); DSP-005 — §5 item 1 below — is
> implemented, KI-023 partly fixed. Two figures here are corrected there: the PREfast alerts called
> C6011 are C28182, and "the 7 added are this round's new tests" is five tests and two helpers. The
> CodeQL anchor in §4 is pinned to `58107a4`, the head it names. The rows below stand as the state at
> `6ee9f29`.

The closure record for the review findings raised against PR #42 at head `8ab0532` (0.2.15) — two
new findings, the true-peak items the previous round left open (KI-024, KI-025), and the GR max and
tooltip items carried from it. Scope, by the owner's instruction: close the true-peak contract, fix
the RESET bug, verify the PR and CI, and only then consider Phase 1; no Phase 1 expansion, no merge.
The evidence is
[`worklogs/2026-09-28-pr42-round4-tp-lowrate-reset.md`](../../worklogs/2026-09-28-pr42-round4-tp-lowrate-reset.md);
the previous round's record ([`2026-09-28-pr42-review-closure.md`](2026-09-28-pr42-review-closure.md))
is not edited.

Each row keeps apart what was FOUND (by the review or by this round's audit), what was IMPLEMENTED,
what was VERIFIED, what was DECIDED and what REMAINS — a bug that existed stays recorded as having
existed.

## 1. The review, item by item

| Item | Status | Root cause | Implementation | Tests | Evidence |
|---|---|---|---|---|---|
| **Low-rate ceiling cuts exceed dBTP limit** (review finding, `CeilingClamp.h`) — with KI-025 | **Fixed — ADR-0046 (on the owner's direction, ⊕)**; KI-025 closed | The clamp's requirement bounds a segment's interpolated reading only while the gains its interpolation reads are equal; the boxcar attack ramp at its 8-sample floor starts beside a segment at the ceiling and lifts it through the kernel's negative lobes (the excess formula matches the measurement to the last digit). **Found larger than recorded**: every rate 4–32 kHz over on a STATIC ceiling (+0.185 to +0.307 dB, a 656k-evaluation search on 0.2.15), and a Ceiling reversal mid-ascent read +0.30 dB at 48 kHz at clamp level (ADR-0045's stamping) | Eased attack (geometric weights, λ·A = 4.8, floor 16 → D = 46 below 66 kHz), release rise ≤ 1 %/sample, revision reach narrowed to the defining readings, stamp = min(entry, predicted emission ceiling), TP engaged from 12 kHz through one predicate for the engine rail and the Ceiling unit — the design's 8 kHz floor moved up when a derived bound on the retarget step exceeded the tolerance there (+0.1008 dB); two other designs built and rejected on measurement | `testTruePeakModeHoldsTheCeilingBelow44k` (fails 12 checks on 0.2.15), `testTheCeilingUnitFollowsTheRateTheTruePeakPathEngagesAt`; the delay and step pins updated; after the round's review, `testTruePeakModeLagsAnAscentByTheEntryCeiling`, `testTheClampReleaseRiseIsCapped`, `testTheClampSilencesAnAstronomicalInput` | §2 below; the worklog §1, §5 and §5.5 |
| **Old peaks survive statistics reset** (review finding, `AnabasisEngine.h`) | **Fixed** | The output estimator reports 6 samples late, so its first 6 readings after RESET described pre-reset positions and reached the new hold — up to +0.957 dB above the OLD session's own maximum. Found on the way: the loudness watermark let the K-weighting's ring-out of the pre-reset programme in (5 s of silence after RESET read −33.7 LUFS integrated) | The session TP skips exactly the report lag's 6 readings; the estimator keeps its history (no invented onset); the first admitted loudness sub-block starts a whole sub-block (100 ms) after a reset or bypass resume — first built at 50 ms, widened when the round's review measured a residual there (row below) | `testStatisticsResetStartsTheSessionAtTheReset`, `testResetRightAfterALoudPassageKeepsTheOldPeakOut` (6 + 2 failures unfixed; each rejected option fails its own checks; the 50 ms guard fails part 6) | §3 below; the worklog §3 and §5.5 |
| **KI-024** (open from the previous round) | **Route C fixed; A, B, E preserved; D / Dd deferred to KI-004; the entry corrected** | A host `reset()` never reaches the engine (the entry's premise was a drift); a Force Max offline entry without a re-prepare latched without restarting the EQ or the dBTP tap — old audio into the render (to −1.0 / −1.2 dBFS) and a +0.88 dB hold for a file with no over | The latch restarts the EQ and the tap, and (after the round's review) empties the bypass leg's ring, as `prepare()` does | `testAForceMaxEntryStartsTheRenderClean` (3 failures unfixed; its bypass case fails without the ring clear) | `KNOWN_ISSUES.md` KI-024's disposition table; the worklog §2 and §5.5 |
| **GR max** (carried) | **FIXED — verified** (regression only) | — | unchanged since `e380d72` | `testTheGrReadoutReadsTheRingItNames`, `testTheTickMaxIsTheDeepestEntryTheGraphDraws` pass on the final tree | the worklog §7 |
| **STATISTICS tooltip** (carried, OQ-018) | **Closed — OQ-018 resolved, option 1** | — | "Waveform statistics off the output" kept: verbatim the recorded 0.1.1 wording (`d1640bb`); no new copy | the truth check `testTheStatisticsPanelResetsOnlyFromItsResetControl` | `OPEN_QUESTIONS.md` OQ-018 |
| **"lim GR" / "GR max" captions** (carried, OQ-019) | **Open — kept as shipped** | — | accurate (the limiter's reduction now; the deepest in the drawn history), so not changed | — | `OPEN_QUESTIONS.md` OQ-019 |
| **CI on `8ab0532`** (the previous round reported `f048328`) | **Verified green** | — | — | — | §4 below |
| **The round's own review of the integrated tree** (`0f162c8`; mutation of every regression test) — D1: the eased weights' float sum just over 1 at 88.2 / 384 / 768 kHz | **Found and fixed before the round closed** (a regression of the round's own fix) | An astronomical input (a forward minimum at ~0) asked for a reduction of 1 + 1.19e-7: a gain of −1.19e-7, clipped by the backstop to a sign-inverted full-scale sample — +1.85 dB over at 1e30 where 0.2.15 was silent | The weighted reduction divided by the float weights' own sum (loop order), capped at 1 | `testTheClampSilencesAnAstronomicalInput` | the worklog §5.5; ADR-0046's implementation note |
| — D2: the KI-024 fix left the bypass leg's ring | **Fixed** (pre-existing, identical in 0.2.15) | A bypassed Force Max render entered without a re-prepare played the realtime input until render latency − 1 (−2.0 dBFS) | `dryRing.clear()` in the latch's branch | the bypass case of `testAForceMaxEntryStartsTheRenderClean` | KI-024 row C |
| — M1 / M2: the engine's min stamp and the release rise cap each survived being reverted | **Pinned** | no check depended on them (the tests' clamp driver re-implemented the stamp) | — | `testTruePeakModeLagsAnAscentByTheEntryCeiling`, `testTheClampReleaseRiseIsCapped`; five mutants of the fixed tree, five killed | the worklog §5.5 |
| — L1: the 50 ms loudness guard | **Fixed** | DC at 0.99 cut at the minimum gap left the ungated mean at −115.5 LUFS after 2 s of silence (16 of 336 runs); 49.98 ms at odd sub-block lengths | a whole sub-block (100 ms): 0 of 336 | part 6 of `testStatisticsResetStartsTheSessionAtTheReset` | ADR-0020's note |
| — the astronomical-input class at 1e9 (+180 dBFS) | **Recorded — KI-027** (pre-existing) | float's gain resolution just below 1 (5.96e-8): up to +1.96 dB over at 384 kHz (0.2.15 +1.81) | not changed | the gain-range check covers it; the reading is not asserted | `KNOWN_ISSUES.md` KI-027 |
| **CI on `58107a4`** (the records commit) | **Two failures, fixed in `f03d673`** | `macos` (AppleClang, universal build): `ClampTruePeakDetector.h` used an unqualified `size_t` with nothing declaring it first; CodeQL: one new high-severity alert in the changed test code (an `int` product widened after multiplying) | `<cstddef>` + `std::size_t`; the products widened before multiplying | — | §4 below |

## 2. The true-peak contract — matrix on the final tree

Over the live smoothed ceiling, product meter / Annex 2 (ADR-0043), tolerance **0.1 dB, unchanged**.
Engine figures from a 24 167-render matrix per tree (the worklog §5.2; the 12 kHz row run afresh for
the moved rail); "excl." removes ±16 samples around a lifecycle event (the splice below); bounds and
clamp searches from the worklog §5.1 / §5.3.

| Rate | TP | Engine matrix, worst (excl.) | Engine climbs (A2) | Retarget step: derived bound / clamp search (A2) | 0.2.15, same matrix | Result |
|---|---|---|---|---|---|---|
| 8 kHz | **not engaged** (sample clip; unit dB) | sample peaks +0.000 dB ⁱⁱ | (engaged: +0.0728) ⁱⁱⁱ | +0.1008 / +0.0867 | A2 +0.1172 | sample-peak promise holds; TP not claimed |
| 11.025 kHz | **not engaged** | sample peaks +0.000 dB ⁱⁱ | (engaged: +0.0530) ⁱⁱⁱ | +0.0733 (relaxation search +0.0999) / +0.0673 | +0.1153 | as above |
| 12 kHz | engaged | +0.0426 / +0.0380 (+0.0019 / +0.0125) | +0.0308 | +0.0672 / +0.0629 | not run | **holds** |
| 16 kHz | engaged | +0.0426 / +0.0380 (+0.0004 / +0.0076) | — | +0.0504 / +0.0509 | +0.1157 | **holds** |
| 22.05 kHz | engaged | splice ⁱ (+0.0005 / +0.0058) | +0.0219 | +0.0366 / +0.0411 | +0.1283 | **holds** |
| 32 kHz | engaged | +0.0426 / +0.0380 (+0.0013 / +0.0038) | — | +0.0252 / +0.0330 | +0.1172 | **holds** |
| 44.1 kHz | engaged | splice ⁱ (+0.0081 / +0.0028) | — | +0.0183 / +0.0281 | +0.0141 (excl.) | **holds** |
| 48 kHz | engaged | +0.0426 / +0.0380 (+0.0006 / +0.0025) | +0.0096 | +0.0168 / +0.0270 | +0.0132 (excl.) | **holds** |
| 88.2 – 192 kHz | engaged | +0.0426 / +0.0380 (≤ +0.0019 / +0.0052) | — | — | ≤ +0.0089 (excl.) | **holds** |
| 384 / 768 kHz | engaged | +0.0426 / +0.0380 (≤ +0.0010 / +0.0016) | — | — | ≤ +0.0049 (excl.) | **holds** |

ⁱ **The one class over 0.1 dB, +0.4064 / +0.6348 dB at 11.025 / 22.05 / 44.1 kHz, bit-identical on
0.2.15:** a Force Max offline entry WITHOUT a re-prepare on an fs/4 sine — the last realtime segments'
interpolation reaches into the emptied pipeline's zeros. It is the stream's own end at a host-drawn
boundary (a reset or re-prepare at the same instant reads the same), the render that follows is
clean, and it is dispositioned with KI-024 route B (Preserve). It is recorded, not hidden.
ⁱⁱ Below the rail the sample clip runs: measured at 7999 Hz in the matrix (every sample at or under
the ceiling, inter-sample readings up to +3.8 dB on hostile programme) and by the 8 kHz burst check in
`testTruePeakModeHoldsTheCeilingBelow44k`. ⁱⁱⁱ Measured on the tree before the rail moved, with the
path engaged there — the law's figure, kept for the owner's best-effort alternative (ADR-0046
decision 5).

**By path** (worst at engaged rates, PM / A2): steady state (static ceiling, 14 programmes, +12 dB Post
shelf) ≤ +0.0426 / +0.0380; engagement mid-programme ≤ +0.0011 / +0.0014 (1872 renders); offline entry
with a re-prepare, a rate change, a reset, an OS switch, a statistics reset ≤ +0.0011 / +0.0111;
offline entry without a re-prepare — the splice above, then clean; automation — fast and slow
descents, fast and slow ascents, three reversal patterns, repeated cuts at 13 ms and 4 ms — inside the
matrix figures, the retarget step bounded above; at clamp level (77 adversarial searches, stereo
linked, blocks 1–512) no output over 0.1 dB — plain cuts +0.0629 at 12 kHz, +0.0280 at 48 kHz; every
reversal ≤ +0.0454 with the min stamp, where 0.2.15 read +0.60 / +0.49 / +0.30 dB at 8 / 22.05 /
48 kHz. Reported latency identical to 0.2.15 in
all 24 167 renders and equal to the measured impulse delay at every rate. **No all-input derived
bound exists at any rate**: the promise rests on the derived bound for inputs not already under
reduction when a cut arrives, a relaxation search for the rest, and the searches — which is why the
rail sits where every one of them is under the tolerance.

**Re-measured on the fixed head `f03d673`** (the review's fixes, §1): the same harness over 14 200
renders at 12 / 16 / 22.05 / 32 / 44.1 / 48 / 96 / 192 kHz — 93.0–96.8 % bit-identical per rate to the
tree above, every reading within 0.0017 dB, every worst above unchanged, the only renders over 0.1 dB
the same 21 splice renders (ⁱ).

## 3. RESET semantics (ADR-0020, implementation note 2026-09-28)

- **What RESET clears** (unchanged): the integrated reading (gated and ungated), LRA, both peak holds
  — and PLR with them — and the session length.
- **Where the new session starts: at the reset.** The session TP hold is the true peak of the output
  waveform at positions from the reset on; no pre-reset position's reading enters it. A reading of a
  post-reset position may still carry the real waveform's tail over the samples just before the reset
  (the kernel's reach) — bounded at −12 dB relative to the pre-reset sample peak.
- **What is NOT reset** (unchanged, by the record): the rolling readings (M, S, RMS) and the true-peak
  estimator's history, so the rolling reading and the GR history stay continuous and no onset is
  invented; the dry/wet meters that feed the loudness compensation; the audio.
- **Loudness:** up to 200 ms after a reset or a bypass resume is left out of I (was up to 100 ms, with
  the K-weighting's ring-out of the old programme let in; the first fix's 50 ms guard, up to 150 ms,
  still let a DC ring-out into the ungated mean); LRA resumes ~3 s after, as before.
- Repeated RESET, RESET with no history, and RESET with TP on or off behave the same (tested).

## 4. CI — the actual result

**`8ab0532` (the head this round started from):** push run 36391998914 — docs, preflight, source-lint,
linux (suites, reproduction, probe, pluginval deterministic ×3 and randomise ×3, ABI floor, warning
gate), linux-lto-tests, linux-lto-clang, realtime, sanitizers, windows, macos, macos-intel: all
**success**; pull-request run 36392004200 — merge-check **success**; CodeQL, PREfast / preflight,
dependency review **success**. No failed or cancelled check.

**`58107a4` (the round's first push — the three code commits and the ADR / KI records):** push run
36487308268 — every job **success** except **`macos`, which FAILED to build**: AppleClang / libc++
(the universal build) rejected an unqualified `size_t` in `ClampTruePeakDetector.h` (reached through
`Latency.h` before anything declares it; the Intel job and every local toolchain accepted it).
**CodeQL** reported **1 new high-severity alert** — "Multiplication result converted to larger type"
on `58107a4:tests/dsp_tests.cpp:6695` (that head), `(96000 / B + 1) * B` in the reset test. merge-check,
PREfast and dependency review succeeded. Both failures are fixed in `f03d673`.

**`f03d673` (the review's fixes and the two CI fixes):** push run 36492315079 — docs, preflight, source-lint, linux (with pluginval ×3
both modes), linux-lto-tests, linux-lto-clang, realtime, sanitizers (ASan / UBSan and valgrind on both
suites), windows, **macos** (built, both slices tested, VST3 and AU pluginval) and macos-intel: all
**success**; pull-request run 36492321791 merge-check **success**; **CodeQL: no new alerts in the
changed code**; PREfast and dependency review **success**. No failed or cancelled check.

**PREfast** (warnings, not failures, on every head): alerts in the PR's changed code 94 → 100 → 101
across `8ab0532` / `58107a4` / `f03d673` — all C6262 stack-size warnings on test functions under
`tests/` (the 7 added are this round's new tests) plus the two pre-existing C6011 of the scanner
audit's group G3; none under `src/`. Recorded, not dismissed.

**`fe29bda` (the records head; plug-in source byte-identical to `f03d673`):** every check **success**
except **`macos-intel`, which FAILED** in its AU randomise lane, pass 3 / 3 (seed `0x5161f59`): every
test passed and printed `SUCCESS`, then the validator aborted at teardown with an uncaught
`std::bad_function_call` (an empty `std::function` invoked). macOS has no crash-retry by design, so
the gate failed. Not reproduced (6 local Linux replays of the seed: 5 clean, 1 XEmbed crash in the
Editor test), cause not established; the first such macOS failure in the last 100 push runs; the same
teardown position as the pre-existing Linux exit segfault the Linux retry has been absorbing. Recorded
as **KI-028**. A re-run of the job (attempt 2) passed every lane on other seeds — intermittent, and
the first attempt's failure stands. CodeQL on `fe29bda`: no new alerts.

## 5. Roadmap

**Completed before this round:** Phase 0 (ADR-0041 / 0042 / 0043; the Phase 0 closure record); in
Phase 1 — MATCH on the processed leg (UX-009, ADR-0044), the STATISTICS core (UX-002, VIS-001, VIS-009
in part, DOC-002; ADR-0020 amendment 4), TEST-001, the GR readout (VIS-007 / VIS-003 step 1, its max
corrected in the third round).

**This round:** the true-peak contract closed at every engaged rate (KI-025, ADR-0046; engaged from 12 kHz); KI-024
dispositioned and route C fixed (its bypass leg included); the RESET stale peak and the loudness
ring-out fixed; the round's own review closed (one regression of the round's fix, one pre-existing
gap, two unpinned mechanisms, the guard's width); GR max verified; OQ-018 closed; CI on `8ab0532`
verified; the rate contract stated (`COMPATIBILITY_MATRIX.md` §Sample rates); KI-026 and KI-027
recorded.

**Next, re-evaluated:**

1. **DSP-005 (KI-023)** — MATCH's predict floor; the whole of the BYPASS comparison's residual (+0.63 LU
   at the calibration point). Its preconditions from the brief — KI-025 closed, KI-024 dispositioned,
   RESET fixed, the TP matrix passing, CI green, no known violation — are **met on `f03d673`** as far as
this round can establish them, with the CI gate qualified by **KI-028** (an intermittent pluginval
teardown crash that failed `macos-intel` once on `fe29bda`, whose plug-in code is `f03d673`'s): KI-025 closed at every engaged rate, KI-024 dispositioned route by
route, RESET fixed, the matrix passing on the fixed tree, every CI check green. One qualification is
stated rather than hidden: **KI-027** is a known reading over the ceiling, pre-existing, at a finite
input near +180 dBFS — outside any programme a host delivers; whether it blocks is the owner's call.
The brief's other precondition is the owner's: the review of the ⊕ records (ADR-0046 above all). Not started in this
   round, and not to share a commit with it.
2. **VIS-010 + UX-010's cue** — the monitor-state indicator's place (a signed surface) and whether it
   carries the MATCH gain (a new published scalar: a Thread Model gate item).
3. **TEST-004, then VIS-005 / VIS-012** — one `formatReading` rule, the session length as the liveness
   heartbeat.
4. **VIS-014, STATE-008, VIS-004** — documentation or small display changes; then **UX-023, UX-008,
   VIS-013**.
5. **KI-028 before a release gate is trusted again** — a macOS reproduction of the AU teardown abort
   with a symbolised report (the seed is recorded); until then a red pluginval lane after `SUCCESS`
   is re-run and recorded, not waved through.
6. **Recorded, not scheduled:** KI-026 (the limiter's release at high rate × OS — a limiter-numerics
   decision); KI-027 (the clamp's float gain resolution at +180 dBFS input); the KI-024 D / Dd routes (KI-004's owner decision); whether an offline entry without a
   re-prepare starts a fresh statistics session (ADR-0020).

## 6. Owner decisions carried

| Decision | Record |
|---|---|
| ADR-0046 as a whole — the clamp's gain law, D = 46 below 66 kHz, the revision reach, the min stamping, the 12 kHz rail, the unit following the path (⊕) | ADR-0046 |
| 3901–11999 Hz (8 and 11.025 kHz included): TP not engaged (chosen) vs a best-effort clamp without the 0.1 dB claim | ADR-0046 decision 5 |
| Whether the editor says more than the unit when TP is not engaged, and in what words | `OPEN_QUESTIONS.md` OQ-020 |
| KI-024 routes D / Dd (empty the pipeline on every offline entry?) | `KNOWN_ISSUES.md` KI-004, KI-024 |
| KI-026 — a reduction-domain release for the limiter | `KNOWN_ISSUES.md` KI-026 |
| KI-027 — a gain domain that resolves near 0 for the true-peak clamp (inputs near +180 dBFS) | `KNOWN_ISSUES.md` KI-027 |
| KI-028 — whether a pluginval teardown crash may be re-run past, or blocks, until diagnosed | `KNOWN_ISSUES.md` KI-028 |
| Carried: ADR-0044, ADR-0045, ADR-0020 amendment 4 (⊕); OQ-019; STATE-002 (KI-021); UX-003 (KI-022); VIS-002 (KI-020); ADR-0042 option A / KI-007; the TP clamp's voicing constants (listening) | as recorded in the earlier records |

## 7. Evidence limits

No listening was performed for any change in this round. Every true-peak figure is the real engine or
the real clamp on synthetic programme, constructed bursts and searched vectors, read on the product
meter and an independent Annex 2 meter; searches give lower bounds on the worst case, and a derived
bound is stated as such where one exists (§2). No DAW was run at a low sample rate or under Ceiling
automation in this round; pluginval ran at 8–192 kHz locally on `0f162c8`, and on `f03d673` only in
CI (§4). The review's fixes were re-measured on the engine matrix and by mutation, not by a second
independent review.
