# ADR-0043 — "dBTP" in the ceiling promise is defined on the product meter and the BS.1770 Annex 2 filter

**Status:** **Accepted — 2026-09-27**, on the owner's instruction of record for the Phase 0 closure:
make the delivery-meter decision from the evidence already gathered, and keep the current
implementation if the product meter + Annex 2 definition is the option that best matches the
accepted product contract and architecture — "do not change the detector merely because a different
reference can produce a different number." It settles audit finding **DSP-001 sub-item (a)**, the
question [ADR-0041](ADR-0041-clamp-true-peak-path-inside-the-latency-allowance.md) decision 7 left
open, and enacts the `DSP_POLICY.md` invariant 4 text below (`ADR_POLICY.md` rule 5). It changes no
code, no sample and no test threshold.

## Context

`DSP_POLICY.md` invariant 4 promises that in true-peak mode the output does not exceed the ceiling by
more than 0.1 dBTP. A true peak is a property of the continuous waveform between samples; every
meter estimates it with a finite interpolator, and on programme with energy in the last few percent
below Nyquist the estimates disagree by up to about a decibel. A clamp holds the ceiling on the
meters whose readings it limits on, and not necessarily on any other — so the promise is complete
only once it names its meter. Until this record the policy said so in as many words: "Which
yardstick 'dBTP' is measured on is recorded, not assumed … the choice between them is the owner's."

ADR-0041's clamp limits on the largest of three readings of one window — an accurate 32-tap
interpolator, the product meter's own 4× phases and the BS.1770-5 Annex 2 example filter — and its
durable guard reads the product meter and an independent Annex 2 implementation. Measured on the
accepted engine over 2736 true-peak-mode configurations (every oversampling cell, Force Max,
44.1–192 kHz, Post shelf 0/+6/+12 dB, block sizes, lookahead, Loudness 0–100 %, three styles):

| Meter | Over the ceiling by > 0.1 dB | Worst |
|---|---|---|
| Product meter (`TruePeakEstimator`, 4×, 12 taps per phase, Blackman) | **0 / 2736** | +0.005 dB |
| BS.1770-5 Annex 2 example filter (order 48, four phases) | **0 / 2736** | +0.004 dB |
| libebur128 1.2.6 (49-tap Hann interpolator) | 130 / 2736 | +0.18 dB |
| 32×/128-tap Kaiser reference (56-configuration hot subset) | — | +0.98 dB |

`main` over the same matrix: 1674–1759 of 2496 over on every meter, worst +4.80 to +7.80 dB.

## Problem

Which meter, or meters, the ≤ 0.1 dBTP promise is measured on — and what the product says about the
meters it is not measured on.

## Options

- **1. The product meter and the Annex 2 example filter** (the implemented behaviour). The promise is
  stated on the meter the plug-in itself displays and on the Recommendation's own printed example
  filter; both are in the suite. Behaviour change: none. CPU: unchanged (the clamp's true-peak path
  0.51 % of one core at 48 kHz). What remains: libebur128 up to +0.18 dB (130 of 2736, synthetic
  HF-heavy or transient programme, the +6/+12 dB Post shelf, mostly linear-phase cells) and a
  long-kernel reference up to +0.98 dB on the hot subset. **Chosen.**
- **2a. Also hold libebur128's reading.** Prototyped: 0 of 2736 over on libebur128, product meter and
  Annex 2 unchanged, no latency change, TP-off bit-identical — but it ties the product's contract to
  one library's filter design (vendored under `DEPENDENCY_POLICY.md`, or its interpolator transcribed
  and pinned), adds ~30 % detector CPU, changes 1858 of the 2736 TP-mode renders (by at most
  0.017 dB RMS), and leaves the long-kernel residual exactly where it was. It moves the promise from
  one finite interpolator to another rather than closing the question. Rejected.
- **2b. A longer accurate kernel.** Measured: 64 taps take the reference residual from +0.93 to
  +0.36 dB and libebur128 from +0.18 to +0.16 dB, at twice the clamp's share of the 10 ms allowance
  (the longest limiter window in TP mode shrinks again) and twice the kernel's CPU; a reference-grade
  kernel is not a practical clamp detector (a 64-sample lag, ~4000 multiply-adds per sample per
  channel). A limiter-window change for a residual that still does not reach zero. Rejected.
- **2c. A margin.** Hold the Option 1 readings at the ceiling minus m (≈ 0.2 dB for libebur128, ≈ 1 dB
  for the reference): costs up to m dB of level on every TP-mode render that reaches the ceiling,
  including the ones no meter would flag, and makes the number the user sets differ from the level
  the output holds — a change to what the Ceiling control means. Rejected.

Why Option 1 is the one that matches the accepted contract: ADR-0006 item 2 and ADR-0003 make the
clamp's estimate a measurement tap on the **shared** BS.1770 estimator — the product meter — so a
definition on that meter is the one the architecture already promised; the Annex 2 filter is the
Recommendation's own example of a conforming measurement; both are implemented, pinned and testable
inside the build; and neither of the alternatives removes the underlying fact (finite interpolators
disagree near Nyquist) — each only moves which reading carries the residual, at a cost in CPU,
level, lookahead or control meaning.

## Decision

1. **"dBTP" in invariant 4's promise is defined on two meters, each of which must hold it:** the
   product's own dBTP estimator (`TruePeakEstimator`, `src/dsp/TruePeak.h`: 4× polyphase, 12 taps per
   phase, Blackman-windowed sinc) — the meter the plug-in displays — and the example FIR of
   ITU-R BS.1770-5 Annex 2 (order 48, four phases; identical in BS.1770-4), its four phases applied at
   every sample rate. Neither may read the true-peak-mode output more than 0.1 dB above the ceiling.
