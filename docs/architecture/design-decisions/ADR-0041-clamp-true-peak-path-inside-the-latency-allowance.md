# ADR-0041 — The ceiling clamp's true-peak path, inside the constant latency allowance

**Status:** **Proposed — 2026-09-27, awaiting the owner's decision at the Architecture Review
Gate.** This record implements a decision that was already Accepted — ADR-0006 items 2 and 3, the
clamp's own true-peak estimate driving its gain — and that the code never carried out (audit finding
**DSP-001**, `docs/reports/2026-09-26-anabasis-product-ux-audit/findings-dsp-tech.md`). Carrying it out
is not free of gate items, and they are named here rather than left for a reviewer to find:

1. **A conflict with Accepted ADR-0004.** The true-peak path needs lookahead of its own; to keep the
   reported latency constant it takes that lookahead out of the 10 ms allowance, which amends
   ADR-0004 items 1, 2 and 7 **for true-peak mode only** (item 1: "no other stage contributes";
   item 2: the audio delay through the limiter stage is the full 10 ms; item 7: the lookahead range
   is exactly 0.5–10 ms). The **reported** latency does not move by a sample — only its composition.
2. **A wording departure from Accepted ADR-0006 item 2** ("both instances come from the shared
   BS.1770-4 estimator"): the clamp's detector evaluates the shared estimator's phases *and* two more
   readings (decision 3). ADR-0006's purpose for item 2 — an estimate of the clamp's own input, a
   measurement tap that never resamples the audio — is kept exactly.
3. **A DSP signal-flow change** (`DSP_POLICY.md` Enforcement: "any change to … the ceiling clamp, or
   the latency contract"), and a **policy amendment**: invariant 2's body and invariant 8's
   enumeration, carried in decision 8 as prescribed text.

A green build does not clear any of these. Until the owner accepts, the PR that carries this record
is not to be merged.

## Context

`DSP_POLICY.md` invariant 4 promises that the output never exceeds the ceiling, **tolerance
≤ 0.1 dBTP in true-peak mode**, and ADR-0006 decided how: a structurally separate final clamp whose
gain, with `truePeakMode` on, "acts on its own TP estimate, with a sample-level hard clip as the
backstop" (item 3). `CeilingClamp.h` never grew past its P1 comment — *"sample-level hard clamp …
The true-peak-driven gain half of ADR-0006 item 3 arrives with the TruePeak tap at P2"* — so in
true-peak mode the clamp still clipped samples, and nothing measured what that left between them.

Measured on `main` at ed06ad0 with a probe that drives the real `AnabasisEngine` (worklog
2026-09-27 §Investigation), over 2496 true-peak-mode configurations — six programme shapes × five
Loudness positions × eight of the nine oversampling cells × four limiter voicings × two ceilings (the
ninth, 8× linear phase, ran as a 240-configuration addendum with the same result), plus the Force
Max bounce, a +6/+12 dB Post shelf, 44.1/96/192 kHz, three block sizes, lookahead up to 10 ms and
Character 0.5 — **1674 exceed the ceiling by more than 0.1 dB on the product's own dBTP meter,
worst +4.80 dB**, and 1759 on the BS.1770 Annex 2 example meter (worst +6.12 dB). Only the LF-heavy
programme passes. The mechanisms that put the overs there are all by design upstream of the clamp,
which is why ADR-0006 made the clamp — not the limiter — the guarantee:

- **the decimation filter regrows peaks** after the region limiter at every oversampling factor,
  and the limiter's own true-peak detection is off at ≥ 4× (ADR-0003 item 6 — correct for its own
  detector, blind to what the down-filter adds);
- **the limiter's attack is deliberately slewed** by Transients and the Punchy style, letting
  fronts through to the clamp (`LookaheadLimiter.h`);
- **a Post-position EQ boost** sits between the limiter and the clamp (ADR-0002);
- at base rate, **a sample clip itself manufactures inter-sample overs**.

## Problem

**(a) A true-peak-driven gain needs lookahead, and ADR-0004 has none to give.** An estimate of the
waveform between two samples is available only after the interpolator has seen the samples on
either side, and a gain that must not step (invariant 8) needs a ramp before the peak. Both are
delay. ADR-0004 fixed the reported latency at `maxLookahead(10 ms) + osLatency` and said the whole
10 ms is the limiter's line — so either the reported latency grows, or the limiter's line shrinks.

**(b) Which true peak?** "dBTP" is defined by BS.1770 as the maximum of the continuous waveform,
and every meter is a finite approximation of it. The product's own 4× estimator (`TruePeak.h`,
12 taps, Blackman) was sized for the limiter's detector and the meter; on the engine's TP-mode output
it reads up to ~1.4 dB **below** the Recommendation's own order-48 example filter, and on dense
broadband material up to ~0.24 dB **above** an accurate interpolator. A clamp driven by any one meter
holds the ceiling on that meter only — which is the audit's merged sub-item (c): "the new clamp TP
tap either uses a more accurate estimator or carries a stated margin".

