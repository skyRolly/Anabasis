# DSP_POLICY.md

**Priority: 3.** System Policy — the system-level DSP invariants for Anabasis. These must hold
across releases.

Derived from `docs/DEVELOPMENT_BRIEF.md` §3, §4, §6 and §10. The P1 skeleton supplies the first
compliance evidence — invariants 7, 9 and 13 are guarded by live tests and 2, 4 and 8 by their P1
forms (see the invariant→test map below). The rest name the test that will guard them once their
stage exists; evidence citations are added as the modules land (constraint C7).

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
   **Scope: the PROGRAMME path** — the processed signal, and everything an offline render can
   emit. The two monitor-only audition legs are outside it by the same reading that lets bypass
   carry the unclamped dry signal (invariant 7): bypass monitoring plays the input as-is, and
   **delta monitoring** plays `dry − processed`, which on decorrelated material can reach roughly
   twice full scale. Both are listening-only (inert under `nonRealtime`, invariant 10), so no
   render can exceed the ceiling; the stated-explicitly rule exists because a reader of the
   pre-clarification text could take "under every condition" to cover the audition legs too.
   A third leg is render-visible and belongs in the same paragraph: **dither runs after the
   clamp** (ADR-0002 fixes that order — "ceiling clamp always last before dither"), and TPDF
   quantisation can round up by ≤ 0.5 LSB plus the ≤ 1 LSB dither offset, so an emitted sample
   can sit ~1.5 LSB above the ceiling. At 16-bit that is 4.6e-5 linear — **0.004 dB** at the
   lowest (−20 dB) ceiling, two orders inside the ≤ 0.1 dBTP tolerance this invariant already
   states, and zero with dither off (the default, and the configuration
   `testOutputNeverExceedsCeiling` runs in). Named because the strict reading of "never exceeds"
   would otherwise be falsified by the very stage the ADR puts there deliberately.
   Recorded 2026-08-01 (PR #5) — a clarification of scope, not a weakening of the promise.
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
   **Scope, the same carve-out invariants 4 and 12 carry** (recorded 2026-08-01, PR #5): the
   bypass null is a property of the **programme path** — bit-exact with the §2.7 monitor
   functions off, and bit-exact in every render, since both are snapped inert under
   `nonRealtime` (invariant 10). Auditioning with **Loudness Comp** engaged scales the bypass leg
   too, by design: the monitor gain is applied POST-mix precisely so that A/B-ing against bypass
   is loudness-matched, which is the feature (§2.7, ADR-0006 "monitoring never in the render
   path"). A bypass null measured with Loudness Comp on is measuring the monitor, not the
   invariant.
   Guarded by: `testNullWithDefaults`, `testBypassNull` (both with the monitor functions off),
   `testLoudnessCompensationDoesNotAlterRender` (the render-side half).

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
   **"Self-heals" is a recovery guarantee, not only a containment one.** Substituting `0.0f` at a
   boundary protects everything DOWNSTREAM of it and nothing else, so it is only half the
   invariant: a stage that overflowed on a legal float (a biquad's gain, a squared detector level,
   a fifth-power colour term, a polyphase filter) keeps the NaN in its own state, and a boundary
   that silently substitutes turns that into permanent silence with no signal that anything
   happened. Therefore every substituting boundary also RECORDS the substitution, and the stages
   that can generate one are REPAIRED on that record. Repair is value-level wherever the state is
   ours (`sanitiseState` clears the members that are non-finite and carries the rest, so a
   poisoned detector filter does not also cost the compressor its gain-reduction envelope — the
   rule `LookaheadLimiter::resetWindow` already followed); the oversampler is the exception,
   because its state is JUCE's and its default path is a polyphase **IIR** that feeds itself, so
   it takes a full `reset()` and only on the boundary that means the oversampler itself produced
   the value. A stage added to the chain must satisfy both halves — see the invariant 9 block in
   `AnabasisEngine::processChunk`. **A reachability argument is not a boundary.** The Post-EQ hole
   was created by one — "its input is the limited signal, bounded by the ceiling" — which ignored
   that the limiter's ATTACK is what bounds it, and that a short lookahead leaves the envelope at
   ~0.29 while a fully boosted EQ multiplies by ~3.4. Where a stage's corruption produces no
   non-finite output at all to detect, the check is unconditional and per block rather than hung on
   a flag. **Three stages are in that class**, and they are the ones whose failure is invisible
   rather than loud: the limiter's detector high-pass (a `NaN` level compares false against the
   ceiling, so the gain computer emits unity for ever), the BS.1770 meters (a `NaN` reading
   compares false against every gate, so the §2.7 compensation freezes and the integrated
   histogram stops accumulating), and the §5.4 feature extractor (a `NaN` feature fails the trim
   hysteresis, so the trim vector holds its last value for the session and looks plausible doing
   it). All three are fed signals the engine keeps finite but does not BOUND, which is why a legal
   float can break them at all.
   **"Self-heals" is not "recovers instantly", and the difference is stated so it is not read as
   one:** the repair runs at the END of the chunk that detected the contamination, so that whole
   chunk has already been processed with the poisoned state and its output is the boundaries'
   `0.0f`. The lookahead ring then holds up to `delayOs` of those zeroed samples, which read out
   over the following ~10 ms. Recovery is therefore **one chunk plus the lookahead line**, not one
   sample — a bounded silence on a signal that was already unusable, which is what graceful
   degradation means here.
   Guarded by: `testNoBadSamples` across the algorithm × oversampling × sample-rate matrix,
   including silence and hostile automation; `testExtremeLevelDoesNotSilencePermanently` for the
   recovery half; `testSelfHealDoesNotSnapTheEnvelope` for the manner of the recovery.

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
    **Scope, stated for the same reason invariant 4's is** (recorded 2026-08-01, PR #5): "last
    stage" means last on the **programme path**, and **three** legs sit downstream of the
    quantiser (the count was corrected from two on 2026-08-02 — the delta leg was missed):
    - the **§2.7 loudness-compensation gain**, applied post-mix so a loudness-matched bypass
      carries the same gain. Monitor-only, snapped inert under `nonRealtime` (invariant 10), so
      it never reaches a render: auditioning with Loudness Comp engaged does scale the dithered
      signal off the grid, which is correct — the monitor is not the export.
    - the **§2.8 bypass crossfade**, which is *not* monitor-only and does run in a render when a
      host automates `bypass`. Both endpoints are exact branches, so every steady state is on the
      grid; the ~10 ms ramp between them is a convex combination of the dithered wet leg and the
      **undithered** dry one, and those samples are off it. Accepted rather than fixed: moving
      the crossfade upstream of dither would send the dry leg through the quantiser, and
      **invariant 7 requires bypass to be a bit-exact null**. A bounded off-grid ramp on an
      audition toggle is the cheaper of the two, and the corrected claim is that a render is on
      the grid *except* across a bypass toggle — not unconditionally.
    - the **§2.9 delta substitution**, `wetLeg = dryForDelta − processed`: it subtracts the
      undithered delay-aligned dry signal from the dithered processed one, so an audition with
      Delta engaged is off the grid too. Monitor-only and snapped inert under `nonRealtime`, so
      like the compensation gain it never reaches a render — which is why the substance of the
      invariant is unchanged and only the enumeration was wrong.

13. **The DSP core is format-agnostic.** `src/dsp/` depends only on `juce_dsp` /
    `juce_audio_basics` and is driven by a POD parameter snapshot; it never includes the plugin
    wrapper, the editor, or any JUCE GUI/plugin-client header.

## Invariant → test map

Maintained here as the modules land. Every invariant above must have at least one guarding test
where feasible (`TESTING_POLICY.md`). An invariant with no test is a documented gap, listed in
`docs/KNOWN_ISSUES.md`, not a silent one.

| Invariant | Guarding test | Status |
|---|---|---|
| 1 chain order | `testLimiterPushDoesNotDriveTheClipper` (the push sits after Clip/Sat), `testEqPositionsAreDistinct` + `testOutputNeverExceedsCeiling` in BOTH EQ positions (the clamp is last before dither) | **live** (P2) |
| 2 latency exactness | `testReportedLatencyMatchesImpulse`, `testOsLatencyMatrix` | **live (P2)** — the impulse lands at exactly `maxLookahead + osLatency` for every lookahead value AND every factor × phase cell, Force-Max-offline included; linear-phase cells are sample-exact, min-phase cells within 1 sample of the nominal bulk delay (IIR dispersion, documented in the test) |
| 3 true peak ≥ 4× | `testTruePeakAccuracy`, `testLimiterTruePeakMode` | **partial (P2)** — the 4× measurement-tap estimator is live in the limiter's detector (grid-aligned ISP −0.004 dB, off-grid −0.171 dB recorded; the ceiling is dBTP-aware in true-peak mode); the full OS-matrix stimulus and the dBTP meter arrive with the oversampler (P2) and metering (P3) |
| 4 ceiling never exceeded | `testOutputNeverExceedsCeiling` | **partial (P2)** — the ADR-0002 mandated stimulus is live: BOTH EQ positions, the Post case with a +12 dB shelf after the limiter (mutation-verified: clamp moved upstream of the post EQ fails it); the ≤ 0.1 dBTP matrix still needs the true-peak tap (P2/P3) |
| 5 oversampling scope | `testOsLatencyMatrix`, `testOsReducesAliasing`, `testCeilingUnderOs`, `testBypassNullUnderOs` | **live (P2)** — the region wraps Clipper/Sat → Limiter; EQ/comp/clamp/dither at base rate; bypass stays bit-exact at every factor; measured: 4× drops the driven-clipper folded 3rd by ~74 dB beyond ADAA alone |
| 6 ADAA | `testClipAdaaReducesAliasing` | **partial (P2)** — first-order ADAA on the clip curve, measured at OS Off: the folded 3rd/5th of a driven 11.72 kHz tone drop 14.8 / 10.4 dB vs the memoryless curve (numbers recorded in the test); the OS × aliasing matrix arrives with the oversampler |
| 7 identity at zero | `testNullWithDefaults`, `testBypassNull` | **live (P1)** |
| 8 click-free transitions | per-path click tests | **live (P2)** — smoothed paths pinned (`testCeilingIsSmoothed`, `testLookaheadIsSmoothed`, `testEqGainIsSmoothed`); the §2.8 duck wraps every discrete rewire (`testDuckWrapsDiscreteRewires`, `testDuckWrapsOsLatch`) and the wrapper bulk swaps (`testDuckOnWrapperRequest`, `testAbSwitchRequestsDuck`) — all mutation-verified; loudnessComp/delta crossfades arrive with their P3 features |
| 9 no NaN/Inf/denormals | `testNoBadSamples`, `testExtremeLevelDoesNotSilencePermanently`, `testExtremeLevelDoesNotBreakTheMetersOrAdaptation`, `testSelfHealDoesNotSnapTheEnvelope` | **live (P1, extended P4)** — a non-finite value never leaves the engine, and the engine RECOVERS from one rather than degrading permanently. Both sources are covered: contamination that arrives (a hostile input buffer, zeroed before any state sees it) and contamination a stage generates from a legal float (EQ biquad in either position, RMS detector square, colour c⁵, polyphase IIR — each verified by its own stimulus, and each case dies against exactly one element of the recovery being reverted), and the stages that emit no audio to check at all (the meters and the feature extractor, repaired per block) |
| 10 monitoring honesty | `testLoudnessCompensationDoesNotAlterRender`, `testDeltaMonitor` | **live (P3)** — offline render bit-identical with comp on/off and with delta on/off; realtime monitor pulled to the dry loudness with the predict floor acting before the measure exists (all mutation-verified) |
| 11 metering accuracy | `testLufsCalibration`, `testLufsGating`, `testLufsWindows` | **partial (P3)** — LUFS M/S/I live against the standard's synthesised calibration points (997 Hz compliance vector −3.01 LKFS ≤ 0.1 LU at 48/44.1 kHz; both gate halves isolated by stimulus, incl. the silence-in-the-threshold-base case only mutation testing surfaced); the dBTP meter and the file-based EBU vector sweep remain |
| 12 dither placement/default | `testDitherModes` + `testNullWithDefaults` | **live (P2)** — Off default is a true no-op (the bit-exact null proves it); 16-bit lands on the 2⁻¹⁵ grid with a randomised LSB; shaping tilts the error spectrum +12.6 dB toward the top of the band; placement after the clamp, processed path only |
| 13 format-agnostic core | build-level: `AnabasisDSP` links without the wrapper | **live (P1)** — the `AnabasisTests` target compiles the core with no wrapper and no GUI |

## Enforcement

- Any change to stage order, stage placement, oversampling scope, the ceiling clamp, or the
  latency contract is a **DSP signal-flow change** → Architecture Review Gate + ADR + an
  **AI Agent Hard Stop**.
- A change must keep the relevant self-tests green (`TESTING_POLICY.md`).
- Changing this policy requires an ADR.
