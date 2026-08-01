# TEST_REPORT.md

Measured data with methodology (constraint C2: no performance or accuracy number may be written
anywhere without the machine and method that produced it). This file accumulates as phases land;
the P6 release report grows out of it. Every number below is also asserted (with margin) by a
named test in `tests/dsp_tests.cpp`, so it cannot silently rot: a regression fails CI, and a
re-measurement updates both the test comment and this table.

**Environment for all measurements:** Linux x86-64 container, GCC, Release build (`-O3` via
`juce_recommended_config_flags`, no `-ffast-math`), JUCE 9.0.0 @ `f8f8864…`, fs = 48 kHz.
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

## True-peak estimator accuracy (invariant 3, ADR-0003) — 2026-08-01, P2

Estimator: 4-phase × 12-tap windowed-sinc, integer-normalised DC, designed at `prepare()`.
Stimulus: fs/4 sine, unit true peak, phase chosen to place the continuous peak on / between the
4× interpolation points.

| Vector | Reads | Test bound |
|---|---|---|
| Grid-aligned +3.01 dB ISP (peak on a 4× point) | **−0.004 dB** | ≤ 0.1 dB |
| On-sample peak | **+0.000 dB** | ≤ 0.1 dB |
| Off-grid worst case (peak midway between 4× points) | **−0.171 dB** | bounded (−0.6, 0.1] — the inherent max-reading 4× property; BS.1770's own tolerance envelope admits it |

Estimator group delay: 5.5 input samples ≈ 0.115 ms — inside the 0.5 ms minimum engaged
lookahead with >4× margin (RISK-008).

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

CPU/performance budget (P2/P6, needs a recorded machine spec), the file-based EBU R128 vector sweep, the dBTP meter against the BS.1770 vector set (P3), listening results (P6).