## Options

**Where the lookahead comes from.**

- **A. Report more latency** — `maxLookahead + D + osLatency`, always or only in TP mode.
  **Lost:** a reported-latency change on every session (always), or PDC that moves whenever a
  preset, A/B slot or undo step carries a different `truePeakMode` (TP mode only) — the exact PDC
  spray ADR-0004 exists to prevent.
- **B. Keep a constant clamp delay in both modes** (the region line is 10 ms − D always). **Lost:**
  the true-peak-OFF path stops being bit-identical — the limiter's longest window shrinks for every
  user and the Post EQ's glide timing moves against the audio — to fix a defect that exists only in
  TP mode.
- **C. Carve D out of the allowance in TP mode only, and latch the composition like an oversampling
  change.** The reported figure never moves; TP-off is bit-identical; the longest engaged lookahead
  in TP mode is 10 ms − D. **Chosen.**
- **D. No lookahead — a reactive gain plus the hard clip.** **Lost:** the gain arrives after the
  peak it is reacting to; physically cannot meet the tolerance.
- **E. Oversample the clamp itself.** **Lost:** ADR-0006 option D — the clamp must be downstream of
  the base-rate Post EQ, and a clamp that resamples the audio it passes is what item 2 forbids.

**What the gain acts on.**

- **F. The shared 4× estimator alone** (ADR-0006 item 2 read literally). Built and measured first:
  the product meter reads the output at the ceiling (worst +0.008 dB) while the Annex 2 example
  meter reads it up to +1.44 dB over and a 32×/128-tap reference up to +2.67 dB. **Lost:** it holds
  the ceiling on the one meter that shares its blind spot.
- **G. An accurate interpolator alone** (16 phases × 32 taps). **Lost:** the product meter then
  reads the output up to +0.63 dB over — the 12-tap Blackman estimator over-reads dense broadband
  material — so the product's own display would show overs the clamp let through.
- **H. The largest of three readings on one window — an accurate interpolator, the product meter's
  phases, the Annex 2 example filter.** **Chosen**; each reading closes a failure the other two
  leave, all three measured.
- **I. The shared estimator plus a fixed margin.** **Lost:** the under-read it would have to cover
  is programme-dependent (0 to ~1.4 dB measured), so a margin is either a loudness tax on every
  master or not a bound.

## Decision

1. **With `truePeakMode` applied, the clamp's gain acts on its own true-peak reading of its own
   input** (ADR-0006 items 2–3, now implemented) with the sample-level hard clip kept as the
   backstop; with it off the clamp is the P1 sample compare, unchanged and bit-identical.

2. **The reading (`ClampTruePeakDetector`, `TruePeak.h`)** describes, per step, x[j] and the
   continuous waveform between x[j] and x[j+1], j = n − 16, from one 32-sample window, as the
   largest of: (i) a 32-tap Kaiser (β = 8) windowed-sinc interpolator designed at `prepare()`,
   evaluated at the quarter points and refined by a parabola through the largest point and its
   neighbours; (ii) `TruePeakEstimator`'s own 4× phases, so the dBTP display cannot show an over the
   clamp let through; (iii) the order-48, 4-phase example filter of **ITU-R BS.1770 Annex 2**
   (BS.1770-5, identical in -4), transcribed and pinned by test. Reporting lag 16.