2. **Other true-peak meters are reference and compatibility measurements, not the definition** —
   libebur128's interpolator and a long-kernel (32×/128-tap) reference among them. They are measured
   and recorded (`TEST_REPORT.md`, `KNOWN_ISSUES.md` KI-020), not asserted, and the product does not
   claim them: on programme with strong content in the last few percent below Nyquist a delivery
   checked on another meter can read a residual (measured 2026-09-27: libebur128 up to +0.18 dB, the
   reference up to +0.98 dB on a hot synthetic subset). KI-020's workaround for such programme — a
   ceiling about 1 dB below a delivery spec checked on a long-kernel meter, and oversampling to
   reduce the near-Nyquist content the clamp has to catch — stands, and the user manual points at
   it.
3. **The durable guards name the definition.** `testTruePeakModeHoldsTheCeiling` and
   `testTruePeakEngagementHoldsTheCeiling` check each defining meter separately, so a failure names
   the meter; `testClampTruePeakDetector` pins the clamp's reading as never below either defining
   meter's reading of the same segment.
4. **Removing a meter from this definition weakens the promise** and is an Architecture Review Gate
   item in its own right; adding one (libebur128's interpolator, say) is an amendment of this record.
5. **Policy amendment (prescribed text, `ADR_POLICY.md` rule 5).** `DSP_POLICY.md` invariant 4's
   yardstick sentence ("Which yardstick 'dBTP' is measured on is recorded, not assumed … the choice
   between them is the owner's") is replaced by: *"**What 'dBTP' means in this promise is defined,
   not assumed** (ADR-0043, 2026-09-27; audit finding DSP-001 sub-item (a)). In true-peak mode the
   tolerance applies to each of two meters reading the output: (i) the product's own dBTP estimator,
   `TruePeakEstimator` (4× polyphase, 12 taps per phase, Blackman-windowed sinc), the meter the
   plug-in displays; and (ii) the example FIR of ITU-R BS.1770-5 Annex 2 (order 48, four phases;
   identical in BS.1770-4), its four phases applied at every sample rate. Neither may read the output
   more than 0.1 dB above the ceiling. Other true-peak meters — libebur128's interpolator, a
   long-kernel reference — are reference and compatibility measurements, recorded
   (`TEST_REPORT.md`, `KNOWN_ISSUES.md` KI-020) and not asserted; near Nyquist every finite
   interpolator reads a different peak, so a delivery checked on another meter can read a residual.
   Removing a meter from this definition weakens the promise and is an Architecture Review Gate
   item; adding one is an ADR amendment."* Invariant 4's tolerance sentence gains ", dBTP as defined
   below".

## Consequences

- **Nothing audible or measurable changes**: the clamp, its three readings, the latency composition
  and every rendered sample are ADR-0041's, untouched.
- **What is not claimed is stated**: the libebur128 and long-kernel residuals are documented as
  reference/compatibility behaviour (KI-020 stays open as a documented limitation, not as a pending
  decision). Real-programme exposure is smaller than the synthetic worst case — an Ardour render of a
  hot programme read libebur128 −0.01 dBTP against a −0.10 ceiling — but is not measured on a music
  corpus.
- **VIS-002 is unaffected and stays open.** The STATISTICS TP row compares its held reading with the
  ceiling exactly (ADR-0020 amendment 2), so in 74 of 2736 TP-mode configurations it turns warn-red
  while printing the ceiling (0.001–0.005 dB over, inside the tolerance this record defines). The
  audit's remedy — the SP row's half-print slack — is an ADR-0020 amendment, orthogonal to which
  meter defines dBTP.
- **Forecloses** a clamp detector that stops reading either defining meter's phases without an
  amendment here; quoting "≤ 0.1 dBTP" anywhere in product copy without the meters (the manual's
  wording already names both).
- **Audit sub-item (b)** ("the hostile-input test also reports a high-accuracy reference") is closed
  by this decision rather than by code: the reference is a documented measurement in the worklog
  matrix and `TEST_REPORT.md`, deliberately not part of the suite (it is external to the build).

## Related code

- `src/dsp/TruePeak.h` (`TruePeakEstimator` — defining meter (i)), `src/dsp/ClampTruePeakDetector.h`
  (the clamp's three readings; `truepeak::designMeterPhases`)
- `tests/dsp_tests.cpp`: `testTruePeakModeHoldsTheCeiling`, `testTruePeakEngagementHoldsTheCeiling`,
  `testClampTruePeakDetector`, and the test-local `Annex2Meter` (defining meter (ii), an independent
  implementation of the Recommendation's table)

## Evidence

Confidence: **Verified** for the measured figures and for the two defining meters' guards; the
reference-meter residuals are **measured and recorded, not claimed**.

- Decision material with every option measured, including the 2a prototype checked against
  libebur128 itself to within 8·10⁻⁷ dB — `docs/reports/2026-09-27-phase0-owner-decisions.md` §1.
- The 2736-configuration four-meter matrix and the hot-subset reference —
  `worklogs/2026-09-27-phase0-product-correctness.md`; re-rendered hash-identical in the PR #42 review
  (`worklogs/2026-09-27-pr42-review-closure.md`).
- ITU-R BS.1770-5 (11/2023), Annex 2 — the example filter table, cross-checked value by value.
- Depends on: ADR-0003 (TP as a measurement tap), ADR-0006 (the clamp's own estimate), ADR-0041 (the
  clamp that holds both meters).
