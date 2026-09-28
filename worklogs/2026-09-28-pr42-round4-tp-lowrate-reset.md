# 2026-09-28 — PR #42, fourth review round: the low-rate true-peak contract, KI-024, the statistics RESET

The fifth round on PR #42, after the third review round
([`2026-09-28-pr42-review-tp-contract.md`](2026-09-28-pr42-review-tp-contract.md)) had been pushed as
`8ab0532`. Asked for: close two new review findings ("Low-rate ceiling cuts exceed dBTP limit",
`CeilingClamp.h`; "Old peaks survive statistics reset", `AnabasisEngine.h`), close KI-025 completely
without widening the tolerance, disposition KI-024, verify GR max and settle the tooltip record, verify
the actual CI — and only then consider the next Phase 1 item. No Phase 1 expansion; no merge; Anamorph
untouched. The closure record is
[`docs/reports/2026-09-28-pr42-round4-closure.md`](../docs/reports/2026-09-28-pr42-round4-closure.md);
this file is the evidence behind it.

Same container and toolchain as the earlier rounds (Linux x86-64, 4 cores; GCC 13.3 Release + LTO for
the suites and the plug-in; clang 22 / 23 for the realtime tiers). Engine figures come from the real
`AnabasisEngine`, compiled from frozen worktrees (`8ab0532` for "base", the integrated head for
"final"); probes, logs, saved vectors and patches stay in the session scratch directory, not in the
tree. Every figure is over the LIVE smoothed ceiling unless stated, as product meter (PM) / BS.1770
Annex 2 (A2) — the two meters ADR-0043 defines dBTP on — tolerance 0.1 dB.

## 0. State at the start (inspected)

- PR #42 head `8ab0532`, base `main` `ed06ad0`.
- **GitHub CI on `8ab0532`** (the previous round's report relied on `f048328`; this is the head it did
  not report): push run 36391998914 — `docs`, `preflight`, `source-lint`, `linux`, `linux-lto-tests`,
  `linux-lto-clang`, `realtime`, `sanitizers`, `windows`, `macos`, `macos-intel`: **all success**;
  pull-request run 36392004200 — `merge-check` success (the rest skipped by design on the PR event);
  CodeQL (36392004201: actions, c-cpp) success; PREfast / preflight (36392004204) success; dependency
  review success. **No failed or cancelled check on `8ab0532`.** The uncertainty the brief named is
  closed: `8ab0532` was green.