3. **The gain** (`CeilingClamp::processFrameTruePeak`): one LINKED gain for all channels; a
   requirement per segment against the lower of its two samples' ceilings (each frame carries the
   ceiling the limiter used for it); the minimum over every segment whose interpolation reads a
   sample; a forward minimum over an **attack of 0.25 ms** (at least 8 samples) and its moving mean
   — a linear ramp that is never above the requirement; a one-pole **release of 10 ms** on the
   reduction, snapping to exactly unity at −120 dB. Below the ceiling the path is an exact delay.
   The two voicing constants are ⊕ listening material, like the limiter's; neither affects the
   ceiling.

4. **The latency composition in true-peak mode (amends ADR-0004 items 1, 2 and 7 for TP mode):**

   ```
   reportedLatency = maxLookaheadSamples(10 ms, sr) + osLatency(factor, phase)   ← unchanged, both modes

   true-peak mode:  region lookahead line = maxLookaheadSamples − D   (base samples; × osN in the region)
                    clamp true-peak path  = D
                    D = attack + 30   (41 samples at 44.1 kHz, 42 at 48 kHz, 54 at 96 kHz, 78 at 192 kHz)
   ```

   The clamp's delay D comes out of the allowance; the region's lookahead line shrinks by D; the
   limiter's engaged window is capped at `maxLookahead − D`. Every lookahead setting up to
   10 ms − D engages exactly as before; a setting above it engages 10 ms − D (9.125 ms at 48 kHz).
   The dry/bypass alignment, `groupDelaySamples()` and `predictLatencySamples()` do not see the mode
   at all. If a host-supplied rate leaves no room for D above the 0.5 ms minimum window, the path is
   not used (a rail; no conforming rate reaches it).

