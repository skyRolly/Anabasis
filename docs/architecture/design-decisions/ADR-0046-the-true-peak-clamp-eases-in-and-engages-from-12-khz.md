# ADR-0046 — The true-peak clamp eases in, and true-peak mode engages from 12 kHz

**Status:** **Accepted — 2026-09-28, on the owner's direction** (the instruction of record for the
fourth review round on PR #42: "When the user enables true-peak protection, the output must obey the
declared ceiling across every supported processing path"; "Close KI-025 completely … Do not simply
widen the tolerance"; "Do not allow an unsupported assumption about sample-rate support to hide a
real contract violation"; "The owner authorizes proceeding through ADR/decision gates"), with the
defect reproduced, three designs built and measured, and the chosen one verified on the integrated
tree before this record was accepted. ⊕ Flagged for the owner's review of the pull request that
carries it. It is an Architecture Review Gate item on five counts, named here rather than left to be
found:

1. a **change to the ceiling clamp's gain law** (`DSP_POLICY.md` Enforcement: "any change to … the
   ceiling clamp") — the attack's shape and floor, a release rate cap, and the revision's reach;
2. a **change to the true-peak path's delay composition** — D = 46 samples at every rate below
   66 kHz, where it was 38–42 — amending the figures of Accepted
   [ADR-0041](ADR-0041-clamp-true-peak-path-inside-the-latency-allowance.md) decision 4 (the
   composition rule and the **reported** latency do not move by a sample);
3. a **conflict with Accepted [ADR-0045](ADR-0045-true-peak-mode-answers-to-the-ceiling-in-force-at-emission.md)**
   decisions 1, 2 and 5 (which ceiling a frame is stamped with, how far a revision reaches, and the
   step figure in its prescribed policy text);
4. a **supported-rate contract** for true-peak mode (engaged from 12 kHz; below it the sample clip),
   which rewrites `DSP_POLICY.md` invariant 4's "any sample rate" and changes behaviour at
   3901–11999 Hz — 8 and 11.025 kHz included — where 0.2.15 engaged the path;
5. a **new cross-thread read** — the Ceiling's value text, which any thread may request, now reads the
   prepared sample rate the GR history ring already publishes — amending Accepted
   [ADR-0015](ADR-0015-pre-ship-contract-refreeze.md) item 5 (the unit follows the path, not the
   switch). No new atomic, no new ordering, no new writer.

