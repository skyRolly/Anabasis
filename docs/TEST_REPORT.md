# TEST_REPORT.md

Measured data with methodology (constraint C2: no performance or accuracy number may be written
anywhere without the machine and method that produced it). This file accumulates as phases land;
the P6 release report grows out of it. Every number below is also asserted (with margin) by a
named test in `tests/dsp_tests.cpp`, so it cannot silently rot: a regression fails CI, and a
re-measurement updates both the test comment and this table.

**Environment for all measurements:** Linux x86-64 container, GCC, Release build (`-O3` via
`juce_recommended_config_flags`, no `-ffast-math`), JUCE 9.0.0 @ `f8f8864…`, fs = 48 kHz.
The pin moved to 9.0.1 @ `e18f7f5…` on 2026-08-16 (ADR-0028) and this line is **not** rewritten:
these are dated measurements, and re-stamping an environment nobody re-measured in turns a record
of what was run into a claim about today. What carries them forward is the named test beside each
number — every one of them re-asserted green at the new pin — plus the file-level fact that
`juce_dsp` and `juce_audio_basics` differ between the two tags only in their module-declaration
version string, so no filter, oversampler or block primitive moved underneath them.
Deterministic stimuli (exact-bin sines, fixed seeds); FFT = `juce::dsp::FFT`, 8192-point,
rectangular window on exact-bin content.

## Aliasing (DSP_POLICY invariants 5/6) — 2026-08-01, P2

Stimulus: exact-bin sine, hard-clip shape (w = 0), drive +12 dB — the worst-case memoryless
nonlinearity. "Naive" = the same transfer curve applied memorylessly (computed from the DSP's own
public `ClipSat::transfer`).

| Configuration | Component | Level vs naive | Test |
|---|---|---|---|
| OS Off, ADAA-1 only, f₀ 11.72 kHz | folded 3rd (source 35.2 kHz) | **−14.8 dB** | `testClipAdaaReducesAliasing` (asserted ≥ 6) |
| OS Off, ADAA-1 only, f₀ 11.72 kHz | folded 5th (source 58.6 kHz) | **−10.4 dB** | same (asserted ≥ 8) |
| OS Off, ADAA-1 only, f₀ 5 kHz | folded 5th (source 25 kHz) | −4.8 dB | recorded in the test comment — matches ADAA-1 theory (≈ sinc(π·f/fs)); the reason the assertion stimulus is the bright tone |
| 4× linear vs OS Off, f₀ 11.72 kHz | folded 3rd | **≈ −74 dB** (23.0 → −51.0 dBr) | `testOsReducesAliasing` (asserted ≥ 20) |
| 4× vs Off, same stimulus | fundamental | **+1.3 dB** (61.4 vs 59.8) — oversampling removes ADAA-1's sinc droop at 11.72 kHz; a real effect, not an error | same (asserted < 2.5 dB delta) |

The droop that "recovery" refers to is ADAA-1's `(1 + z⁻¹)/2` divided difference in the curve's
linear region: **any** non-zero drive low-passes the whole programme (cos(πf/fs), ≈2 dB at 10 kHz
at a 48 kHz base rate) and adds a half-sample group delay `Latency.h` does not model. Both are
inherent to first-order ADAA and are stated in the `ClipSat.h` header; the half-sample delay is
also why the impulse-position latency test stays sample-exact only with `clipDriveDb == 0`.

**Measured again on the whole engine, 2026-09-27 (audit finding DSP-004).** A −30 dBFS sine (the
clipper linear) through the real `AnabasisEngine`, response with Clip Drive 0.07 dB — what the
Loudness macro sets at 30.5 % — relative to drive exactly 0 at the same oversampling:

