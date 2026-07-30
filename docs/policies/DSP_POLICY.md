# DSP_POLICY.md

**Priority: 3.** System Policy — the system-level DSP invariants for Anabasis. These must hold
across releases.

Derived from `docs/DEVELOPMENT_BRIEF.md` §3, §4, §6 and §10. **No compliance evidence exists
yet** (no `src/`): each invariant below states what the code must satisfy and names the test that
will guard it. Evidence citations are added as the modules land (constraint C7).

## Invariants (binding)

1. **The signal chain is strictly serial, in this fixed order** (§3):

   ```
   Input Gain → EQ (pre by default) → Compressor → Clipper + Saturation (colour)
   → Limiter (lookahead + true peak) → Ceiling → Dither → Output
   ```

   The EQ position switch (Pre/Post) moves the EQ block **only** between the two defined
   positions — before the compressor, or after the limiter. No other reordering exists.
   Guarded by: the chain-order test + the transfer-order test.
   *Any change here is a DSP signal-order change → Architecture Review Gate + ADR + Hard Stop.*

2. **Reported latency is exact and matches the measured latency.** Every latency source —
   lookahead and oversampling — is reported to the host so PDC compensates correctly. With
   oversampling off, the reported latency is **exactly the engaged lookahead**, and nothing else
   contributes. Latency must never change mid-block: an oversampling-factor or lookahead change is
   **latched** and applied at a reset or a crossfaded boundary.

   Note that `DEVELOPMENT_BRIEF.md` §4.3 specifies the limiter lookahead as **0.5–10 ms**, with no
   zero position — so on that range **the plugin always reports non-zero latency**, and "reports 0"
   is not a reachable state to test for. Whether lookahead gets an explicit **0 / off** position is
   a P0 `DESIGN.md` decision with real consequences (it is the only way to offer a zero-latency
   tracking mode, and adding it later widens a parameter range, which is itself an
   `ARCHITECTURE_REVIEW_GATE` item under `PARAMETER_COMPATIBILITY_POLICY.md` rule 3). Decide it
   before the parameter is created, not after.
   Guarded by: `testReportedLatencyMatchesImpulse` (impulse-response measurement across the
   oversampling × lookahead matrix, including both ends of the lookahead range).

3. **True-peak detection runs at ≥ 4× oversampling and is BS.1770-4 compliant.** A true-peak
   reading is an inter-sample estimate, not a sample peak; the ceiling is interpreted as dBTP when
   true-peak mode is on.
   Guarded by: the true-peak accuracy test (≤ 0.1 dB against known inter-sample-peak signals).

4. **The output never exceeds the ceiling.** A final safety clamp sits after the limiter and
   before dither, and holds **under every condition** — any input, any parameter combination, any
   automation rate, any sample rate, during and after every transition. Tolerance ≤ 0.1 dBTP in
   true-peak mode.
   This is the product's core promise; weakening it is an Architecture Review Gate item in its own
   right. Guarded by: `testOutputNeverExceedsCeiling` (hostile-input sweep).

5. **Oversampling wraps the nonlinear stages; linear stages stay at base rate.** The clipper /
   saturation stage and the true-peak detector are inside the oversampled region; the EQ and the
   metering taps that do not need it stay outside. Oversampling off ⇒ no oversampling latency.
   Guarded by: the latency-matrix test + the aliasing measurement.

6. **The soft-clip stage uses antiderivative antialiasing (ADAA)** in addition to global
   oversampling (§4.2). Aliasing/image suppression is **measured in dB**, not asserted, and
   recorded in `TEST_REPORT.md` with its method (constraint C2).

7. **Identity at zero / transparent by default.** With the plugin at its defaults and no
   processing engaged, output is a bit-exact (delay-aligned) copy of the input: unity input gain,
   flat EQ, compressor below threshold, clipper/saturation at zero drive, limiter below threshold,
   dither off. Bypass is a null test.
   Guarded by: `testNullWithDefaults`, `testBypassNull`.