No parameter, saved-state, signal-order or reported-latency change; TP-off output is bit-identical.
Review finding "Low-rate ceiling cuts exceed dBTP limit" (PR #42, `CeilingClamp.h`, 2026-09-28);
`KNOWN_ISSUES.md` KI-025, which this record closes.

## Context

ADR-0041's clamp bounds each segment's interpolated reading through a chain of windows over a
per-segment requirement: r[j] (the gain segment j may take), q[k] = the 32-wide minimum of r over
every segment that reads x[k], m[k] = the forward minimum of q over the attack A, the attack ramp
ga[k] = the boxcar mean of m over A samples, a 10 ms one-pole release that can only hold the gain
lower, and the sample clip as backstop. It delays its audio by D = A + 30, taken out of the 10 ms
allowance (ADR-0041 decision 4); A = max(8, round(0.25 ms · sr)).

**The requirement bounds a segment's reading only while the gains its interpolation reads are
equal.** With y = g·x, G the largest gain in a segment's 12-tap defining window and h the kernel,

    reading(y) ≤ G·reading(x) + Σ_k (G − g_k)·|h_k|·|x_k|

— the second term is what a ramp starting beside a segment adds through the kernel's negative
lobes. The boxcar ramp puts its steepest slope at its TOP, next to segments sitting at the ceiling
with no headroom, and at its 8-sample floor (every rate up to 32 kHz) that slope is 1/8 of the whole
reduction per sample. A search for worst-case bursts hill-climbed on the real 0.2.15 engine (the
third round's worklog) found, over the live ceiling on the BS.1770 Annex 2 meter: **+0.157 dB at
22.05 kHz and +0.125 dB at 32 kHz** at the bottom of a fast Ceiling cut, **+0.113 dB at 8 kHz and
+0.116 dB at 16 kHz** on a static ceiling, +0.23 dB at 4 kHz (KI-025). This round's review raised the
same thing against `CeilingClamp.h` ("Low-rate ceiling cuts exceed dBTP limit").

Two more gaps turned up while the fix was designed, both in ADR-0045's machinery and both larger
than the finding:

- **A Ceiling reversal.** ADR-0045 stamps each frame with the ceiling predicted at its emission.
  During an UPWARD glide the frames in flight carry the rising trajectory; a new downward target
  mid-ascent then lowers those stamps by both slopes at once, and the segment straddling the
  emission point reads over through its lowered post-lobe taps. Measured at clamp level on 0.2.15
  (stereo linked, the engine's stamping and revision, a −20 → 0 → −20 dB reversal): **+0.30 dB at
  48 kHz, +0.49 dB at 22.05 kHz, +0.60 dB at 8 kHz** (Annex 2). Not reproduced at engine level —
  the limiter ahead of the clamp absorbs it on every programme tried — but the clamp is the
  guarantor for arbitrary post-EQ input, so it is a defect in the clamp's contract.
- **The rate boundary nobody had stated.** The engine accepts any positive rate; the only rail kept
  the true-peak path where its delay fits the allowance, which at the 8-sample floor was every
  integer rate from 3901 Hz. Below that the sample clip ran silently while the Ceiling still read
  `dBTP`. Nothing in the repository says which rates are supported (`COMPATIBILITY_MATRIX.md` had
  no rate row; the tests engaged TP only at 44.1–192 kHz; pluginval ran 44.1/48/96 kHz).

## Problem

How to make the true-peak output obey the declared ceiling, within the existing 0.1 dB tolerance on
both defining meters (ADR-0043), during the whole of a downward transition and on a static ceiling,
at every rate the product claims — without widening the tolerance, without moving the reported
latency, without touching TP-off, and allocation-free and bounded; and to state the rates it
claims.

## Options

Three designs were built on scratch copies of the 0.2.15 engine by independent designers, each
evaluated by an independent evaluator on the real engine and at clamp level, and judged against the
owner's criteria (correctness under automation, live ceiling, fixed latency, CPU, smoothness, TP-off
identity, steady state, oversampling, rates, complexity). Every figure below is over the live
ceiling, Annex 2 unless marked; the round's worklog §2 carries the method and the logs.

- **A. Two-stage verify-and-correct.** The existing law, then a second stage that re-reads its own
  output and corrects what the first let through, with a knapsack-derived static bound (0.0653 dB)
  over all inputs. **Rejected.** It fails the automation criterion at engine level on both meters
  (a burst under Ceiling automation at 12 kHz: +0.1007 dB product meter / +0.1375 dB Annex 2,
  reproduced by the judge at host blocks of 8, 16 and 64); its delay (116 samples at 48 kHz)
  engages TP only from 11701 Hz (8 and 11.025 kHz lose TP — as they now do under C too, for a different reason: decision 5); whole-engine CPU +7–28 %, over the
  ~5 % target at 48 kHz with 4× oversampling; the limiter's longest window 74 samples shorter; the
  dip after a cut 4.6× deeper (−0.88 dB). Its bound's premise (|y| ≤ c) is false at a downward
  retarget.
- **B. A longer attack floor (24 samples), with the delay taken from the allowance.** The finding's
  own suggestion. **Rejected.** It breaches on a static ceiling at the common rates behind a +12 dB
  Post shelf (+0.159 / +0.154 / +0.167 dB at 32 / 44.1 / 48 kHz — where 0.2.15 and C read ≤ 0), fails
  at the start of a cut at 8, 11.025 and 5 kHz (+0.375 dB at 8 kHz), and its designer's own
  derivation shows no attack LENGTH closes it: the boxcar's top slope is the problem, not its
  length.
- **C. An eased single-stage law.** Keep one stage and change the ramp's SHAPE so its steep part sits
  where the segments have headroom; cap the release's relative slope; narrow what a revision
  reaches; engage the path from a stated rate. **Chosen**, with two changes the judge required and
  measured before accepting it: the reversal fix (decision 4) and a derived bound for the revision
  step (the Evidence).

| | A | B | C (chosen) |
|---|---|---|---|
| Static ceiling | ≤ +0.000 on saved bursts; bound 0.0653 | **+0.167 at 48 kHz** (Post +12) | ≤ +0.042 (stereo clamp climbs); LP ≤ 0.035–0.061 |
| Start of a full-range cut | ≤ +0.000 (engine) | **+0.375 at 8 kHz** | clamp search +0.0867 at 8 kHz, +0.0629 at 12 kHz; **derived bound +0.1008 at 8 kHz, +0.0672 at 12 kHz** (Evidence) |
| Ceiling reversal | **+0.1375 at 12 kHz (engine)** | not measured | +0.215 at 8 kHz as submitted; **≤ +0.031 with decision 4** |
| TP engages from | 11701 Hz | 3901 Hz | 8000 Hz as submitted; **12000 Hz** after the bound (decision 5) |
| Clamp delay D at 48 kHz | 116 | 54 | 46 |
| Reported latency | unchanged | unchanged | unchanged |
| TP-off | bit-identical | bit-identical | bit-identical |
| CPU | +7–28 % | clamp +3–15 % | not measurable (−4 % / +3 %) |
| Step at a −1 → −20 dB cut (glide steps; 0.2.15: 22.5) | 8.9 | 28.5 | 4.9 |

## Decision

1. **The attack eases in.** The ramp is the forward minimum averaged with weights that grow
   geometrically with AGE, `w_a ∝ e^(λ·a)`, `λ·A = 4.8` (`kEaseSpan`), not a boxcar: it leaves the
   level it starts from with a first step of ~0.25 % of its depth and reaches the deeper level with
   its steepest steps, where every segment reading them sits under its own requirement with
   headroom. It is summed on the REDUCTIONS 1 − m, so a window of ones answers exactly 0 whatever
   the weights round to (the idle path stays bit-exact). The attack floor is **16 samples**:
   `A = max(16, round(0.25 ms · sr))` (`kMinAttackSamples`), because the shape is a balance, not a
   monotone knob — A = 12 at the same λ·A reads +0.13 dB, λ = 0.6 reads +0.57 dB at 20 dB depth.
2. **The release rises at most 1 % per sample** (`kReleaseRise`, with a −80 dB floor `kRiseFloor`
   so a gain silenced to exactly 0 by an overflowing estimate can recover). A release that runs into
   a later, lower requirement meets it at its own slope with no headroom on the far side, and at low
   rates the 10 ms one-pole's relative slope is large (+0.48 dB at 4 kHz, +0.24 dB at 8 kHz for a
   20 dB release into a 12 dB cap, uncapped); capped, +0.061 dB. It binds only below ~−15 dB of
   clamp reduction at 44.1 kHz, so ordinary reductions release exactly as before.
3. **A revision reaches only what a revised requirement defines** (amends ADR-0045 decision 2's
   mechanism, not its promise). The step a revision forces at the emission point is set by how far
   and how early a revised requirement reaches. A revised r_j now constrains only the frames its two
   defining readings read, x[j−5 .. j+6]; on the taps BEFORE its main lobe it is judged against the
   glide `kRevisionLead` = 1 frame after the tap rather than against its own lower value; the
   ENTRY-TIME requirements (`segReqEntry`, planned with the attack's lookahead) keep their full
   32-sample reach. What the relaxation lets through is the glide's fall between frame k + 1 and the
   segment on taps before the main lobe — ≤ 0.81 glide steps of the ceiling on every defining phase.
   The one-sample step at a downward retarget with the output at the ceiling falls from ~(16 + A/2
   + 1) to **~4.9 glide steps at every rate below 66 kHz** (0.0040 on 0.9 DC at 48 kHz for a −1 →
   −20 dB cut, where 0.2.15 stepped 0.0185).
4. **A true-peak frame answers to the LOWER of the ceiling at its entry and the ceiling predicted at
   its emission** (amends ADR-0045 decision 1): `ceilEmitArr[n] = min(ceilingAhead, ceilArr[n])` in
   stage A. Identical on a static ceiling and during a descent (the emission value is the lower
   there); during an ascent a frame answers to its entry value, so a reversal mid-ascent revises
   stamps that were never raised onto the rising trajectory. The same array feeds the clamp stamp
   and the limiter's TP-mode threshold (ADR-0045 decision 3), so both move together.
5. **True-peak mode engages from 12 kHz** — `truePeakPathEngages (sr)`: `sr ≥
   CeilingClamp::kMinTruePeakRate` (12000) and the path fits the allowance with the 0.5 ms minimum
   window left (`src/dsp/Latency.h`). ONE predicate decides both the engine's rail
   (`AnabasisEngine::prepare`) and the Ceiling's unit (`CeilingUnitSource::truePeakEngaged`), so the
   unit cannot claim `dBTP` where the path is not running: below 12 kHz with TP on, the Ceiling reads
   ` dB` and the sample clip runs. The unit reads the prepared rate the processor ALREADY publishes —
   the GR history ring's pair (`GrHistoryBuffer::prepared()`, KI-017), one relaxed load of one
   scalar, no pairing needed and so no epoch bracket; before the first prepare (rate 0) it answers
   as at 48 kHz, the fallback every view uses.
   **Why 12 kHz, and not the 8 kHz the design submitted.** The static figure is per sample and so the
   same at every rate; the step a downward retarget forces is (Z + 1 + E) glide steps, and a
   full-range cut's glide step is 0.9 / (0.02 · sr), so its excess grows as the rate falls. The
   design chose 8 kHz from a clamp-level SEARCH (+0.0867 dB there). The verification of the
   integrated tree then DERIVED bounds (Evidence): at 8 kHz the tightest one — a global
   branch-and-bound over the straddling segment's readings, for inputs not already under reduction —
   is **+0.1008 dB**, over the tolerance; a search over requirement sequences with an exact inner
   maximiser, covering inputs already under reduction when the cut arrives (a relaxation's search
   value, neither a bound nor a realised input), reaches +0.1213 dB at 8 kHz and **+0.0999 dB at
   11.025 kHz — at the tolerance, still rising when stopped**; at **12 kHz every figure is under it**
   (bound +0.0672, relaxation search +0.0930, clamp search +0.0629). The rail follows what can be
   supported, not what was found: no search found a real input over the tolerance at 8 kHz.
   **The owner's decision recorded (⊕):** 3901–11999 Hz — 8 and 11.025 kHz among them — where 0.2.15
   engaged the path (with the KI-025 residual, +0.21 / +0.23 dB static at 8 / 11.025 kHz), now runs
   the sample clip: inter-sample peaks can then exceed the Ceiling by ~2 dB (+1.87 dB Annex 2 measured
   on ordinary programme with inter-sample bursts at 3901–7999 Hz; +2.3 dB on the saved 4 kHz bursts).
   The alternative on record: keep the clamp engaged below 12 kHz as a best-effort mode without the
   0.1 dB claim (unit ` dB`) — +0.087 dB by clamp search at 8 kHz, +0.13 dB at 4801 Hz, but unbounded
   by derivation. Not taken, because a mode that runs the true-peak path while the product withholds
   its promise is a second contract to explain, at rates no listed host uses; the owner may take it.
6. **Policy amendment (prescribed text, `ADR_POLICY.md` rule 5).** `DSP_POLICY.md` invariant 4:
   "any sample rate" becomes *any host sample rate — true-peak mode's inter-sample promise holds
   wherever the path engages, 12 kHz and up (`truePeakPathEngages`, ADR-0046); below 12 kHz the
   ceiling holds on sample peaks and the Ceiling reads dB*, and the guard list gains
   `testTruePeakModeHoldsTheCeilingBelow44k`. Invariant 8's second yielding place (ADR-0045 decision
   5) keeps its words with the figure *(Z + 1 + E) ≈ 4.9 glide steps — 0.0040 on 0.9 DC for a −1 →
   −20 dB cut at 48 kHz* in place of *(16 + A/2 + 1) glide steps — 0.0185*.

## Consequences

- **KI-025 is closed** at every engaged rate (12 kHz and up), the tolerance unchanged: over a
  24 167-render engine matrix per tree (12 kHz–768 kHz engaged, OS off–16× and Force Max, host blocks
  1–512 and irregular, 13 Ceiling automation shapes, a lifecycle tier) the worst reading at every
  engaged rate is +0.0426 dB (product meter) / +0.0380 dB (Annex 2), and ≤ +0.0081 / +0.0125 dB away
  from lifecycle splices; the one class over 0.1 dB is the stream splice at a Force Max offline entry
  without a re-prepare, bit-identical on 0.2.15 and dispositioned with KI-024 — where 0.2.15 read over
  0.1 dB in 378 renders outside it. Engine climbs: +0.0308 dB at 12 kHz, +0.0219 at 22.05 kHz, +0.0096
  at 48 kHz.
- **TP-on output changes wherever the clamp acts** (the eased attack, the rise cap) — up to 0.227
  absolute on a hot matrix at 96 kHz, including rates whose D is unchanged. With nothing over the
  ceiling the path is still exact (bit-identical output, only delayed). **TP-off is bit-identical.**
- **The limiter's longest window in TP mode is shorter below 66 kHz**: 10 ms − 46 samples —
  **8.96 ms at 44.1 kHz, 9.04 ms at 48 kHz** (was 9.07 / 9.125), 7.94 ms at 22.05 kHz, 6.17 ms at
  12 kHz; unchanged at 88.2 kHz and above. The 2 ms default window is unaffected at every engaged
  rate. The reported latency is `ceil(0.01 · sr) + the OS table` in both modes, as before.
- **Slower recovery from DEEP clamp reductions** — the rise cap: on DC at 0 dBFS through a −20 → 0 dB
  glide at 8 kHz, clamp-only, the mean clamp gain over the glide is −9.6 dB against −3.5 dB before
  (−1.07 / −0.89 dB at 48 kHz). The clamp acts on what the limiter leaves, so on programme this is
  the tail of a transient the limiter did not catch.
- **An upward glide reaches the output up to D samples later in TP mode** (decision 4) — under the
  ceiling, never over; ≤ 0.6 dB of extra undershoot in the clamp-only glide test.
- **The one-sample step at a downward retarget is 4–5× smaller** (decision 3).
- **Below 12 kHz true-peak mode is not available** (decision 5), stated in `USER_MANUAL.md` §3.2,
  `COMPATIBILITY_MATRIX.md` §Sample rates and here; whether the editor should say more than the
  unit is new copy, `OPEN_QUESTIONS.md` OQ-020.
- **Cost.** Per sample, an A-term weighted sum while anything is below 1 (was an A-term sum); the
  revision's rebuild ~40A + 290 operations once per block at most (~930 at 48 kHz). Whole-engine
  CPU is not measurably changed: with TP on, the integrated tree over 0.2.15 is 0.92–1.07 (medians of 7 runs at 44.1 / 48 / 96 kHz × blocks 64 / 512 × OS off / 4× / 16×), inside the 0.89–1.14 spread of the TP-off cells, whose code did not change. Memory: two more rings of 2A + 32 floats.
- **Teardown.** `CeilingUnitSource::preparedPair` points at a processor member, like `truePeakRaw`
  before it: a parameter-text query DURING the processor's destruction would read a destroyed
  object. Nothing queries text then (the hazard `PluginProcessor.h` already records for
  `truePeakRaw`); unchanged in kind.
- **Forecloses** a boxcar attack ramp and an attack shorter than 16 samples in the true-peak path,
  a revision whose reach exceeds what the revised reading defines, and claiming dBTP at a rate the
  path does not engage, without superseding this record.

## Related code

- `src/dsp/CeilingClamp.h` — the banner's "WHY THE RAMP'S SHAPE IS THE GUARANTEE"; `kMinAttackSamples`,
  `kEaseSpan`, `kReleaseRise`, `kRiseFloor`, `kRevisionLead`, `kMinTruePeakRate`; `easeWeight`;
  `segReqEntry`; `lowerInFlightCeilings`
- `src/dsp/AnabasisEngine.cpp` — stage A's `ceilEmitArr` (decision 4); `prepare`'s rail
- `src/dsp/Latency.h` — `truePeakPathEngages`
- `src/PluginParameters.h` — `CeilingUnitSource::preparedPair`, `truePeakEngaged`; `src/PluginProcessor.cpp` (the wiring)
- `tests/dsp_tests.cpp` — `testTruePeakModeHoldsTheCeilingBelow44k`, `testTruePeakModeBoundsTheStepAtACeilingCut`
  (pin 7 glide steps), `testCeilingClampTruePeakPath` (the delay pin, 46); `tests/state_tests.cpp` —
  `testTheCeilingUnitFollowsTheRateTheTruePeakPathEngagesAt`

## Evidence

Confidence: **Verified** at engine and clamp level — synthetic programme, constructed bursts and searched vectors, both defining meters — at every engaged rate (12 kHz and up); **derived** bounds for the retarget step over inputs not already under reduction; **no all-input bound**. Not heard (no listening was performed); not run in a host at a low rate or under Ceiling automation.

- `testTruePeakModeHoldsTheCeilingBelow44k` (the KI-025 bursts at 16 / 22.05 / 32 kHz where they
  were found, and the 8 kHz one below the rail; 360 renders over five rates 12–32 kHz × four bursts ×
  three cuts × OS off / 2× / 4× × two offsets; the clamp-level reversal and cut vectors at 8 kHz
  driven as the engine drives them; the rail at 11999 / 12000 Hz and the engaged window at 10 ms)
  **fails 12 checks on the 0.2.15 engine** — the delay and step pins, four rail checks, the three
  bursts (+0.1566 / +0.1250 / +0.1157 dB), the matrix (+0.1179 dB), the reversal premise — and passes
  on this one.
- **The bounds** (the fourth round's verification, a Python model of the final law checked against
  the C++ clamp to 2e-6 in gain): the revision step at a plain full-range cut, for inputs whose
  readings never exceed the pre-cut ceiling, by a global branch-and-bound keeping every reading of
  the 23 segments around the straddling one (the exact code reach, Z = 1, the eased weights, the
  release): **+0.1008 / +0.0733 / +0.0672 / +0.0504 / +0.0366 / +0.0252 / +0.0183 / +0.0168 dB**
  (Annex 2) at 8 / 11.025 / 12 / 16 / 22.05 / 32 / 44.1 / 48 kHz; the judge's knapsack relaxation
  +0.1159 / +0.0844 / +0.0774 at 8 / 11.025 / 12 kHz. These cover that input class only — clamp
  searches with inputs already under reduction beat them at 16 kHz and above (by ≤ 0.010 dB) — so the
  all-input case is covered by a search over requirement sequences with an exact inner maximiser:
  +0.1213 / +0.0999 / +0.0930 / +0.0784 dB at 8 / 11.025 / 12 / 16 kHz (fit ≈ 0.031 + 762 / sr,
  crossing 0.1 dB near 11.05 kHz), a relaxation's search value that no real input has realised (the
  8 kHz pattern replayed on the clamp reads −0.149 dB). The static figure over arbitrary requirement
  sequences (an outer search, exact inner LP): +0.0612 dB at 8 and 16 kHz, +0.0495 at 48 kHz (a
  release running into a cap). **No all-input bound was obtained at any rate** — the rigorous model
  did not converge; the promise at 12 kHz and above rests on the bound for the revision-only class,
  the relaxation search for the rest, and the clamp and engine searches below.