| Rate | OS | 5 kHz | 10 kHz | 15 kHz | 20 kHz |
|---|---|---|---|---|---|
| 48 kHz | Off | −0.47 dB | **−2.01 dB** | −5.11 dB | **−11.74 dB** |
| 48 kHz | 2× | −0.12 dB | −0.47 dB | −1.09 dB | −2.01 dB |
| 48 kHz | 4× | −0.03 dB | −0.12 dB | −0.26 dB | **−0.47 dB** |
| 44.1 kHz | Off | −0.56 dB | −2.42 dB | −6.35 dB | −16.74 dB |
| 44.1 kHz | 4× | −0.03 dB | −0.14 dB | −0.31 dB | −0.56 dB |

Identical at a 3 dB drive: the loss is the kernel's, not the curve's. The bold figures are
asserted by `testClipDriveDroopIsTheDisclosedOne` (±0.05/±0.1 dB, and "< 0.6 dB" at 4×); the rest
are the same probe's readings, method in `worklogs/2026-09-27-phase0-product-correctness.md`.
Disclosed in `KNOWN_ISSUES.md` KI-005 and USER_MANUAL §3.5; not changed.

## Output true peak in true-peak mode (invariant 4, ADR-0041) — 2026-09-27

**Method.** A scratchpad probe drives the REAL `AnabasisEngine` (compiled from `main` @ ed06ad0 for
"before" and from this round's tree for "after") with six deterministic stereo programmes —
music-like (kick, noise snap, pink bed), transient-heavy (one-sample clicks, 0.1 ms noise bursts,
3 kHz rim shots), LF-heavy (kick + 45/90 Hz sub), HF-heavy (high-passed hats, tones to 0.8·Nyquist,
a 2997 Hz hard square), sustained (six-note chord + pink noise) and the fs/4 45° inter-sample vector —
at the macro's Loudness 0/25/50/75/100 % curves, eight of the nine oversampling cells (Off, 2×/4×
min + linear, 8× min, 16× min + linear — 8× linear ran as an addendum, below), four limiter voicings (Transparent / Punchy / Loud, Transients 50 or
100 %) and ceilings −0.1 / −1.0 dBTP: 1920 configurations, plus 240 Force Max bounces, 72 with a
+6/+12 dB Post shelf, 108 at 44.1/96/192 kHz, 24 block sizes, 96 lookahead settings (to 10 ms) and 36
at Character 0.5 — **2496 in true-peak mode**, 8 s each. The output is read by four meters: the
product's own dBTP estimator (`TruePeakEstimator`), the BS.1770 Annex 2 example filter
(ITU-R BS.1770-5, order 48, 4 phases), libebur128 1.2.6 and a 32×/128-tap Kaiser reference (the last
on a 56-configuration hot subset, 4 s each). "Over" = more than 0.1 dB above the ceiling.

| Meter | before: over / 2496 | before: worst | after: over / 2496 | after: worst |
|---|---|---|---|---|
| product dBTP meter | 1674 | +4.80 dB | **0** | **+0.005 dB** |
| BS.1770 Annex 2 example | 1759 | +6.12 dB | **0** | **+0.004 dB** |
| libebur128 | 1710 | +5.41 dB | 98 | +0.18 dB |
| 32×/128-tap reference (56 hot configs) | 53 / 56 | +7.80 dB | 39 / 56 | +0.98 dB |

**8× linear addendum** (the ninth cell, 240 configurations on the same axes): before 161 / 173 over
(product / Annex 2), worst +3.67 / +4.58 dB; after **0 / 0**, worst **+0.001 / +0.001 dB**;
libebur128 32 over, worst +0.18 dB. Every cell measured: 0 of 2736 over on either held meter.

True-peak mode OFF: **222 of 222** configurations are bit-identical to `main` (output hash). Level:
median RMS change −0.06 dB over the main 1920; the large drops (to −6.6 dB) are the +12 dB
Post-shelf cases that rendered ~+4.7 dB true-peak overs before. The residual on the last two meters
is content in the top few percent below Nyquist (`KNOWN_ISSUES.md` KI-020) — which of these meters
defines the promise is the owner's decision.

