# ADR-0046 — The true-peak clamp eases in, and true-peak mode engages from 12 kHz

> **✅ RATIFIED — THE ARCHITECTURE REVIEW GATE IS CLEARED (2026-09-29).** The owner accepted this
> record at the gate as it stands, with its 2026-09-28 implementation note (the instruction of
> record is in the Status below; what acceptance re-verified, and three code facts the record had
> not stated, are in the ratification note after the implementation note). It had been taken on the
> owner's direction on 2026-09-28 (the instruction of record for the fourth review round on PR #42:
> "When the user enables true-peak protection, the output must obey the declared ceiling across
> every supported processing path"; "Close KI-025 completely … Do not simply widen the tolerance";
> "Do not allow an unsupported assumption about sample-rate support to hide a real contract
> violation"; "The owner authorizes proceeding through ADR/decision gates"), with the defect
> reproduced, three designs built and measured, and the chosen one verified on the integrated tree
> before it was filed. How it arrived stays in the record, and so does why the architecture changed
> (Context): the clamp's boxcar attack bounded a segment's reading only while the gains its
> interpolation read were equal, ADR-0045's stamping let a Ceiling reversal mid-ascent read over,
> and no record stated which rates the true-peak promise covered. It was an Architecture Review Gate
> item on five counts, and they were named here rather than left to be found:
>
> 1. a **change to the ceiling clamp's gain law** (`DSP_POLICY.md` Enforcement: "any change to … the
>    ceiling clamp") — the attack's shape and floor, a release rate cap, and the revision's reach;
> 2. a **change to the true-peak path's delay composition** — D = 46 samples at every rate below
>    66 kHz, where it was 38–42 — amending the figures of Accepted
>    [ADR-0041](ADR-0041-clamp-true-peak-path-inside-the-latency-allowance.md) decision 4 (the
>    composition rule and the **reported** latency do not move by a sample);
> 3. a **conflict with Accepted [ADR-0045](ADR-0045-true-peak-mode-answers-to-the-ceiling-in-force-at-emission.md)**
>    decisions 1, 2 and 5 (which ceiling a frame is stamped with, how far a revision reaches, and the
>    step figure in its prescribed policy text);
> 4. a **supported-rate contract** for true-peak mode (engaged from 12 kHz; below it the sample clip),
>    which rewrites `DSP_POLICY.md` invariant 4's "any sample rate" and changes behaviour at
>    3901–11999 Hz — 8 and 11.025 kHz included — where 0.2.15 engaged the path;
> 5. a **new cross-thread read** — the Ceiling's value text, which any thread may request, now reads the
>    prepared sample rate the GR history ring already publishes — amending Accepted
>    [ADR-0015](ADR-0015-pre-ship-contract-refreeze.md) item 5 (the unit follows the path, not the
>    switch). No new atomic, no new ordering, no new writer.
>
> It was filed `Accepted` on that direction, ⊕ flagged for the owner's review of the pull request
> that carried it as gate items a green build does not clear, and held there until the owner
> answered.