- **The engine matrix** (24 167 renders per tree, 0.2.15 alongside; the worklog §5.2): the worst at
  every engaged rate +0.0426 / +0.0380 dB; the only class over 0.1 dB the Force Max offline-entry
  splice, bit-identical on 0.2.15 (KI-024); 0.2.15 over 0.1 dB in 378 renders outside it, the
  integrated tree in none; 142 seeded engine climbs (1.1 million renders) at 8 / 11.025 / 22.05 /
  48 kHz, ≤ +0.0728 dB, and 20 more at 12 kHz, ≤ +0.0308. Reported latency identical to 0.2.15 in
  every render and equal to the measured impulse delay.
- **The clamp adversary** (77 searches, stereo linked, blocks 1–512; §5.3): nothing over 0.1 dB; plain
  cuts +0.0870 / +0.0629 / +0.0280 dB at 8 / 12 / 48 kHz following 0.015 + 573 / sr; every reversal
  ≤ +0.0454 with decision 4's stamp, where 0.2.15's own stamping reads +0.60 / +0.49 / +0.30 dB.
- **Identity, latency, realtime** (the worklog §5.4 and §6): TP off bit-identical to 0.2.15 except a
  Force Max offline entry without a re-prepare with an EQ shelf in use (ADR-0046 alone: identical in
  all 609 renders); TP on with nothing over the ceiling bit-identical except 33 of 2879 renders at
  44.1 / 48 kHz with OS 4–16×, ≤ −144.5 dBFS in 3–17 samples, reproduced by 0.2.15 with only the
  16-sample floor (the line-length change); reported latency, group delay and the measured impulse
  identical in 1140 configurations; the function-effects tier clean with the revision in it.
- The design round (three designs, three evaluations, a judge's own measurements) and the
  verification of the integrated tree: the fourth-round worklog.
- Depends on: ADR-0041 (the path; decision 4's figures amended), ADR-0043 (the two meters), ADR-0045
  (the emission-time ceiling; decisions 1, 2 and 5 amended), ADR-0015 item 5 (the unit; amended).