**Asserted by** `testTruePeakModeHoldsTheCeiling` (123 runs on the product meter and an independent
Annex 2 meter, ≤ 0.1 dB; measured worst +0.001 dB — and on `main` 102 of 123 runs over, worst
+6.04 dB), `testCeilingClampTruePeakPath` (the canonical +3 dB vector held within 0.1 dB at the stage).
Environment: the machine and compiler of the performance section below.

## True-peak estimator accuracy (invariant 3, ADR-0003) — 2026-08-01, P2

Estimator: 4-phase × 12-tap windowed-sinc, integer-normalised DC, designed at `prepare()`.
Stimulus: fs/4 sine, unit true peak, phase chosen to place the continuous peak on / between the
4× interpolation points.

| Vector | Reads | Test bound |
|---|---|---|
| Grid-aligned +3.01 dB ISP (peak on a 4× point) | **−0.004 dB** | ≤ 0.1 dB |
| On-sample peak | **+0.000 dB** | ≤ 0.1 dB |
| Off-grid worst case (peak midway between 4× points) | **−0.171 dB** | bounded (−0.6, 0.1] — the inherent max-reading 4× property; BS.1770's own tolerance envelope admits it |

Estimator reporting lag: **6 input samples** ≈ 0.125 ms — inside the 0.5 ms minimum engaged
lookahead with 4× margin (RISK-008). The FIR's nominal group delay is (12−1)/2 = 5.5, but each
call returns the MAXIMUM over `x[n−6]` and the interpolated points at n−5.75/−5.5/−5.25, so the
oldest sample an estimate can describe is n−6 — that is the figure the margin argument must use,
and the one this file, `TruePeak.h` and RISK-008 now all quote.

**What that delay costs the limiter, since the number is easy to read as free.** In true-peak
mode the wedge is fed the estimate, whose oldest covered sample is n−6, while the window
`wOs` is still derived from the engaged lookahead as though the fed value were the sample playing
`wOs` steps ahead. The effective pre-emption is therefore up to 6 base samples SHORTER than the
lookahead setting: negligible at the 10 ms maximum (0.06 %), but **~25 % at the 0.5 ms minimum**
(24 samples at 48 kHz). Invariant 4 is unaffected — the clamp is downstream and unconditional —
so the consequence is that true-peak mode does measurably less of the limiting work at the bottom
of the lookahead range than at the top, not that anything escapes the ceiling. Recorded here
rather than compensated: subtracting the group delay from `wOs` would shift the detector tap for
every non-true-peak configuration too, for a correction smaller than one wedge entry above 2 ms.

## Metering cost, structural — not yet measured (DESIGN §9 ≤ 0.5 %)

Recorded as the starting point for the P6 CPU measurement, which is now the binding open number
for this subsystem. Per base sample and per channel, stage E runs four meters: `dryMeter`,
`wetMeter` and `outMeter` (two biquads each, K-weighting) plus `outTp` (3 interpolation phases ×
12 taps). That is ~6 biquads + ~72 MACs per frame **on top of** the chain, and `integratedLufs()`
walks 751 bins twice per block at the publish site. None of it allocates or locks, so
`REALTIME_SAFETY_AUDIT.md`'s claims are unaffected — but the §9 budget is the constraint that is
still `Unverified`, and the histogram cache (noted at the call site) is the obvious first
reduction. **Measure before adding another per-sample tap** (the P5 spectrum rings are the next
candidate).

## Reported-latency matrix (invariant 2, ADR-0004) — 2026-08-01, P2

Impulse-peak position vs `predictLatencySamples`, all cells (`testOsLatencyMatrix`):