- Code scanning: the 38 PREfast threads on the PR are all under `tests/` (C6262 stack size on fixtures,
  the two C6011 of the scanner audit's group G3); none on `src/`.
- Open records touched by this round: KI-024, KI-025 (both "the next true-peak items" in the third
  round's closure §4), OQ-018, OQ-019, ADR-0045 (Accepted, ⊕), ADR-0020 amendment 4 (⊕).

## 1. KI-025 and the review finding "Low-rate ceiling cuts exceed dBTP limit"

### 1.1 Root cause, proved on the base engine

An instrumented copy of the base clamp (logging x, r, q, m, ga, the release, g and the emitted ceiling;
output bit-identical to the unmodified engine, hash `f808fd08c6ee4b63` on the stored 22.05 kHz burst):

1. **What the requirement guarantees.** For every k in [j − 15, j + 16], g[k] ≤ ga[k] ≤ q[k] ≤ r_j, and
   each reading is positively homogeneous, so reading(λ·x) = λ·tp_j ≤ ceiling — **only if g is the
   same λ on every sample the reading reads.** The backstop bounds samples, never the points between.
2. **The attack ramp.** A length-A boxcar of the forward minimum: for q stepping to r at k0,
   ga[k0 − A + i] = 1 − i(1 − r)/A. A deeper segment j' = j + 16 + A starts its ramp at exactly x[j + 2],
   just past segment j's main lobe; every worst case measured fits (j + 24 at A = 8, j + 27 at 44.1 kHz,
   j + 28 at 48 kHz).
3. **What each reading reads.** The product meter and Annex 2 read x[j − 5 .. j + 6]; the accurate
   interpolator x[j − 16 .. j + 16].
4. **The excess, exactly.** With G the largest gain in the 12-tap window,
   abs(Y_p) − ceiling ≤ Σ_t (G − g_k)·(−s·h_p[t]·x_k): on the ramp taps x[j + 2 .. j + 6], h·x has the
   opposite sign to the reading, so LOWERING them RAISES it. At 22.05 kHz the five ramp taps add
   0.00252412, taking the reading from 0.13878014 to 0.14130426 = +0.1566 dB; the formula matches the
   measurement to the last digit, and holding all the window's gains at their maximum reads exactly
   +0.0000 dB in every worst case from 4 to 48 kHz.
5. **Why low rates.** The attack never drops below its 8-sample floor (8 at every rate to ~34 kHz, 11
   at 44.1, 12 at 48), so the ramp's slope — its depth over A — is 0.028–0.041 per sample at 4–32 kHz
   (every over above 0.1 dB) against 0.015–0.016 at 44.1 / 48 kHz.
6. **Why ADR-0045 does not cover it.** The emission-time fix corrects WHICH ceiling r is judged
   against, and that part is exact; the gain law is unchanged by it.

**Smallest reproducer on the base branch:** the engine at 22.05 kHz, a stored 32-sample burst placed at
the bottom of a 0 → −20 dB Ceiling cut (`adv_base sr=22050 … auto="0:0;1000:-20"`): PM +0.0332 /
**A2 +0.1566 dB**. The same burst family: 32 kHz cut A2 +0.1250, 8 kHz static +0.1134, 16 kHz static
+0.1157. A clamp-only reproducer fails at +0.1134 dB.

### 1.2 Every rate swept before choosing (base)

~350 hill-climbs (656k engine evaluations), every rate 4–192 kHz plus 384 kHz static, four scenarios
(static −20 dB; 0 → −20 cut; −1 → −12 cut; a 50 ms host ramp), block schedules 64 / irregular, OS 2× /
4× at 44.1 / 48 kHz, 32-, 64- and 128-sample bursts; every result replayed with a static ceiling to
classify it. Worst per rate, A2 / PM:

| Rate | Worst | Class | > 0.1 dB |
|---|---|---|---|
| 4 kHz | +0.3279 / +0.0826 | automation-specific (−1 → −12, mid-glide) | yes |
| 8 kHz | +0.2142 / +0.0646 | static floor | yes |
| 11.025 kHz | +0.2319 / +0.0745 | static floor | yes |
| 16 kHz | +0.1968 / +0.0737 | static floor | yes |
| 22.05 kHz | +0.1885 / +0.0437 | static floor | yes |
| 24 kHz | +0.1878 / +0.0444 | static floor | yes |
| 32 kHz | +0.1848 / +0.0420 | static floor | yes |
| 44.1 kHz | +0.0950 / +0.0395 | static floor | no (thin) |
| 48 kHz | +0.0612 / +0.0169 | automation-specific (50 ms ramp) | no |
| 88.2 / 96 kHz | +0.0383 / +0.0186 | static floor | no |
| 176.4 / 192 / 384 kHz | ≤ +0.0012 | — | no |

These are lower bounds (64-sample bursts raised the 32-sample figures by 0.02–0.04 dB); OS 2× / 4× at
44.1 / 48 kHz found nothing above 0.0000. **The finding is larger than KI-025 recorded**: every rate
from 4 to 32 kHz is over on a STATIC ceiling, not only under a cut.

### 1.3 Which rates are supported — the boundary nobody had stated

Read from the code and measured (base): any positive rate is accepted (no wrapper, JUCE-setting or
engine check); the only rail kept the TP path where its delay fits the allowance — every integer rate
≥ 3901 Hz at the 8-sample floor — and below it the sample clip ran silently while the Ceiling read
`dBTP` (3900 Hz: +2.09 / +0.86 dB on a replayed burst). The tests engaged TP at 44.1–192 kHz only;
pluginval ran 44.1 / 48 / 96 kHz; `COMPATIBILITY_MATRIX.md` had no rate row; `DSP_POLICY.md` invariant
4 said "any sample rate". So "rates below 22.05 kHz are unsupported" could not be claimed from the
repository, and the fix had to hold wherever TP engages or say where it does not. Found on the way:
the limiter's release stalls at high rate × OS (KI-026, below); negative rates inverted the old rail
(sr ≤ −76000 engaged TP with a negative line — unreachable from a conforming host; the new predicate
requires sr ≥ 12000).

### 1.4 Three designs, built and judged

Independent designers built three candidates on scratch copies of the base engine; independent
evaluators measured each on the real engine and at clamp level; a judge re-measured the decisive
cases. Summary (ADR-0046 Options carries the table):

- **A — two-stage verify-and-correct:** rigorous static bound (0.0653 dB), but **fails at engine level
  under Ceiling automation at 12 kHz: PM +0.1007 / A2 +0.1375** (reproduced by the judge at host
  blocks 8 / 16 / 64); TP floor 11701 Hz (loses 8 and 11.025 kHz); CPU +7–28 %; window −74 samples;
  the post-cut dip −0.88 dB. Rejected.
- **B — a 24-sample attack floor:** **breaches on a static ceiling at 32 / 44.1 / 48 kHz behind a
  +12 dB Post shelf (+0.159 / +0.154 / +0.167 dB)**, +0.375 dB at the start of a cut at 8 kHz; by its
  own derivation no attack length closes it. Rejected.
- **C — eased single-stage law:** chosen, conditionally. The judge found one hole none of the
  evaluators did — **the Ceiling reversal** (−20 → 0 → −20 dB mid-ascent) at clamp level, stereo linked:
  C as submitted +0.215 dB at 8 kHz, +0.166 at 11.025, +0.130 at 16, +0.105 at 22.05, +0.088 at 32,
  +0.052 at 48 kHz — and **base +0.604 / +0.490 / +0.299 dB at 8 / 22.05 / 48 kHz**, i.e. 0.2.15
  already broke ADR-0045's downward-automation promise for reversals (not reproduced at engine level:
  two 2500-step climbs with a +12 dB Post shelf at 8 kHz found nothing over; the limiter absorbs it).
  The judge's one-line fix — stamp each frame with min(entry, predicted emission ceiling) — measured
  ≤ +0.031 dB at 8 kHz, ≤ +0.028 / +0.012 / +0.019 / +0.015 at 11.025 / 16 / 22.05 / 48 kHz, and leaves
  a static ceiling and a descent unchanged (the plain-cut vector reads +0.0867 with and without it).

C's evaluation on the real engine (before the reversal fix, which changes no descent): static
≤ +0.0069 A2 over climbs at 8–192 kHz; the stored KI-025 bursts **0 of 306 replays over 0.1 dB, where
base had 93**; the start of a full-range cut +0.0748 (engine) / +0.0835 (clamp) at 8 kHz, +0.0542 /
+0.0649 at 11.025 kHz — the thin margin the verification below targets; reported latency 160 / 160
unchanged; TP-off 420 / 420 bit-identical; TP-on with the clamp idle 18 / 18 bit-identical; 0
allocations; the function-effects gate clean; CPU not measurable (−4 % / +3 %). Below 8 kHz C
disengages TP: 3901–7999 Hz regresses from engaged (base ≤ +0.003 dB on ordinary hostile programme)
to the sample clip (+1.87 dB A2) — the owner decision ADR-0046 records.

### 1.5 Implementation (commit 1) — and the rail moved from 8 to 12 kHz by the verification