**Status:** **Accepted — 2026-09-28, on the owner's direction; ratified at the Architecture Review
Gate on 2026-09-29**, on the owner's explicit approval (the instruction of record, the owner's brief
of 2026-09-29: "The owner has authorized proceeding through ADR decisions." "Change ADR-0046 from
Proposed to Accepted according to repository conventions." "Update the related amended ADRs so
their current accepted state is unambiguous." "Preserve the history of the decision and the reason
the architecture changed."). **Drift recorded:** the brief says "from Proposed", and this record was
never `Proposed` — it was filed `Accepted — 2026-09-28, on the owner's direction`, ⊕ flagged for the
owner's review of the pull request that carried it. Under the repository's convention for a gated
record (`e4f9205`, where ADR-0041 and ADR-0042 were accepted at the gate), acceptance here is
ratification: the ⊕ is cleared and the five gate items are answered; the status word was already
`Accepted` and does not change. The approval is of the design recorded below — decisions 1–6 and the
implementation note's code, including decision 5's rate contract (the path engages from 12 kHz; the
best-effort alternative stays on record, not taken) and decision 6's prescribed policy text as
completed at acceptance — and explicitly *not* of
[ADR-0045](ADR-0045-true-peak-mode-answers-to-the-ceiling-in-force-at-emission.md), which this record
amends and which stays ⊕ pending the owner's review, that review being of ADR-0045 as amended here;
nor of ADR-0041's voicing constants (attack 0.25 ms, release 10 ms), which stay ⊕ listening material
— the 16-sample floor, the ease span and the rise cap are this record's guarantee, not voicing; nor
of any editor copy beyond the unit (OQ-020, open). *(Pointer 2026-09-29, after the ratification:
OQ-020 is resolved — option 2, below 12 kHz the TP and Ceiling tooltips name the boundary instead of
claiming dBTP; its words stay ⊕ for the owner's fine review and are not part of this approval.)*

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
   *(Settled at acceptance, 2026-09-29: the approval covers the 12 kHz rail; the best-effort
   alternative stays on record, not taken — taking it later is a record of its own.)*
   *(Pointer 2026-09-29, not an amendment: the editor's TP and Ceiling tooltips follow the rate
   half of this predicate, `CeilingUnitSource::rateEngagesTruePeak`, which `truePeakEngaged` now
   calls — so the unit and the tips share one decider — and below 12 kHz name the boundary instead
   of claiming dBTP; `OPEN_QUESTIONS.md` OQ-020.)*
6. **Policy amendment (prescribed text, `ADR_POLICY.md` rule 5).** `DSP_POLICY.md` invariant 4:
   "any sample rate" becomes *any host sample rate — true-peak mode's inter-sample promise holds
   wherever the path engages, 12 kHz and up (`truePeakPathEngages`, ADR-0046); below 12 kHz the
   ceiling holds on sample peaks and the Ceiling reads dB*, and the guard list gains
   `testTruePeakModeHoldsTheCeilingBelow44k`. Invariant 8's second yielding place (ADR-0045 decision
   5) keeps its words with the figure *(Z + 1 + E) ≈ 4.9 glide steps — 0.0040 on 0.9 DC for a −1 →
   −20 dB cut at 48 kHz* in place of *(16 + A/2 + 1) glide steps — 0.0185*.
   *(Completed at acceptance, 2026-09-29, so the approval covers the policy text as it stands: the
   ratification note below, item (d) — invariant 3's prescribed text, which this decision missed,
   and invariants 4 and 8 as they now read.)*

> **Implementation note 2026-09-28 (decisions 1, 2 and 4; not an amendment).** The round's
> independent review of the integrated tree (`0f162c8`) found one defect in decision 1's code and two
> mechanisms no test pinned; `f03d673` closes all three. Recorded rather than folded in:
>
> - **Decision 1's "a window of ones answers exactly 0 whatever the weights round to" was true, and
>   its mirror was not.** The weights are normalised in double and stored as floats, and at some
>   attack lengths — A = 22, 96, 192, i.e. 88.2 / 384 / 768 kHz — their float sum is 1 + 1–3 ulp.
>   A window of ZEROS (a forward minimum at 0, an astronomical input: a required gain below about
>   −138 dB) then answered a reduction of 1 + 1.19e-7, a gain of −1.19e-7, and the sample backstop
>   clipped the sign-inverted product to the ceiling: at 1e30 input, 255 samples at the ceiling and
>   the product meter +1.85 dB over at 88.2 / 384 / 768 kHz, where 0.2.15's boxcar (1 − 0/A) was
>   exactly silent. The weighted sum is now divided by `easeTotal` — the float weights summed in the
>   frame loop's own order — and capped at 1, so a window of zeros answers exactly 1 at every rate
>   (`testTheClampSilencesAnAstronomicalInput`: 44.1–768 kHz, ±1e9 and 1e30 bursts, the gain in
>   [0, 1], 1e30 exactly silent). A plain cap alone was measured insufficient (a sum just under 1
>   leaves a gain of ~6e-8). The same review class at 1e9 (+180 dBFS) with the output over the
>   ceiling is older than this record: `KNOWN_ISSUES.md` KI-027.
> - **Decision 1's "first step of ~0.25 %"** is 0.29 % at A = 16 (0.18 % at 24, 0.09 % at 48;
>   derived); **decision 2's "−80 dB floor"** is an additive term in the rise, μ·(1 − r + φ), not a
>   floor on the gain — a gain silenced to exactly 0 restarts from 0 at μ·φ per sample (measured at
>   48 kHz: ~1000 samples to half gain, ~7300 to exactly 1). The code comments say so; the decision
>   texts above keep their words.
> - **Unpinned until `f03d673`:** reverting decision 4's min stamp in the engine and removing
>   decision 2's rise cap each passed the whole suite (the tests' clamp driver re-implemented the
>   stamp). Now `testTruePeakModeLagsAnAscentByTheEntryCeiling` (44.1 / 48 kHz: the engine's TP-on
>   output lags TP-off by more than half the D-sample glide through > 90 % of an ascent, and is never
>   above it) and `testTheClampReleaseRiseIsCapped` (48 kHz: the rise after a deep reduction never
>   exceeds μ·(1 − r + φ) per sample) each fail on its mutant. Mutation on the fixed tree: the
>   emission-only stamp, the uncapped rise, the unnormalised ease, the uncleared dry ring and the
>   50 ms loudness guard are all killed.
>
> Output: the engine matrix re-run on `f03d673` (14 200 renders, 1775 at each of 12 / 16 / 22.05 /
> 32 / 44.1 / 48 / 96 / 192 kHz) is 93.0–96.8 % bit-identical per rate to the same jobs on the tree
> the review read; every reading is within 0.0017 dB of it, the worst at every rate unchanged
> (+0.0426 / +0.0380 dB), and the only renders over 0.1 dB are the same 21 Force Max splice renders
> (KI-024 route B).

