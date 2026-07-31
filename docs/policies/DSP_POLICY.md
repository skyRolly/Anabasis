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
   → Limiter (lookahead + true peak) → [EQ (post)] → Ceiling → Dither → Output
   ```

   The EQ position switch (Pre/Post) moves the EQ block **only** between the two defined
   positions — before the compressor, or after the limiter and **before the ceiling clamp**. The
   ceiling clamp is **always** the last stage before dither, in both positions. No other
   reordering exists.

   **Why the clamp placement is part of this invariant.** It is not a preference: a post-limiter
   shelf of up to +12 dB re-introduces overshoot, so a clamp
   upstream of it could not satisfy invariant 4, and "the output never exceeds the ceiling" would
   be false in the Post position. Amended by **ADR-0002** (2026-07-31) — the pre-amendment text
   printed the clamp immediately after the limiter and left the Post-EQ's placement relative to it
   unstated, which a literal reader could take as EQ-after-clamp.
   Guarded by: the chain-order test + the transfer-order test + `testOutputNeverExceedsCeiling`
   run in **both** EQ positions.
   *Any change here is a DSP signal-order change → Architecture Review Gate + ADR + Hard Stop.*

2. **Reported latency is exact and matches the measured latency.** *Every* latency source is
   reported to the host so PDC compensates correctly — reported latency is the sum of whatever
   actually delays the signal, not a subset chosen for convenience.

   ```
   reportedLatency = maxLookaheadSamples(10 ms, sr)   ← CONSTANT, not the engaged value
                   + osLatency(factor, phaseMode)     ← 0 when the factor is Off
   ```

   **The lookahead contributes its MAXIMUM, always** (ADR-0004, 2026-07-31). The limiter reads at
   a variable offset inside a fixed 10 ms delay line and the engine pads the difference, so the
   *engaged* lookahead moves freely while the *reported* figure never does. The reason is a
   compatibility one, not an aesthetic one: `lookahead` is carried by every preset, A/B slot and
   undo step, so under the obvious `engagedLookahead + OS` model, browsing presets or A/B-comparing
   **during playback** would change host PDC on nearly every step.

   `osLatency` is a function of the **effective** factor, which is not always the one the user
   selected: at `int_offlineQuality = Force Max` an offline bounce renders at 16×, so the reported
   figure under `isNonRealtime()` uses the forced factor (ADR-0003/ADR-0004). Reported latency
   therefore has **three** inputs — `int_oversample`, `int_osPhase` and `int_offlineQuality` — all
   host-hidden, plus the realtime→offline transition itself; PDC recomputes on all four
   (ADR-0011).

   Latency must never change mid-block: an **oversampling-factor or phase-mode** change is
   **latched** and applied at a reset or a crossfaded boundary. Both are named because both are
   inputs to `osLatency(factor, phaseMode)` — linear-phase FIR stages carry group delay that the
   minimum-phase path does not — so latching the factor alone would leave a phase switch free to
   move reported latency mid-block. *(Pre-ADR-0004 this sentence also named lookahead; a lookahead
   change no longer alters any reported figure, so it is an ordinary smoothed read-offset move — but
   it is still a switchable path under invariant 8 and needs its own click-free test.)*

   **True-peak detection is a measurement tap** (ADR-0003, 2026-07-31; this closes what was an
   open point here). Invariant 3's ≥ 4× requirement is met by an estimator that feeds only the
   peak estimate, the limiter's gain computer and the ceiling clamp's decision — never the audio
   that is output — so it contributes **no** delay. With user oversampling off, reported latency
   is therefore exactly the **lookahead allowance**, and the detector adds nothing to it. The
   design constraint that makes this true is that the estimator's group delay fits *inside* the
   0.5 ms minimum **engaged** lookahead; it is verified by impulse measurement at P2 and tracked
   as `FUTURE_RISKS.md` RISK-008 until then.

   `DEVELOPMENT_BRIEF.md` §4.3 specifies the limiter lookahead as **0.5–10 ms**, and ADR-0004
   ratified **no zero/off position** — so **the plugin always reports non-zero latency** and
   "reports 0" is not a reachable state to test for.
   Guarded by: `testReportedLatencyMatchesImpulse` (impulse-response measurement across the
   oversampling × lookahead matrix — the impulse must land at exactly `maxLookahead + OS` for
   **every** lookahead value, so a padding bug is a test failure rather than a host-sync
   complaint).

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

5. **Oversampling wraps the nonlinear stages; linear stages stay at base rate.** The region is
   **Clipper/Saturation → Limiter**. The EQ, the compressor, the **ceiling
   clamp** — which must sit after the Post-position EQ per invariant 1 — dither and the metering
   taps all stay at base rate, **with one named exception: the true-peak estimator.** Its own rate
   varies by setting (**ADR-0003 decision item 6** — *not* invariant 6 of this policy: its own 4×
   interpolator at OS Off, a further ≥ 2× at 2×, and the
   oversampled signal read directly at ≥ 4×) so that invariant 3's ≥ 4× requirement holds at every
   setting. That is consistent with "metering taps stay at base rate" because the estimator is a
   *measurement tap*, not an audio capture point — the capture points for LUFS, spectrum and GR
   history are all base-rate. **"Oversampling off ⇒ no oversampling latency" now holds
   unconditionally**, because the ≥ 4× true-peak path is a measurement tap; the sentence is
   asserted, no longer assumed. *(Amended by ADR-0003, 2026-07-31.)*
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
   the oversampling factor, **the oversampling phase mode**, the EQ position, **the colour model**,
   **the lookahead**, the mode switch, or a **bulk swap — a preset load, an A/B switch, or an undo
   step, three routes through the same forced duck, each owed its own test** — must produce no
   click, pop, or level jump. All parameters are smoothed; discrete switches are crossfaded or
   ducked. *(Amended by ADR-0004, 2026-07-31.)*
   Guarded by: the click-free transition tests (one per switchable path).

   **Lookahead is named explicitly because it is the one switchable path with neither a duck nor a
   latch** (ADR-0004 made reported latency constant in it, so it is an ordinary smoothed change).
   That makes it the path most likely to be skipped at P1. Its click-free mechanism is specific:
   the **audio** delay stays fixed at the full 10 ms allowance and the lookahead value moves only
   the *detector / gain-computer* alignment — a smooth, band-limited control signal — so no audio
   sample is ever skipped or repeated. A per-path click test is owed for it like every other entry
   in this list.

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
    Guarded by: the EBU R128 vector test + the inter-sample-peak test (`TESTING_POLICY.md`
    §"Anabasis measurement gates", which restates `DEVELOPMENT_BRIEF.md` §20.4).

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