8. **Every transition is click-free.** Toggling bypass, loudness compensation, delta monitoring,
   the oversampling factor, the EQ position, the mode switch, or a preset load must produce no
   click, pop, or level jump. All parameters are smoothed; discrete switches are crossfaded or
   ducked.
   Guarded by: the click-free transition tests (one per switchable path).

9. **No NaN / Inf / denormals leave the engine, and the engine self-heals.** Denormal protection
   (FTZ/DAZ) is active for the whole block; a non-finite sample anywhere resets the affected state
   rather than propagating. Filter cutoffs are Nyquist-clamped; automation cannot drive a
   coefficient out of range.
   Guarded by: `testNoBadSamples` across the algorithm × oversampling × sample-rate matrix,
   including silence and hostile automation.

10. **Loudness-compensated monitoring and loudness-matched bypass are honest** (§3). The
    compensation is a measurement-driven gain applied to the *monitoring* path; it must never
    alter the rendered output, and it must not become a continuous AGC. Delta monitoring
    (auditioning the difference signal) is likewise monitoring-only.
    Guarded by: `testLoudnessCompensationDoesNotAlterRender`.

11. **Metering accuracy is a contract, not a display detail.** LUFS (momentary / short-term /
    integrated, gated per BS.1770-4 / EBU R128) is accurate to **≤ 0.1 LU** against the official
    EBU test vectors; true peak to **≤ 0.1 dB**. PLR is derived (true peak − integrated LUFS), not
    independently estimated.
    Guarded by: the EBU R128 vector test + the inter-sample-peak test (`TESTING_POLICY.md` §20.4
    of the brief).

12. **Dither is off by default and is the last stage before output.** It is intended for final
    export only; enabling it must not change gain staging. TPDF, with optional noise shaping.

13. **The DSP core is format-agnostic.** `src/dsp/` depends only on `juce_dsp` /
    `juce_audio_basics` and is driven by a POD parameter snapshot; it never includes the plugin
    wrapper, the editor, or any JUCE GUI/plugin-client header.

## Invariant → test map

Maintained here as the modules land. Every invariant above must have at least one guarding test
where feasible (`TESTING_POLICY.md`). An invariant with no test is a documented gap, listed in
`docs/KNOWN_ISSUES.md`, not a silent one.

| Invariant | Guarding test | Status |
|---|---|---|
| 1 chain order | chain-order / transfer-order test | TODO (P2) |
| 2 latency exactness | `testReportedLatencyMatchesImpulse` | TODO (P2) |
| 3 true peak ≥ 4× | true-peak accuracy test | TODO (P3) |
| 4 ceiling never exceeded | `testOutputNeverExceedsCeiling` | TODO (P2) |
| 5 oversampling scope | latency-matrix + aliasing measurement | TODO (P2) |
| 6 ADAA | aliasing measurement (dB, recorded) | TODO (P2) |
| 7 identity at zero | `testNullWithDefaults`, `testBypassNull` | TODO (P1) |
| 8 click-free transitions | per-path click tests | TODO (P2) |
| 9 no NaN/Inf/denormals | `testNoBadSamples` | TODO (P1) |
| 10 monitoring honesty | `testLoudnessCompensationDoesNotAlterRender` | TODO (P3) |
| 11 metering accuracy | EBU R128 vectors + ISP signals | TODO (P3) |
| 12 dither placement/default | dither default + placement test | TODO (P2) |
| 13 format-agnostic core | build-level: `AnabasisDSP` links without the wrapper | TODO (P1) |

## Enforcement

- Any change to stage order, stage placement, oversampling scope, the ceiling clamp, or the
  latency contract is a **DSP signal-flow change** → Architecture Review Gate + ADR + an
  **AI Agent Hard Stop**.
- A change must keep the relevant self-tests green (`TESTING_POLICY.md`).
- Changing this policy requires an ADR.