5. **A true-peak toggle is a latched rewire.** It moves the region line's length, so it rides the
   §2.8 duck exactly like an oversampling-factor change: out, latch at the silent bottom (the
   lookahead ring, the oversampler and the clamp's own ring restart), hold for the refill, in.
   Adopted directly on the first block after prepare/reset and on entering offline, as the factor
   is. The limiter's own detector mode still follows the snapshot per block, as before.

6. **Realtime.** Everything is allocated at `prepare()`; the per-frame path is allocation-free,
   lock-free and bounded (fixed windows; the requirement windows are scanned only while they hold a
   value below unity). The only recursive value (the reduction) is a max of values in [0, 1] ordered
   so a NaN operand loses; no finite input produces a non-finite output.

7. **Verification is on independent meters, at the policy's own 0.1 dB.** The durable guard
   (`testTruePeakModeHoldsTheCeiling`) reads every run with the product meter AND an independently
   implemented Annex 2 meter; the matrix in the worklog adds libebur128 and a 32×/128-tap reference.

8. **Policy amendment (prescribed text, `ADR_POLICY.md` rule 5).**
   - `DSP_POLICY.md` invariant 2 gains, after "the engine pads the difference": *"In true-peak mode
     the ceiling clamp's true-peak path takes a short delay of its own (ADR-0041) out of that same
     allowance — the lookahead line shrinks by it and the longest engaged window becomes 10 ms minus
     it — so the reported figure is unchanged in both modes."*
   - `DSP_POLICY.md` invariant 8's enumeration gains **"the true-peak mode"** beside the
     oversampling phase mode, with its reason: it moves the clamp's share of the allowance, so it is
     latched at the §2.8 duck like an oversampling change.

## Consequences

- **The promise holds at every oversampling cell and in the Force Max bounce**, on the product meter
  and on the Annex 2 example meter — measured, not argued (worklog §Verification; the numbers are
  quoted in `TEST_REPORT.md`).
- **What is not claimed, stated:** a 32×/128-tap reference still reads residual overs on synthetic
  programme with strong content in the last few percent below Nyquist (clicks, near-Nyquist tones,
  a +12 dB Post shelf), and libebur128 reads a smaller residual. Filtering that content to 20 kHz
  before measuring makes the reference read HIGHER, not lower — the "true" peak of near-Nyquist
  content depends on the reconstruction filter. Which yardstick defines the promise is the owner's
  call (audit DSP-001 sub-item (a)); `KNOWN_ISSUES.md` KI-020 carries the figures and the options (a
  longer accurate kernel measured to halve the residual, at twice the lookahead and CPU).
- **The limiter's longest window in true-peak mode is 10 ms − D.** Invisible below that setting.
- **A true-peak toggle dips the output** for the §2.8 duck (~6 ms out, a refill hold, ~28 ms in),
  like any latched rewire; a preset, A/B or undo step that changes the mode already sits inside one.
- **Level:** true-peak mode now removes the overs it used to emit. Median RMS change over the main
  matrix −0.06 dB; the largest drops (−5 to −6.6 dB) are the +12 dB Post-shelf stress cases that
  rendered ~+4.7 dB true-peak overs before.
- **Cost:** the clamp's true-peak path measures 0.51 % of one core at 48 kHz standalone
  (`AnabasisBench`, 107 ns/sample; the detector's arithmetic is folded over its filters' mirror
  symmetry to get there). The budget case (48 kHz · 512 · 4× · working) goes from 3.00 % to 3.60 %
  with TP on — inside the ≈5 % target, but over DESIGN §9's limiter + TP-detection row at ≥ 4×,
  which this path is charged against (`PERFORMANCE_BUDGET.md`). The cost is per base sample, so it
  doubles at 96 kHz. TP mode is off by default (ADR-0015).
- **The GR meter still shows the limiter's reduction only** (unchanged semantics); the clamp's own
  reduction is not displayed.
- **TP-off is bit-identical to `main`** — 222 of 222 true-peak-off configurations hash-identical.
- **Forecloses:** reporting the clamp's delay as extra latency; a clamp detector that reads fewer
  than the three readings without a new record.

## Related code

`src/dsp/CeilingClamp.h` (the true-peak path), `src/dsp/TruePeak.h` (`ClampTruePeakDetector`,
`TruePeakEstimator::designPhases`), `src/dsp/AnabasisEngine.{h,cpp}` (`latchOsConfig`'s TP
composition, `latchWanted`, the window cap, stage E's frame-wise clamp), `tests/dsp_tests.cpp`
(`testClampTruePeakDetector`, `testCeilingClampTruePeakPath`, `testTruePeakModeHoldsTheCeiling`,
`testTruePeakModeCapsTheWindowNotTheLatency`, `testDuckWrapsTruePeakLatch`,
`testTruePeakModeIsExactBelowTheCeiling`, and the TP-mode loops added to
`testReportedLatencyMatchesImpulse`, `testOsLatencyMatrix`, `testBypassNullUnderOs` and
`testTheAudioPathAllocatesNothing`), `tests/bench.cpp` (the clamp row and the `working+TP` mode).

## Evidence

Confidence: **Verified** for the mechanism and the measured figures (tests mutation-checked: the
latency composition, the TP path, the window cap and the toggle latch each fail their own guard when
reverted); **Unverified** for the voicing constants (not listened to).

- Before/after engine matrix, 2718 configurations, four meters, and the negative control (the
  regression guard fails on `main`: 102 of 123 runs over on either meter, worst +6.04 dB) —
  `worklogs/2026-09-27-phase0-product-correctness.md`.
- Estimator comparison (why three readings): same worklog, §Investigation.
- ITU-R BS.1770-5 (11/2023), Annex 2 — the example filter table; cross-checked value by value
  against the Recommendation's text.
- Depends on: ADR-0002 (clamp placement), ADR-0003 (TP as a measurement tap), ADR-0004 (amended
  here for TP mode), ADR-0006 (implemented here), ADR-0015 (TP off by default).