| Factor | min-phase | linear-phase |
|---|---|---|
| Off | exact (480) | exact (480) |
| 2× | +1 sample (485 vs 484) | **exact** (529) |
| 4× | **exact** (486) | **exact** (541) |
| 8× | +1 sample (487 vs 486) | **exact** (545) |
| 16× | +1 sample (487 vs 486) | **exact** (547) |
| Force-Max offline (2× selected, 16× forced) | — | **exact** (547) |

Linear-phase cells are asserted sample-exact (a symmetric FIR's impulse peak *is* its group
delay). Min-phase cells are asserted within ±1: an IIR cascade's group delay is
frequency-dependent by design, and its impulse peak sits within a sample of the nominal
integer-compensated bulk delay that PDC reports.

## Oversampling round-trip transparency — 2026-08-01, P2

4× linear, defaults otherwise, 1 kHz tone: residual vs delay-aligned input **−69.0 dB**
(`testOsTransparency`, asserted < −60).

## Dither (§4.5, invariant 12) — 2026-08-01, P2

16-bit mode: every output sample on the 2⁻¹⁵ grid, LSB genuinely randomised (not plain
rounding). First-order noise shaping moves quantisation-error energy upward by **+12.6 dB**
(top-quarter vs bottom-quarter band energy ratio, against −0.1 dB unshaped) —
`testDitherModes`. Off is a true no-op, proven by the bit-exact null.

## LUFS accuracy (invariant 11, BS.1770-4) — 2026-08-01, P3

Synthesised calibration vectors (`testLufsCalibration`/`testLufsGating`, exact-frequency sines):

| Vector | Reads | Bound |
|---|---|---|
| 0 dBFS 997 Hz, ONE channel (the standard's compliance sentence) | −3.01 LKFS | ≤ 0.1 LU, 48 kHz AND 44.1 kHz |
| Same tone, both channels | +3.01 higher | ≤ 0.1 LU |
| −20 dBFS stereo (linearity) | −20.0 LUFS | ≤ 0.1 LU |
| −20 programme + trailing silence | −20.0 (absolute gate) | ≤ 0.15 LU |
| −20 programme + −45 tail | −20.0 (relative gate; ungated would read ≈ −26) | ≤ 0.3 LU |
| −20 + −38 band + 120 s silence | −20.0 (absolute gate keeps silence out of the relative threshold's base; without it ≈ −24.7) | ≤ 0.3 LU |

The file-based EBU R128 vector sweep (seq-3341 et al.) needs the vector FILES and lands with the
P3 meter publication work.

## Not yet measured (do not cite)

The file-based EBU R128 vector sweep, the dBTP meter against the BS.1770 vector set (P3),
listening results (P6). *(CPU/performance stood in this list until 2026-08-03 while the section
below already measured it — the line was not updated when the bench landed.)*

## Performance (re-measured 2026-09-27, `AnabasisBench` — full matrix in `docs/architecture/PERFORMANCE_BUDGET.md`)

Budget case **48 kHz · 512 · 4× OS · working state: 3.0 % of one core — 3.6 % with true-peak mode
on (ADR-0041)** (Intel Xeon @ 2.10 GHz,
gcc 13.3.0, Release at the BENCH target's flag set — not the plugin's; `PERFORMANCE_BUDGET.md`'s
build-configuration note owns that distinction and is not restated here — median ns/sample of the
timed `process()` region, 5×1 s runs) against the
brief-§10 target of ≈5 % on a modern desktop core. Null path ≈1.3 %; the 16× quality extreme
9.2 % (48 kHz) / 20.2 % (96 kHz). Machine + method travel with every quote (C2).

**The per-stage standalone costs are NOT repeated here.** They lived in this paragraph as a second
copy and went stale exactly as a second copy does: round 46 corrected the per-stage harness (the
stimulus generator had been inside the timed region) and re-measured, and four of the five figures
here still read at their pre-correction values — the project's own authority document had already
established them as wrong in the unsafe direction. `docs/architecture/PERFORMANCE_BUDGET.md` owns
the table and the verdicts; quote it, not this file.