> **Ratification note 2026-09-29 (not an amendment).** What acceptance re-verified against the code
> at `6ee9f29`, three code facts the record above did not state, and the completion of decision 6.
> Nothing in the code moved for it except comments; the decision texts keep their words.
>
> - **(a) The re-verification.** Every constant and mechanism decisions 1–5 name matches the code:
>   `kAttackMs` 0.25 and `kMinAttackSamples` 16 (`attackFor`), `kEaseSpan` 4.8, `kReleaseRise`
>   0.01, `kRiseFloor` 1e-4, `kRevisionLead` 1 and `kMinTruePeakRate` 12000 (`src/dsp/CeilingClamp.h`);
>   the `easeTotal` normalisation; the min(entry, emission) stamp of stage A
>   (`AnabasisEngine::processChunk`, `src/dsp/AnabasisEngine.cpp:1014-1015`), which feeds both the
>   limiter's TP-mode threshold (`src/dsp/AnabasisEngine.cpp:1109`) and the clamp
>   (`src/dsp/AnabasisEngine.cpp:1377`); and ONE predicate, `truePeakPathEngages`
>   (`src/dsp/Latency.h`), behind the engine's rail (`AnabasisEngine::prepare`,
>   `src/dsp/AnabasisEngine.cpp:147`; `AnabasisEngine::process`, `src/dsp/AnabasisEngine.cpp:392`)
>   and the Ceiling's unit (`CeilingUnitSource::truePeakEngaged`, `src/PluginParameters.h`).
>   `truePeakPathEngages`, `CeilingClamp::truePeakDelayFor` and `predictLatencySamples`, compiled
>   from the repository's JUCE-free headers over 26 rates from 3.9 to 768 kHz, with the boundaries
>   scanned over every integer rate to 800 kHz: the path engages at every integer rate from 12000 Hz
>   and at none below it (it would fit the allowance from 4801 Hz; the old law fit from 3901 Hz);
>   D = 46 at every rate below 66 kHz, 47 at 66 kHz, 52 / 54 / 78 at 88.2 / 96 / 192 kHz.
>
>   **The reported latency is identical with TP on and off** at every rate × Oversampling off–16× ×
>   both phases × Force Max. That is a DERIVED identity, not a measurement of the engine: the latch
>   (`AnabasisEngine::latchOsConfig`, `src/dsp/AnabasisEngine.cpp:212-216`) takes `clampDelay` out
>   of the region's line and the clamp adds it back, so the composition sums to `delaySamples` plus
>   the oversampler's latency — `predictLatencySamples`, which never reads the mode. The MEASURED
>   evidence is the impulse tests `DSP_POLICY.md` invariant 2 is guarded by,
>   `testReportedLatencyMatchesImpulse` and `testOsLatencyMatrix`, which run in both modes at 48 kHz,
>   and the round's 1140-configuration identity (Evidence).
>
>   **The limiter's longest window with TP on, per rate** (a 10 ms Lookahead; computed from the same
>   headers; 0.2.15's law is A = max(8, round(0.25 ms · sr)), and its rail checked only that the path
>   fit):
>
>   | Host rate | TP engages | A | D | Longest window, 0.2.16 | 0.2.15: A / D | Longest window, 0.2.15 | Change |
>   |---|---|---|---|---|---|---|---|
>   | 8 kHz | no | 16 | 46 | 80 / 10.000 ms (all of it) | 8 / 38, engaged | 42 / 5.250 ms | longer |
>   | 11.025 kHz | no | 16 | 46 | 111 / 10.068 ms (all of it) | 8 / 38, engaged | 73 / 6.621 ms | longer |
>   | 11.999 kHz | no | 16 | 46 | 120 / 10.001 ms (all of it) | 8 / 38, engaged | 82 / 6.834 ms | longer |
>   | 12 kHz | yes | 16 | 46 | 74 / 6.167 ms | 8 / 38 | 82 / 6.833 ms | shorter |
>   | 22.05 kHz | yes | 16 | 46 | 175 / 7.937 ms | 8 / 38 | 183 / 8.299 ms | shorter |
>   | 44.1 kHz | yes | 16 | 46 | 395 / 8.957 ms | 11 / 41 | 400 / 9.070 ms | shorter |
>   | 48 kHz | yes | 16 | 46 | 434 / 9.042 ms | 12 / 42 | 438 / 9.125 ms | shorter |
>   | 61.999 kHz | yes | 16 | 46 | 574 / 9.258 ms | 15 / 45 | 575 / 9.274 ms | shorter |
>   | 62 kHz | yes | 16 | 46 | 574 / 9.258 ms | 16 / 46 | 574 / 9.258 ms | same |
>   | 66 kHz | yes | 17 | 47 | 613 / 9.288 ms | 17 / 47 | 613 / 9.288 ms | same |
>   | 96 kHz | yes | 24 | 54 | 906 / 9.438 ms | 24 / 54 | 906 / 9.438 ms | same |
>   | 192 kHz | yes | 48 | 78 | 1842 / 9.594 ms | 48 / 78 | 1842 / 9.594 ms | same |
>
>   So gate item 2's "where it was 38–42" and the Consequences' "shorter below 66 kHz … unchanged at
>   88.2 kHz and above" read more precisely as: D was 38 at 8–32 kHz, 41 / 42 at 44.1 / 48 kHz and
>   up to 45 below 62 kHz; the TP-mode window is shorter from 12 kHz to below 62 kHz and unchanged
>   from 62 kHz; and at 3901–11999 Hz, where 0.2.15 engaged the path and 0.2.16 does not, the
>   limiter gets the FULL allowance — longer (8 kHz 5.25 → 10 ms, 11.025 kHz 6.62 → 10.07 ms). And
>   decision 5's "one relaxed load of one scalar": the unit's read goes through
>   `GrHistoryBuffer::prepared()`, which makes the pair's two relaxed loads (rate and block) and uses
>   the rate alone; nothing pairs them, so the conclusion — no epoch bracket — stands.
> - **(b) Below 12 kHz the TP switch is not inert.** The rail gates the clamp's true-peak path, not
>   the limiter's detector: `limiter.setTruePeakMode (p.truePeakMode && osN < 4)`
>   (`AnabasisEngine::process`, `src/dsp/AnabasisEngine.cpp:796`) does not read `tpClampFits`, so
>   with TP on at Oversampling Off or 2× the limiter detects on its 4× true-peak estimate at every
>   rate, 8 and 11.025 kHz included. Below 12 kHz that is a best effort with NO dBTP guarantee: the
>   Ceiling itself is held on sample peaks, by the sample clip, and reads ` dB`. The switch causes no
>   latch, duck or engagement decay there (`wantTpClamp`, `src/dsp/AnabasisEngine.cpp:392`).
>   Recorded as the intended behaviour; no DSP change was made. Where this record says true-peak
>   mode "is not available" below 12 kHz (the Consequences), it means the dBTP ceiling, not the
>   switch.
> - **(c) A rate ≤ 0 after an explicit prepare.** The GR history ring stores the host's rate as
>   given, so after a prepare at 0 or a negative rate the unit's 48 kHz fallback
>   (`CeilingUnitSource::truePeakEngaged`) reads ` dBTP` with TP on while the engine, whose predicate
>   refuses zero and negative rates, runs the sample clip. Only a non-conforming host reaches it
>   (VST3's and AU's rates are specified positive; `AnabasisEngine::prepare`'s rail comment), and
>   before any prepare no audio runs. Decision 5's "cannot claim `dBTP` where the path is not
>   running" is exact at every conforming host rate; this is recorded as a limitation, not fixed —
>   telling "never prepared" from "prepared at ≤ 0" needs more than the ring's pair.
> - **(d) Decision 6, completed** (prescribed text, `ADR_POLICY.md` rule 5). `DSP_POLICY.md`
>   invariant 3's "the ceiling is interpreted as dBTP when true-peak mode is on", which this record
>   missed — below 12 kHz the switch can be on while the ceiling is a sample-peak one — becomes:
>   *the ceiling is interpreted as dBTP when true-peak mode is ENGAGED — the switch on and the host
>   sample rate 12 kHz or more (`truePeakPathEngages`, ADR-0046) — and as a sample-peak ceiling
>   otherwise.* Invariant 4 now carries decision 6's text exactly; from 2026-09-28 it had carried it
>   split in two and reworded (" — 12 kHz and up", a bold sentence of its own), and its guard list
>   still labelled `testTruePeakModeHoldsTheCeilingBelow44k` "any sample rate" — both corrected,
>   with a dated note in the invariant. Invariant 8 carries the figure as prescribed and one
>   sentence more, prescribed here so the approval covers the text as it stands: *"The figure is
>   ADR-0046's, whose narrower revision reach replaced ADR-0045's "(16 + A/2 + 1) glide steps —
>   0.0185"."* The invariant → test map's rows 4 and 8 gain the ADR-0045 / ADR-0046 guards
>   (`testTruePeakModeHoldsTheCeilingUnderAutomation`, `testTruePeakModeHoldsTheCeilingBelow44k`;
>   `testTruePeakModeBoundsTheStepAtACeilingCut`).

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
  unit is new copy, `OPEN_QUESTIONS.md` OQ-020. *(Ratification note (b), 2026-09-29: what is not
  available below 12 kHz is the dBTP ceiling; the switch still moves the limiter's detector onto its
  true-peak estimate at Oversampling Off and 2×, a best effort with no dBTP guarantee.)*
  *(Pointer 2026-09-29: OQ-020 is resolved — below 12 kHz the TP switch's and the Ceiling's
  tooltips name the 12 kHz boundary instead of claiming dBTP, the unit unchanged; the words ⊕,
  `testTheTruePeakTipsFollowTheRateTheTruePeakPathEngagesAt`.)*
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
  (pin 7 glide steps), `testCeilingClampTruePeakPath` (the delay pin, 46),
  `testTruePeakModeLagsAnAscentByTheEntryCeiling` (decision 4 in the engine),
  `testTheClampReleaseRiseIsCapped` (decision 2), `testTheClampSilencesAnAstronomicalInput`
  (decision 1's normalisation); `tests/state_tests.cpp` —
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
- **Acceptance re-verification, 2026-09-29** (read-only, against `6ee9f29`): the ratification note,
  item (a) — the constants, the stamp, the predicate, the rail and the unit read from the code; the
  rail's boundaries, D, the TP-mode windows and the reported-latency identity computed from the
  repository's JUCE-free headers (the identity derived; the impulse tests the measurement).
- Depends on: ADR-0041 (the path; decision 4's figures amended), ADR-0043 (the two meters), ADR-0045
  (the emission-time ceiling; decisions 1, 2 and 5 amended), ADR-0015 item 5 (the unit; amended).