C's patch plus the judge's reversal fix, the rail as one predicate (`truePeakPathEngages`, used by the
engine's prepare and by `CeilingUnitSource`), and the Ceiling unit reading the prepared rate the GR ring
already publishes. First committed locally with the design's 8 kHz rail (`d7aec51`, never pushed).
The verification of the integrated tree (§5.1) then derived bounds for the retarget step and found
8 kHz unsupported (+0.1008 dB derived; +0.1213 by the combined relaxation search) and 11.025 kHz at
the tolerance (+0.0999, still rising); every figure is under it at 12 kHz. So
`CeilingClamp::kMinTruePeakRate` became 12000 and commit 1 was rebuilt with it (`4ff71bd`), the two
later commits replayed on it unchanged — nothing had been pushed, and the brief's commit order is kept.
Tests:

- `testTruePeakModeHoldsTheCeilingBelow44k` (DSP suite): the rail at 11999 / 12000 / 11025 / 8000 /
  4801 / 3901 / 0 / −96000 Hz and at every common host rate 12–768 kHz; the engine's engaged window at
  11999 vs 12000 Hz; three KI-025 bursts at the rates they were found at (22.05 / 32 kHz cut, 16 kHz
  static) with reach premises, and the 8 kHz burst below the rail against the sample-peak promise; a
  360-render matrix (12 / 16 / 22.05 / 24 / 32 kHz × four bursts × three cuts × OS off / 2× / 4× × two
  offsets); clamp-level vectors at 8 kHz driven with the engine's stamping (blocks of 8) — the judge's
  reversal vector live (premise: emission-only stamping reads > 0.1 dB) and held with min stamping,
  the 0.2.15 reversal vectors held, the searched plain-cut vector with a reach premise (the law is
  rate-free below the rail, and 8 kHz is its widest glide step).
- `testTheCeilingUnitFollowsTheRateTheTruePeakPathEngagesAt` (state suite): TP on reads `dBTP` at 48 /
  12 / 22.05 kHz and plain `dB` at 11999 / 11025 / 8000 Hz, through the host-facing `getText`.
- Updated pins: the clamp delay 42 → 46 (`testCeilingClampTruePeakPath`); the retarget step bound
  (16 + A/2 + 2) → 7 glide steps and its premise 5× → 3× (`testTruePeakModeBoundsTheStepAtACeilingCut`).

**The regression test fails on the base engine.** `tests/dsp_tests.cpp` of the rebuilt commit 1
compiled against `8ab0532` (a shim adding 0.2.15's rail as `truePeakPathEngages`): **FAIL: 676
checks, 12 failures** — the delay pin and the step bound (×2), four rail checks, the three bursts
(22.05 kHz cut +0.1566, 32 kHz cut +0.1250, 16 kHz static +0.1157 dB A2), the matrix (worst +0.1179 dB
over 360 renders) and the reversal premise. On the fix: PASS, 676 checks; the state suite 1599.

## 2. KI-024 — every route reproduced, dispositioned, route C fixed (commit `a43094b`; the bypass leg `f03d673`, §5.5)

Harness: the real base engine, TP on, ceiling −1 dBTP, block 512, 48 boundary positions per
configuration, four programmes (HF tones + square + hats; gated fs/4 + pink; full-scale white; chord +
pink), a hot operating point (+18 dB push, comp −12 dB 2:1, clip drive 9 dB), EQ flat or a +12 dB 8 kHz
shelf in either position; Freeze off / on / engaged halfway. Readings: the continuous stream at the
boundary; the file from the boundary (fresh meters); the file from boundary + reported latency (what a
host that trims latency writes); old audio (the same run with the input zero from the boundary); the
engine's dBTP tap and the session hold.

| Route | Stream at boundary | File from b | Old audio after b | Tap / hold − ceiling | Disposition |
|---|---|---|---|---|---|
| A host `reset()` | +0.000 / +0.002 | +0.919 / +0.972 (head; pre-reset audio) | −1.0 dBFS, 455–493 samples | +0.003 | Preserve — a no-op by design (`reset()` is not overridden; the only caller of `AnabasisEngine::reset()` is `prepare()`); KI-024's "host reset" is a drift, corrected |
| B re-prepare | **+0.884 / +0.918** (seg −2) | ≤ +0.004 (zeros) | none | +0.003 | Preserve — the reading of the end of a stream the host stopped (an uninterrupted run's own end reads +0.87 to +0.93); a checked decay prototyped and rejected (+0.81 / +0.69 across the cut, pre-cut audio into the render, the file then over at its first sample) |
| C offline entry that latches, no re-prepare | **+0.884 / +0.918** | ≤ +0.004 | **−1.0 dBFS / 54 samples (Post +12); −1.2 dBFS up to 637 samples (Pre +12, after the latency window)** | **+0.884** | **Modify — fixed**: the latch also restarts the EQ and the dBTP tap. After: tap ≤ +0.000, hold ≤ +0.003, no old audio in either position, the file unchanged; the stream's own step is B's and stays |
| D offline entry, no composition change | +0.000 / +0.002 | +0.919 / +0.972 | −1.0 dBFS / 455 | +0.003 | Defer (KI-004): emptying the pipeline on every entry would change rendered samples and break `testOfflineEntryDropsTheEngagementTail` part (4) |
| Dd as D with a forced duck | +0.917 / +0.945 | +0.919 / +0.947 | −1.0 / 455 | +0.917 | Defer with D |
| E / E0 offline → realtime | −0.000 / +0.001 | n/a | the duck's out-leg (by design) | +0.003 | Preserve |

Freeze changes none of these (B and C with Freeze off / on / engaged halfway: +0.884–0.887 /
+0.918–0.931, the same tap readings). The reviewer's +0.96 / +0.98 dB was not reproduced; the worst here
is +0.884 / +0.918 dB with the EQ flat (+0.251 / +0.507 with the shelf). Not investigated further,
recorded: a clean re-prepared Force Max render trimmed by the reported latency reads +0.40 / +0.50 dB
at its head (the 16× linear-phase oversampler's energy ahead of its integer latency; the emitted
stream is not over).

**Test:** `testAForceMaxEntryStartsTheRenderClean` (44.1 / 48 kHz; EQ flat, Post +12 dB and Pre +12 dB;
two programmes; silence or new programme after the entry): the premise (the entry latches), the old
audio exactly 0, the tap ≤ 0.1, the hold ≤ 0.1, the render read as a file ≤ 0.1 on both meters.
Unfixed: 3 failures (tap and hold +0.827 dB; old audio); the EQ reset alone or the tap reset alone
each fails its half.

## 3. The review finding "Old peaks survive statistics reset" (commit `0f162c8`; the guard widened in `f03d673`, §5.5)

Reproduced at engine level on base, calling `resetMeterHolds()` where the wrapper does (a block top,
before `process()`), the hold kept as the wrapper keeps it:

- After a reset, the first 11 post-reset frames differ from an estimator started at the reset in every
  loud → silence run (blocks 1–1024; 44.1 / 48 / 96 / 192 kHz); frames 0–5 report positions from
  BEFORE the reset at full level. After pure digital silence the new hold read −1.041 dBTP (TP on;
  pre-reset −1.013), +2.098 (TP off), and **up to +0.957 dB above the old session's own maximum** in a
  sine scan — the old session never saw those last 6 positions (the estimator reports 6 samples late).
- The same class in loudness: the watermark admitted a sub-block starting right at the reset, into
  which the K-weighting filters ring the pre-reset programme — **5 s of silence after a reset read
  −33.7 LUFS integrated** (a 40 Hz tone before it) when the reset landed on, or up to ~29 ms
  (integrated) / ~47 ms (ungated) before, a 100 ms boundary. PLR inherits both (29.6–49.2 LU after reset
  + 5 s silence). Unaffected: the sample-peak hold, the session length, LRA (empty in every silence
  run). The RMS row is a 50 ms rolling window by design.

Options measured: restarting the estimator from zeros (three variants) invents an onset when programme
continues through the reset — up to +0.97 dB high (−0.043 dBTP on a −1 dBTP ceiling whose real peak is
−1.008), +0.2 % of a core at 48 kHz; skipping the estimator's whole 11-frame reach misses real
post-reset peaks (a click in samples 0–4 read 15.7–64 dB low, the TP hold under the SP hold). **Chosen:
skip exactly the 6 readings of the report lag** — one counter, bit-identical audio, the rolling reading
and the GR history untouched; equal to the "post-reset positions only" reference in 576
configurations; never above the continuous meter; the residual after loud → silence is the real
waveform's tail, ≤ 0.2504 × the pre-reset sample peak (−12.03 dB) by the kernel, measured 14.9–68 dB
below. Plus a 50 ms guard on the loudness watermark (and the same on a bypass resume): the leak gone in
all 450 reset positions scanned. (Widened to a whole sub-block, 100 ms, after the round's review measured
a residual at the 50 ms minimum gap — §5.5.)

**Tests:** `testStatisticsResetStartsTheSessionAtTheReset` (DSP) and
`testResetRightAfterALoudPassageKeepsTheOldPeakOut` (state; the processor's own request and published
holds, a full-scale fs/4 tone ending at the reset). Unfixed: 6 DSP and 2 state failures; each rejected
option fails its own checks.

## 4. The sample-rate audit's other findings

- **KI-026 (new, recorded, not changed):** the limiter's release is a float one-pole on the envelope
  that stops moving once its step is below half an ulp; the stall depends on the region rate R = sr ×
  OS. Whole engine, 1000 ms manual release, after a burst then −30 dBFS: residual reduction −0.012 dB
  at 48 kHz × 1, **−0.20 dB at 48 kHz × 16, −0.92 dB at 192 kHz × 16, −6.02 dB at 768 kHz × 16** (never
  releases); AUTO −0.065 / −0.26 / −1.24 dB. Force Max renders at 16× at any host rate. A level error,
  never a ceiling violation. The fix (a reduction-domain one-pole, as the clamp uses) changes the
  limiter's numerics everywhere — its own decision.
- Above 192 kHz nothing crashes and TP held in a 64-run programme sweep (352.8–768 kHz; worst +0.0003 /
  +0.0002 dB); 768 kHz × 16 is not realtime on this machine (144 % of a core).
- The K-weighting (f0 1681.97 Hz) is valid only above ~3.4 kHz; the EQ and the sidechain HPF clamp their
  frequencies to 0.49 · sr — both recorded in `COMPATIBILITY_MATRIX.md` §Sample rates.

## 5. Verification of the integrated tree

Five independent tracks on a frozen worktree of the integrated tree as first built (`b712139`, local
and never pushed: the three code commits with the design's 8 kHz rail; they were replayed onto the
12 kHz rail as `4ff71bd` / `a43094b` / `0f162c8`, and §2–§3 cite the pushed SHAs), each told to report only what it measured or
derived, with a reproduce command. The rail then moved to 12 kHz (§5.1); the code at 12 kHz and above
is identical between the two heads, so every figure at those rates carries over, and the 12 kHz row
was run afresh on the same harness (§5.2).

### 5.1 Bounds (the finding that moved the rail)

A Python model of the final gain law, driven by detector peaks, matched the C++ clamp to 2.06e-6 in
gain over 1000 steps on the saved 8 kHz cut vector. The plain full-range cut throughout (the ceiling
at 0 dBFS, one retarget to −20 dB at a block top, JUCE's 20 ms linear smoother in float32, relative
glide step ρ = 0.9 / ⌊0.02·sr⌋), stamping min(entry, emission), the reference min(live[j], live[j+1]).

| Rate | Clamp search (A2 / PM) | Knapsack bound (A2) | Global bound (A2 / PM) | Combined relaxation search (A2) |
|---|---|---|---|---|
| 8 kHz | +0.0867 / +0.0490 | **+0.1159** | **+0.1008** / +0.0787 | **+0.1213** |
| 11.025 kHz | +0.0673 / +0.0379 | +0.0844 | +0.0733 / +0.0573 | **+0.0999** (still rising) |
| 12 kHz | +0.0629 / +0.0358 | +0.0774 | +0.0672 / +0.0525 | +0.0930 |
| 16 kHz | +0.0509 / +0.0263 | +0.0581 | +0.0504 / +0.0394 | +0.0784 |
| 22.05 kHz | +0.0411 / +0.0205 | +0.0422 | +0.0366 / +0.0286 | — |
| 32 kHz | +0.0330 / +0.0143 | +0.0291 | +0.0252 / +0.0197 | — |
| 44.1 kHz | +0.0281 / +0.0107 | +0.0211 | +0.0183 / +0.0143 | — |
| 48 kHz | +0.0270 / +0.0097 | +0.0194 | +0.0168 / +0.0131 | — |

- **Knapsack** — the judge's relaxation as specified (only the target's same-phase reading and
  the sample bound abs(x) ≤ 1 kept; per-tap gain boxes from the law's monotonicity), exact. **Global** — SCIP spatial
  branch-and-bound keeping every reading of the 23 segments around the target, the exact code reach
  (defining x[j−5..j+6], Z = 1, full reach after the revision), the eased weights and the release;
  gap 0 at the reported maxima. Both cover only the **revision-only class** (inputs whose readings
  never exceed the pre-cut ceiling). The exact per-tap drop replacing (i + Z + 1 + E): 4.77, 5.83,
  6.91, 8.01, 9.16 … glide steps for in-flight taps e = 0, 1, 2, 3, 4 ….
- **They are beaten by real clamp searches at 16 kHz and above** (by 0.0005–0.0102 dB) — the winning
  search vectors are already under reduction when the cut arrives — so they are not all-input bounds.
  The combined case (static activity plus the revision) was searched: exact law, exact inner LP over
  x (the parabola refinement dropped), a hill-climb over requirement sequences. Its values are a
  relaxation's search values, neither bounds nor realised inputs (the 8 kHz pattern replayed as a
  real input on the clamp reads −0.149 dB); fit ≈ 0.031 + 762 / sr dB, crossing 0.1 dB near 11.05 kHz.
- **Static figure** over arbitrary requirement sequences (outer search, exact inner LP): +0.0612 dB
  at 8 and 16 kHz, +0.0495 at 48 kHz (a release running into a cap; reproduces the designer's
  +0.0609). A rigorous all-input model (a normalised MINLP) is valid but did not converge in 150–600 s
  per case; **no all-input derived bound was obtained at any rate.**
- **Decision:** 8 kHz fails the derived bound; 11.025 kHz sits at the tolerance in the combined
  search; **12 kHz is the lowest common rate at which every figure is under 0.1 dB** (≥ 0.007 dB
  margin). `kMinTruePeakRate` = 12000 (§1.5). No measured violation exists at 8 kHz — the rail follows
  what can be supported.

### 5.2 The engine TP matrix

The real engine of the integrated tree and of `8ab0532`, the same 24 167 renders each: rates 7999 –
768 000 Hz; OS off / 2× / 4× / 8× / 16× / Force Max, minimum and linear phase; host blocks 1 / 7 / 64 /
512 and two irregular schedules; lookahead 0.5 / 2 / 10 ms; EQ flat and a +12 dB Post shelf; 14
programmes (the four KI-025 bursts among them) × 13 Ceiling automation shapes (static, fast / slow
descents and ascents, three reversal patterns, repeated cuts); a lifecycle tier (TP engagement at
three points, Force Max offline entry with and without re-prepare, return to realtime, re-prepare at a
new rate, reset, a statistics reset, an OS switch). Plus 142 seeded engine climbs (1 125 710 renders)
at 8 / 11.025 / 22.05 / 48 kHz. An independent numpy Annex 2 (the Recommendation's 48-tap table)
agreed with the harness to 4 decimals on the top renders and every climb vector.

| Rate | Worst, continuous stream (PM / A2) | Worst, ±16 samples around a lifecycle event excluded | Renders > 0.1 dB |
|---|---|---|---|
| 12 kHz ⁱ | +0.0426 / +0.0380 | +0.0019 / +0.0125 | 0 of 1631 |
| 16 kHz | +0.0426 / +0.0380 | +0.0004 / +0.0076 | 0 |
| 22.05 kHz | +0.4064 / +0.6348 ⁱⁱ | +0.0005 / +0.0058 | 10, all ⁱⁱ |
| 32 kHz | +0.0426 / +0.0380 | +0.0013 / +0.0038 | 0 |
| 44.1 kHz | +0.4064 / +0.6348 ⁱⁱ | +0.0081 / +0.0028 | 11, all ⁱⁱ |
| 48 kHz | +0.0426 / +0.0380 | +0.0006 / +0.0025 | 0 |
| 88.2 – 768 kHz | +0.0426 / +0.0380 | ≤ +0.0019 / +0.0052 | 0 |

ⁱ Run afresh on the same harness and job set for the 12 kHz rail (1775 renders, 1631 with TP on).
ⁱⁱ **One class, identical in `8ab0532`**: a Force Max offline entry WITHOUT a re-prepare on an fs/4
sine at 45° — the last realtime segments' interpolation windows reach into the emptied pipeline's
exact zeros (KI-024 route C's stream step, the same as a reset or re-prepare at that instant, which the
harness excludes by convention and which reads the same +0.6348 with the exclusion off). The render
that follows is clean. Dispositioned with KI-024 route B (Preserve).

- **Climbs** (A2, worst per rate; `8ab0532` on the same vector): 8 kHz +0.0728 (+0.2013); 11.025 kHz
  +0.0530 (+0.1508); 22.05 kHz +0.0219 (+0.0298); 48 kHz +0.0096 (+0.0096). At 12 kHz, 20 further
  unseeded climbs (4000 iterations each, start-of-cut, landing, static and reversal scenarios): worst
  +0.0308. 8 and 11.025 kHz are below the new rail; their figures are the law's, measured.
- **`8ab0532`** read over 0.1 dB in 378 renders outside the splice class, all at ≤ 32 kHz on burst
  programmes (A2 up to +0.1283 at 22.05 kHz, +0.1172 at 8 / 32 kHz); the integrated tree in none.
- **Engagement mid-programme**: +0.0011 / +0.0014 dB over 1872 renders. **With re-prepare, rate
  change, reset, OS switch, statistics reset**: ≤ +0.0011 / +0.0111 dB.
- **Below the rail** (7999 Hz here; 8–11.999 kHz on the new head): TP not engaged — the sample clip
  holds every sample (SP +0.00000 dB) and inter-sample readings reach +3.8 dB on the hostile
  programmes, as ADR-0046 states.
- **Reported latency** identical between the trees in all 24 167 renders and equal to the measured
  impulse delay at every rate (Force Max: argmax = reported + 1, the minimum-phase peak, identical in
  both trees). **A statistics reset changes no output sample** (1248 / 1248 bit-identical in each
  tree). **No non-finite output.**

### 5.3 Clamp-level adversary

The clamp of the integrated tree driven exactly as the engine drives it (the 20 ms smoother retargeted
at each block top, the copy run D steps ahead into `lowerInFlightCeilings`, min(entry, emission)
stamps), stereo linked, judged on the product meter, the product-table Annex 2 and an independent
double-precision Annex 2 (cross-checked in numpy on the worst vector). 77 searches — hill-climbs of
250k–1M iterations, simulated annealing, coordinate sweeps, automation-mutating climbs — at 8, 11.025,
12, 16, 22.05, 32, 48 and 96 kHz; blocks 1 / 3 / 5 / 7 / 8 / 13 / 512 and two mixed sequences. **No
output over the live ceiling + 0.1 dB on any meter.**

| Rate | Plain cut | Reversal | Mutated automation | Other |
|---|---|---|---|---|
| 8 kHz ⁱ | **+0.0870** (PM +0.0504 on its own objective) | +0.0454 | +0.0675 | cut depths −3 / −6 / −20 dB: +0.0384 / +0.0548 / +0.0870; static 0 dB +0.0380; after a reset +0.0354; staircases ≤ +0.0500 |
| 11.025 kHz ⁱ | +0.0673 | +0.0383 | +0.0458 | — |
| 12 kHz | +0.0629 | — | — | — |
| 16 kHz | +0.0509 | +0.0358 | +0.0255 | — |
| 22.05 kHz | +0.0411 | +0.0364 | +0.0366 | — |
| 32 kHz | +0.0330 | +0.0308 | +0.0223 | — |
| 48 kHz | +0.0280 | +0.0170 | +0.0336 | static 0 dB +0.0442 |
| 96 kHz | +0.0089 | +0.0388 | +0.0036 | — |

ⁱ Below the 12 kHz rail on the final head: the law's figures, kept for the record.

The plain-cut worst follows the prior fit 0.015 + 573 / sr dB to within 0.001 dB from 8 to 48 kHz.
Twelve independent searches converge on +0.0867–0.0870 at 8 kHz: the segment just before the glide
starts, whose post-lobe frames the revision lowers after its main lobe has left. **Every saved vector
replayed** (69 from the design round): final ≤ +0.0867, every reversal ≤ +0.0309; `8ab0532` (emission-
only stamping) +0.6039 / +0.4897 / +0.2993 dB on the 8 / 22.05 / 48 kHz reversal vectors. Out of the
parameter range (cuts to −40 dB): +0.0938.


### 5.4 Identity, latency and cost (the gates track, on `b712139`; the rail change touches none of it)

- **TP off — bit-identical to `8ab0532`** in 577 of 609 renders (8 kHz–192 kHz, OS off–16×, host blocks
  512 / irregular, EQ settings and position moves, Ceiling automation, bypass, offline entry); the 32
  that differ are all a Force Max offline entry without a re-prepare WITH an EQ shelf in use —
  KI-024's fix restarting the EQ, as intended (with the EQ flat, identical). **Commit 1 alone** (the TP law) is bit-identical to
  `8ab0532` with TP off in all 609.
- **TP on with nothing over the ceiling** — 2846 of 2879 idle renders at 44.1 / 48 / 96 kHz
  bit-identical; the other 33 (44.1 / 48 kHz, OS 4–16×) differ in 3–17 samples of ~53 000 by at most
  5.96e-8 (−144.5 dBFS), and `8ab0532` with ONLY the attack floor raised to 16 reproduces them
  bit-for-bit — the 4-sample change in the limiter's line length at those rates, not the gain law.
- **Reported latency** — `predictLatencySamples`, the group delay, the measured impulse and the
  bypass delay identical to `8ab0532` in all 1140 configurations (4 kHz–768 kHz × OS × phase × TP ×
  Force Max; the impulse's content differs only where the clamp acts on it).
- **CPU** (`tests/bench.cpp`, clang 22, 7 runs per cell, median ns/sample; 44.1 / 48 / 96 kHz × blocks
  64 / 512 × OS off / 4× / 16×): with TP on, final / base 0.92–1.07 — inside the 0.89–1.14 spread of
  the cells without TP, whose code did not change. Not measurable.
- The static checkers (`check-realtime` 42 files 0 violations, `check-portability` 50 / 0, their
  self-tests, the clang-warning gate's self-test) passed; both suites built with clang 22 passed.

### 5.5 The round's independent review, and the fix commit (`f03d673`)

An independent review of the integrated tree (`0f162c8`), with every regression test mutation-checked
(the full DSP suite, 722 checks, per mutant) and its own probes against `0f162c8` and `8ab0532`:

| # | Finding | Class | Evidence (review, measured unless marked) | Disposition |
|---|---|---|---|---|
| D1 | The eased attack's float weights sum to 1 + 1–3 ulp at some attack lengths (A = 22, 96, 192 → 88.2 / 384 / 768 kHz). A forward minimum at ~0 (a required gain below ~−138 dB — an astronomical input) then gives a reduction of 1 + 1.19e-7, a gain of −1.19e-7, and the backstop clips the sign-inverted product to the ceiling | **Regression of `4ff71bd`** | clamp: 256 / 256 frames at the ceiling, minimum gain −1.1920929e-07 (0.2.15: 0 non-zero frames); engine, 1e30, default settings: 255 samples at the ceiling, product meter **+1.85 dB** over at 88.2 / 384 / 768 kHz (0.2.15 silent); 48 kHz (A = 16) silent in both | **Fixed**: the weighted reduction is divided by `easeTotal`, the float weights summed in the frame loop's own order, and capped at 1 — a window of zeros answers exactly 1 and a window of ones exactly 0 at every A. A cap alone was measured insufficient: a sum just under 1 left a gain of ~6e-8, which still clipped 1e30 to the ceiling |
| D2 | A Force Max offline entry without a re-prepare empties the wet ring (`latchOsConfig`) but not the bypass leg's delay ring (`dryRing`, cleared only by `reset()`); with BYPASS on the render carries the realtime input | Pre-existing (identical in `8ab0532`); a gap in the KI-024 route C fix | −2.0 dBFS until sample 446 (44.1 kHz) / 485 (48 kHz) = render latency − 1; with a re-prepare silent; TP on and off alike | **Fixed**: the latch's branch clears `dryRing` with the EQ |
| M1 | Reverting ADR-0046 decision 4's `min(entry, emission)` stamp in the ENGINE passes the whole suite (the tests' clamp driver re-implements it) | Test gap | mutant: 0 failures of 722 | **Pinned**: `testTruePeakModeLagsAnAscentByTheEntryCeiling` |
| M2 | Removing decision 2's rise cap (`kReleaseRise`) passes the whole suite | Test gap | mutant: 0 failures of 722 | **Pinned**: `testTheClampReleaseRiseIsCapped` |
| L1 | The loudness guard's 50 ms minimum gap is not enough for the slowest ring-out, and is 49.98 ms at odd sub-block lengths | Incomplete fix in `0f162c8` | DC at 0.99 cut at the minimum admitted gap: ungated mean −115.5 LUFS after 2 s of silence (floor −120.7) in 16 of 336 runs, 8–192 kHz; integrated empty in all 336; 1102 samples at 22.05 kHz (derived) | **Fixed**: `firstCleanSubBlock` = the straddler rule + one whole sub-block (100 ms): 0 of 336, worst −120.69 LUFS. The stated cost after a reset or a bypass resume moves from up to 150 ms to up to 200 ms (ADR-0020's note, the manual) |
| C1–C5 | Comment drift: the gain-law formula omitted the μ·φ term; "~0.25 %" first step (0.290 % at A = 16, derived); "~920 samples to unity" (measured 7290 at 48 kHz to exactly 1; ~1000 to half gain); `kRiseFloor` called a gain floor (it is an additive term); "reduction in [0, 1]" (false per D1) | Documentation | as stated | **Corrected** in the code comments; ADR-0046's decision texts keep their words and its implementation note records the corrections |

The review also checked, and found no defect in: every index of `lowerInFlightCeilings` and `prepare`
for A ≥ 16 (derived bounds, and a gcov run executing every line of `lowerInFlightCeilings` over 28
rates × blocks 1–8192 under ASan / UBSan / `_GLIBCXX_ASSERTIONS`); an engine fuzz (25 rates × five
block sizes, deliveries to 8195 samples; 37 500 sanitised calls, 125 000 allocation-guarded calls, 0
allocations, 0 non-finite outputs); the function-effects tier; RESET edge cases (blocks 1–7 and 64, TP
on / off, a reset inside a bypass pause, repeated presses, a reset on a latching offline entry: 0 of 70
over the 0.2504 kernel bound, `8ab0532` 56 of 70); `firstCleanSubBlock` at every sub-block phase; and
the record's figures (46 samples / 0.958 ms, 4.91 glide steps 12–65.999 kHz, 0.004049 at 48 kHz,
0.2504 / −12.03 dB, the 0.2.15 burst figures reproduced by a boxcar + floor-8 mutant). It did not build
the state suite, and did not reproduce the branch-and-bound figures.

**The fix commit `f03d673`** (with the two CI findings of `58107a4`, §6.1): D1, D2, L1, the comments,
`<cstddef>` / `std::size_t` in `ClampTruePeakDetector.h` and `CeilingClamp.h`, and the test products
widened before multiplying. New checks: `testTheClampSilencesAnAstronomicalInput` (44.1–768 kHz, ±1e9
and 1e30 bursts: the gain in [0, 1] at every attack length, 1e30 exactly silent),
`testTheClampReleaseRiseIsCapped` (48 kHz: after a −32 dB burst the gain goes below 0.05, the rise runs
at the cap for > 50 samples, and never exceeds (1 + μ)·g + μ·φ by more than 1e-6),
`testTruePeakModeLagsAnAscentByTheEntryCeiling` (44.1 / 48 kHz, 0.9 DC pushed into a −20 → −1 dB rise:
TP-on output trails TP-off by more than half the D-sample glide in > 90 % of the ascent, and is never
above it by more than 1e-4), a bypass case in `testAForceMaxEntryStartsTheRenderClean`, and a part 6
in `testStatisticsResetStartsTheSessionAtTheReset` (the DC ring-out at the minimum gap, 22.05 / 48 /
96 kHz, fills L/2 − 1 / L/2 / L/2 + 1).

**Mutation on the fixed tree** (the full DSP suite, 737 checks, per mutant): the emission-only engine
stamp — 2 failures (`tpAscent`, both rates); the uncapped rise — 1 (`clampRise`); the unnormalised ease
— 2 (`clampAstro`, both); the dry ring left — 1 (`forceMaxEntry`, the bypass render); the 50 ms guard —
1 (`statsReset`, the ungated energy). Five mutants, five killed.

**Re-measured on the fixed tree:**

- **Engine TP matrix** (the §5.2 harness, 14 200 renders — 1775 at each of 12 / 16 / 22.05 / 32 / 44.1
  / 48 / 96 / 192 kHz): 93.0–96.8 % bit-identical per rate to the same jobs on `0f162c8`; every
  reading within 0.0017 dB of it; the worst at every rate unchanged, +0.0426 / +0.0380 dB; the only
  renders over 0.1 dB the same 21 Force Max splice renders (10 at 22.05 kHz, 11 at 44.1 kHz; KI-024
  route B).
- **Astronomical input on the engine** (the review's probe, TP on, −1 dBTP, a 256-sample burst): 1e30
  **silenced at every rate** (0 samples at the ceiling; was 255 at 88.2 / 384 / 768 kHz). At 1e9
  (+180 dBFS), default settings: 48 kHz −5.25 dB (0.2.15 +0.78), 88.2 kHz +0.89 (+0.89), 384 kHz
  **+1.96** (+1.81), 768 kHz +1.85 (+1.85); Punchy + transients 100 %: 48 kHz −8.53 (0.2.15 +1.64),
  silent at the others (as 0.2.15). The 1e9 class is float's gain resolution just below 1 (5.96e-8,
  −144.5 dB) and is older than this round → **KI-027** (recorded, not changed).
- **Suites** — GCC 13 LTO: `AnabasisTests` 737 / 737, `AnabasisStateTests` 1605 / 1605.

## 6. Gates — locally on `0f162c8`, and on the fixed head `f03d673`

The local gates below ran on `0f162c8` (the head the review read). On `f03d673` the suites, the
mutation run and the engine matrix were re-run locally (§5.5); its realtime, sanitizer, valgrind and
pluginval gates are GitHub CI's (§6.1), not re-run locally.


- **Suites** — GCC 13 LTO (the `linux-lto-tests` class): `AnabasisTests` 722 / 722, `AnabasisStateTests`
  1605 / 1605; GCC non-LTO at each code commit: commit 1 676 / 1599, the head 722 / 1605.
- **pluginval** at the `build.yml` strictness, editor under Xvfb, VST3 built from the final code:
  deterministic 3 / 3, randomise 3 / 3, and one seeded pass over 8 / 11.025 / 12 / 16 / 22.05 / 32 /
  44.1 / 48 / 96 / 192 kHz × blocks 1 / 16 / 64 / 512 / 1024 — all passed. **Warning:** on the build
  before the rail moved, one randomise attempt (seed `0x37bc7e`) segfaulted at validator exit and the
  script's Linux crash-retry passed it; that seed replayed 29 times with the editor tests (1 more
  exit segfault, after `SUCCESS`) and 3 times without them (clean), 8 times under gdb (clean). The
  third round recorded the same exit crash on `dd983ec`, so it predates this round; not diagnosed.
- **Realtime** — the RTSan liveness canary aborts with a RealtimeSanitizer report (exit 43); the
  function-effects tier (`tests/realtime_effects.cpp`, clang 22, `-Werror=function-effects`) compiles
  clean, and its canary fails with the `-Wfunction-effects` diagnostic; the DSP suite built
  RelWithDebInfo with `-fsanitize=realtime` (the `realtime` job's configuration) passes 717 checks
  (the allocation guard stands down in that lane) with **0 RealtimeSanitizer reports**.
- **valgrind memcheck** (the `sanitizers` job's flags — clang 22, Release, the allocation guard compiled
  out; `--track-origins=yes --error-exitcode=1`): `AnabasisTests` PASS 717, `AnabasisStateTests` PASS 1605,
  **0 errors** in each. (The gates track's own valgrind runs on `b712139` were cut off by a container
  restart and are not counted.)
- **Docs** — `check-docs` clean; `check-citations` clean against `8ab0532`, `ed06ad0` and `origin/main`
  after re-anchoring 43 anchors the three code commits moved; `check-realtime` and `check-portability`
  clean.

### 6.1 GitHub CI

- **`58107a4`** (the ADR / KI records commit; the three code commits under it): push run 36487308268 —
  `docs`, `preflight`, `source-lint` (citation gate included), `linux` (suites, reproduction, probe,
  pluginval deterministic ×3 / randomise ×3, ABI floor, warning gate), `linux-lto-tests`,
  `linux-lto-clang`, `realtime`, `sanitizers` (ASan / UBSan and valgrind on both suites), `windows` and
  `macos-intel` (each with its pluginval lanes) **success**; **`macos` FAILED at its build step** —
  AppleClang / libc++ (the universal arm64 + x86_64 build) rejected an unqualified `size_t` in
  `ClampTruePeakDetector.h`, which `Latency.h` → `CeilingClamp.h` parses before anything declares it
  (the Intel job's toolchain accepted it; GCC and clang with libstdc++ locally did too, so it was not
  reproducible here). Pull-request run 36487313107 `merge-check` success; PREfast 36487313119
  success; dependency review success. **CodeQL** (36487313171): the workflow succeeded, but its check
  reported **1 new high-severity alert** in the changed code — "Multiplication result converted to
  larger type: Multiplication result may overflow 'int' before it is converted to 'long long'", on
  `tests/dsp_tests.cpp` line 6695 of `58107a4`: `const long long R = (96000 / B + 1) * B;` in the
  reset test (read from the check run's annotation). Widened before multiplying, with the other such
  products in the round's tests. Both fixed in `f03d673`.
- **`f03d673`** — push run 36492315079 — `docs`, `preflight`, `source-lint` (citation gate
  included), `linux` (suites, reproduction, probe, pluginval deterministic ×3 / randomise ×3, ABI floor,
  warning gate), `linux-lto-tests`, `linux-lto-clang`, `realtime` (RTSan canary, effects tier, RTSan
  DSP suite), `sanitizers` (ASan / UBSan, then valgrind memcheck on both suites), `windows`, `macos`
  (the universal build that failed on `58107a4`: build, self-tests on both slices, VST3 and AU
  pluginval both modes ×3, channel probes, packaging) and `macos-intel`: **all success**. Pull-request
  run 36492321791 `merge-check` **success**; CodeQL 36492321797 (actions, c-cpp) **success — "No new
  alerts in code changed by this pull request"**, so `58107a4`'s alert is gone; PREfast 36492321799
  success (warnings below); dependency review 36492321802 success. **No failed or cancelled check on
  `f03d673`.**
- **PREfast, both heads:** alerts in the PR's changed code 94 on `8ab0532` → 100 on `58107a4` → 101
  on `f03d673`; every one is a C6262 (a test function's stack above 16 KB — an engine or a fixture held
  by value) under `tests/`, plus the two C6011 of the 2026-09-03 scanner audit's group G3 ("DO NOT
  FIX"). The 7 added are this round's new test functions. None under `src/`. **Warning**, as in the
  earlier rounds: recorded, not dismissed.

## 7. GR max and the tooltip

- **GR max — FIXED, verified (regression only).** The fix (`e380d72`) is unchanged on the final tree
  (`git diff e380d72 HEAD -- src/gui/GrHistoryView.{h,cpp} src/dsp/GrHistoryBuffer.h` is empty), and its
  two tests — `testTheGrReadoutReadsTheRingItNames`, `testTheTickMaxIsTheDeepestEntryTheGraphDraws` —
  pass in the final state suite. Their failure on the old range was established in the third round
  (7 checks) and not re-run here.
- **Tooltip — OQ-018 resolved as option 1.** `LoudnessMeterView::tooltipText` returns "Waveform
  statistics off the output", verified verbatim against the first sentence of `d1640bb`'s 0.1.1
  wording (ADR-0020). No new copy; the code comment now points at the resolution. "lim GR" / "GR max"
  stay as shipped (OQ-019 open — the brief keeps them unless inaccurate, and they are accurate: the
  limiter's reduction now and the deepest in the drawn history).

## 8. Not verified

- **An all-input bound** for the true-peak promise at any rate: the derived bounds cover inputs not
  already under reduction when a cut arrives; the rest is covered by a relaxation search and by
  engine and clamp searches, which give lower bounds on the worst case.
- **Listening**: no change in this round was heard — the eased attack and the release cap change
  TP-on output wherever the clamp acts.
- **Hosts**: no DAW was run at a sample rate below 44.1 kHz, under Ceiling automation in true-peak
  mode, across a statistics RESET, or across an offline entry without a re-prepare; which hosts enter
  offline without re-preparing is not verified in a DAW.
- **The Ceiling unit below the rail** in a real editor or generic host view (the state test drives the
  host-facing `getText`, headlessly).
- **The pluginval exit segfault** (pre-existing): not diagnosed.
- **The KI-024 D / Dd routes** and **KI-026** are recorded, not changed; **a clean re-prepared Force Max
  render trimmed by the reported latency reads +0.40 / +0.50 dB at its head** (the 16× oversampler's
  energy ahead of its integer latency) — observed, not investigated.
- **The reviewer's KI-024 figure** (+0.96 / +0.98 dB) was not reproduced (+0.884 / +0.918 here).
- **The review's fixes** (`f03d673`) were verified by the suites, by mutation and by re-running the
  engine matrix and the astronomical probe — not by a second independent review; their realtime,
  sanitizer, valgrind and pluginval gates are CI's (§6.1), not local runs.
- **KI-027** (a finite input near +180 dBFS reads up to +1.96 dB over at 384 kHz; 0.2.15 +1.81) is
  recorded, not changed; the 1e9 reading is not asserted by any test.
- **The review's unexercised items**: the Ceiling unit after `prepareToPlay (0, …)` (a conforming host
  cannot supply rate 0; derived only) and a text query during the processor's destruction (none
  found; the hazard ADR-0046 records).
