# ADR-0003 — Oversampling wraps Clip/Sat and Limiter; true-peak detection is a measurement tap at >=4x total

**Status:** Accepted (2026-07-31 — owner sign-off on `docs/DESIGN.md`)

## Context

`DEVELOPMENT_BRIEF.md` §7 puts user oversampling on the Settings page as **Off / 2× / 4× / 8× / 16×**
with a **phase mode** (minimum phase / linear phase) and an **offline-render quality** switch
(Follow / Force Max). §4.3 independently requires **true-peak detection at ≥ 4× oversampling,
compliant with ITU-R BS.1770-4**, and §4.2 requires ADAA on the soft-clip stage *in addition to*
global oversampling. §10 sets the measurement gates the result must pass: true peak within **0.1 dB**
of known inter-sample-peak signals, output never above the ceiling within **0.1 dBTP**, clipper image
suppression measured in dB at **4×**, and roughly **5% of one core at 48 kHz stereo 4× OS**.

`DSP_POLICY.md` makes this binding from three directions: invariant 3 (true peak ≥ 4×, BS.1770-4),
invariant 5 (oversampling wraps the nonlinear stages, linear stages stay at base rate) and invariant 2
(reported latency is exact and is the sum of *whatever actually delays the signal*). Invariants 2 and 5
were written with an **open point** left in them precisely here — invariant 5's "oversampling off ⇒ no
oversampling latency" could only be *assumed* while the ≥ 4× true-peak path was unspecified, because a
detector that resamples the **output** would put a resampler in the render path at every setting,
including Off. `DESIGN.md` §3 was tasked with resolving that open point and §3.1–3.2 do so.

`ADR_POLICY.md` makes an ADR mandatory for **oversampling strategy** and **latency behaviour**, and
rule 5 makes an ADR the only instrument that can enact a Policy change. `DESIGN.md` §3.2 therefore
left `DSP_POLICY.md` untouched and deferred the invariant 2 + 5 edits to this ADR (constraint C6).

The sibling product gives mechanics but not scope: the P0 research pass recorded that Anamorph's
ADR-0003 oversamples only its nonlinear/modulation region and reports **0** latency when nothing
nonlinear is active, with **no** true-peak obligation at all — so its engagement predicate and its
scope cannot be copied, only its filter construction, its latched-switch discipline and its
pre-built-instance rule (`worklogs/2026-07-30-p0-anamorph-research.md`).

**This ADR carries a policy amendment.** See *Decision*, item 8.

## Problem

Two questions, and they are entangled — which is why one ADR settles both.

1. **How much of the chain does the user's oversampling factor wrap?** Oversampling costs latency and
   CPU at every factor, and the region's boundaries are not free to choose: ADR-0002 fixes the ceiling
   clamp as the last stage before dither and *downstream of a Post-position EQ*, so the clamp cannot be
   pulled inside any region that excludes the EQ. Wrapping more than the harmonic-generating stages
   buys nothing measurable and spends the ≈5%-of-a-core budget.

2. **Is the ≥ 4× true-peak path in the signal path or beside it?** This is the non-obvious one. The
   brief's phrase "peak detection at ≥ 4× oversampling" reads two ways. If the ≥ 4× estimator is an
   *in-path* processor — the audio actually upsampled, limited and downsampled at ≥ 4× whenever
   true-peak mode is on — then **an "Off" oversampling setting cannot exist**: `truePeakMode` defaults
   on (§4.2 row 33), so every session would carry the group delay of a resampler the user explicitly
   switched off, and invariant 2 would have to report it. If instead the estimator is a **measurement
   tap** — an interpolator whose output is a *number*, consumed by gain computers and meters, never
   written back into the audio — then it delays nothing and Off means Off. Both readings satisfy
   invariant 3's letter. Only one of them lets invariant 5's own sentence be true.

The follow-on question the tap reading forces: if the detector never resamples the output, **what runs
it at ≥ 4× when the user's factor is Off or 2×**, without double-resampling when the factor is already
≥ 4×?

## Options

- **A. Oversample the whole chain at the user factor.** Simplest to reason about; one rate for
  everything. **Lost:** the EQ is minimum-phase IIR and the compressor's gain signal is band-limited by
  its 5 ms minimum attack (§2.2, §2.3), so neither aliases audibly at glue ratios ≤ 4:1 — they would
  pay latency and CPU for nothing against a ≈5% budget. Worse, it is *structurally impossible* as
  stated: ADR-0002 requires the ceiling clamp to sit downstream of the Post-position EQ and be the last
  stage before dither, so clamp and Post-EQ must be outside the region; and every metering tap (§2.9)
  would then read a resampled signal and need its own down-conversion to report base-rate figures.
- **B. Wrap `Clipper/Sat → Limiter` only; EQ, compressor, ceiling clamp, dither and all metering taps
  stay at base rate.** The two adjacent stages that actually generate harmonics — the clip/sat
  waveshaper and the limiter's gain modulation — sit inside one region with one up/down boundary.
  **Chosen.**
- **C. Wrap the clipper/saturator only; leave the limiter at base rate.** Narrower still. **Lost:** the
  limiter is a nonlinearity too — its gain computer modulates the signal and generates its own
  products, which is why §3.1 names *both* stages as harmonic generators — and splitting two adjacent
  nonlinear stages inserts a second resampling boundary between them, adding filter latency and cost to
  suppress aliasing that the boundary itself did not remove. It also destroys the free ≥ 4× detector
  input of option F: at user factors ≥ 4× the limiter's detector could no longer read an already-
  oversampled signal, so *every* setting would need its own interpolator.
- **D. True-peak in-path — the output actually limited and rendered at ≥ 4× whenever true-peak mode is
  on.** The literal reading of "detection at ≥ 4× oversampling", and it makes the limiter's control
  signal and the audio share one rate. **Lost:** it forces nonzero oversampling latency even at **Off**,
  contradicting the brief §7 Off position and making invariant 5's "off ⇒ no OS latency" false by
  construction; it makes `truePeakMode` (row 33) a latency-affecting parameter, which footnote ⁴
  explicitly denies; and it buys nothing the ceiling clamp plus the estimator do not already guarantee
  inside the ≤ 0.1 dBTP tolerance, since the clamp's hard-clip backstop is the thing that actually
  enforces invariant 4.
- **E. One fixed 4× detector interpolator at every user setting, never reading the oversampled
  signal.** Uniform code path, one input path to test. **Lost:** at user factors ≥ 4× it resamples a
  signal that is *already* at ≥ 4×, spending the ≤ 1.5% OS-resampling allocation twice and stacking a
  second interpolation on the detector's estimate for no accuracy gain.
- **F. Detector rate composed per setting — own 4× at Off, a further ≥ 2× at 2×, direct read at ≥ 4×.**
  Costs a per-setting input path and therefore a per-setting accuracy test. **Chosen** (see Decision 6).
- **G. Linear-phase FIR half-band always.** Removes phase distortion from the OS filters entirely.
  **Lost:** FIR group delay and pre-ringing on every setting, including the default. Anamorph's ADR-0003
  chose minimum-phase IIR for exactly this trade — "no linear-phase pre-ringing / waveform
  misalignment", the prioritised property [Verified] — and a maximizer's transients are the last place
  to accept pre-ringing by default. Retained as an *elected mode*, since the brief §7 requires the
  phase-mode switch; not made the default.
- **H. Copy Anamorph's signal-dependent engagement predicate** (`oversample != Off && (driveDb > 0.01
  || isModAlgorithm)`, `Anamorph:docs/.../ADR-0003` [Verified]). Saves CPU and latency when the user has
  selected a factor but is not driving the clipper. **Lost:** it makes reported latency a function of
  `clipDrive` (row 21), a host-**automatable** parameter — so an automation lane could spray PDC changes
  mid-playback, the exact failure §3.3 spends the constant-lookahead-allowance decision to avoid. §3.3's
  latency model is `maxLookahead + osLatency(factor, phaseMode)` with no signal term, and this option
  contradicts it.
- **I. Design the linear-phase FIR kernels on a worker thread at switch time.** Avoids paying kernel
  memory for a mode that may never be used. **Lost:** §1.4 fixes the threading model at two threads with
  **no workers in v1**; a worker would be a threading-model change (Hard Stop, `ADR_POLICY.md`), and
  kernel design at switch time reintroduces the allocation-on-switch hazard the pre-built-instance rule
  exists to remove.

## Decision

1. **The oversampled region is `Clipper/Sat → Limiter`, and nothing else.**

   ```
   Input Gain → [EQ pre] → Comp → ⟦ OS ×N: Clip/Sat (ADAA) → Limiter ⟧ → [EQ post] → Ceiling → Dither
   ```

   EQ (either position), the compressor, the **ceiling clamp**, dither and **every metering and feature
   tap** (§2.9, §5.4) run at base rate. The clamp's base-rate placement is load-bearing, not
   incidental: ADR-0002 requires it downstream of the Post-position EQ, which is itself outside the
   region.

2. **Engagement is unconditional on the factor.** The region is wrapped whenever
   `int_oversample != Off`, with no signal- or parameter-dependent predicate. Reported latency is
   therefore a pure function of `(factor, phaseMode)` as §3.3 specifies, and no automatable parameter
   can move it.

3. **Filters.** JUCE **minimum-phase polyphase IIR half-band** stages with `useIntegerLatency = true`,
   so PDC is exact (precedent `Anamorph:src/dsp/AnamorphEngine.cpp:44-56` and its ADR-0003 [Verified]).
   `int_osPhase = linear` swaps the same region to **FIR half-band** stages with **precomputed** kernels,
   at the cost of FIR group delay and pre-ringing; minimum phase is the default (⊕ min, §4.3).

4. **All instances for every factor are constructed and `initProcessing`'d at `prepare()`**, sized for
   the maximum factor (16×), so a factor or phase change never allocates and no worker thread is needed
   (§1.4). A factor or phase change is **latched** and applied at a reset or at the silent bottom of the
   §2.8 duck (~6 ms out / ~28 ms in), with stale oversampler state reset there; latency never changes
   mid-block. `int_oversample` / `int_osPhase` are host-hidden (§4.3), so they are never carried by a
   preset, an A/B slot or an undo step.

5. **Offline render — `int_offlineQuality` Follow / Force Max.** At **Force Max** an offline bounce
   renders at **16×** regardless of the live setting. The factor change rides the existing latched-switch
   path at render start, and reported latency during `isNonRealtime()` uses the **forced** factor, so the
   host compensates the render it actually gets.

6. **True-peak detection is a MEASUREMENT TAP.** The BS.1770-4 ≥ 4× polyphase FIR interpolator
   (`src/dsp/TruePeak.h`) produces an *estimate*. That estimate feeds exactly three consumers — the
   limiter's gain computer, the ceiling clamp's decision, and the TP/PLR meter. **The audio that reaches
   the output is never resampled by it, and it therefore contributes no delay to the reported figure.**

   Detector **total** rate is ≥ 4× at every user setting (invariant 3), composed per setting:

   | `int_oversample` | Detector input | Detector interpolation | Total rate |
   |---|---|---|---|
   | Off | base-rate signal | own 4× interpolator | 4× |
   | 2× | the 2×-region signal | a further ≥ 2× | ≥ 4× |
   | 4× / 8× / 16× | the oversampled signal, read directly | none | = factor, ≥ 4× |

   No setting double-resamples. The **ceiling clamp's** tap (ADR-0002, §2.6) is a distinct instance whose
   input is base-rate by construction — the clamp sits outside the region — so it always runs its own 4×
   interpolation regardless of the user factor. That follows from ADR-0002; it is not a new placement.

7. **The zero-latency claim rests on one design constraint, and it is named rather than assumed.** The
   estimator's group delay must fit *inside* the lookahead window — i.e. be ≤ the 0.5 ms **minimum
   engaged** lookahead (§3.4). Design arithmetic, not a measurement (C2): the BS.1770-4 Annex 2
   48-coefficient, 4-phase interpolator has group delay (48 − 1)/2 = 23.5 taps at 4× ≈ 5.9 base samples
   ≈ 0.12 ms at 48 kHz. **To be verified by the P2 impulse test**, tracked until then as
   `FUTURE_RISKS.md` **RISK-008**. §3.3 makes the fallback cheap: if the measured delay exceeds the
   minimum engaged lookahead, it is absorbed by raising the minimum read offset inside the fixed 10 ms
   line, so the *reported* figure does not move and no ADR amendment or Architecture Review is triggered.

8. **Policy amendment (`ADR_POLICY.md` rule 5).** `DSP_POLICY.md` invariants 2 and 5 **drop their "open
   point" paragraphs** and assert the measurement-tap reading:

   > **Invariant 2 gains:** True-peak detection is a **measurement tap**. Invariant 3's ≥ 4× requirement
   > is met by an estimator that feeds only the peak estimate, the limiter's gain computer and the
   > ceiling clamp's decision — never the audio that is output — so it contributes **no** delay. With
   > user oversampling off, reported latency is therefore exactly the lookahead allowance, and the
   > detector adds nothing to it. The design constraint that makes this true is that the estimator's
   > group delay fits inside the 0.5 ms minimum engaged lookahead; verified by impulse measurement at P2
   > and tracked as `FUTURE_RISKS.md` RISK-008 until then.

   > **Invariant 5 becomes:** Oversampling wraps the nonlinear stages; linear stages stay at base rate.
   > The region is **Clipper/Saturation → Limiter**. The EQ, the compressor, the **ceiling clamp** —
   > which must sit after the Post-position EQ per invariant 1 — dither and the metering taps all stay
   > at base rate. **"Oversampling off ⇒ no oversampling latency" now holds unconditionally**, because
   > the ≥ 4× true-peak path is a measurement tap; the sentence is asserted, no longer assumed.

   Invariant 3 is **not** amended — it already says ≥ 4× and BS.1770-4, and this ADR states how that is
   reached. Invariant 2's *other* amendments (dropping lookahead from the latch sentence, re-phrasing
   against the lookahead **allowance**) belong to **ADR-0004** and are disjoint from the paragraph above;
   the two ADRs touch invariant 2 in different places on the same date.

9. **Guards.** The ≤ 0.1 dB true-peak accuracy test (invariants 3 and 11) runs **across the whole
   oversampling matrix** — Off / 2× / 4× / 8× / 16× × both phase modes — because item 6 makes the
   estimator's *input path* differ per setting, so a single-setting pass proves nothing about the
   others; it covers the clamp's tap as well as the limiter's detector (ADR-0002). The latency-matrix
   test plus `testReportedLatencyMatchesImpulse` assert `maxLookahead + osLatency(factor, phaseMode)`
   over the OS × lookahead matrix, with Off reporting the lookahead allowance and nothing else. The
   aliasing/image-suppression measurement is recorded in dB at 4× (invariant 6, brief §10). OS factor and
   phase each get their own click-free path test under invariant 8.

## Consequences

- **Invariant 5's own sentence becomes provable.** "Off ⇒ no oversampling latency" is now a testable
  assertion rather than an assumption resting on an unresolved detector question; the Off column of the
  latency matrix is a hard expectation.
- **Invariant 3 is satisfied without putting a resampler in the render path.** The ≥ 4× obligation is
  met by measurement, so `truePeakMode` (row 33) is genuinely not latency-affecting — which is what
  footnote ⁴ asserts and what lets it be frozen non-automatable for unrelated reasons.
- **The ceiling guarantee is unchanged in mechanism.** The estimate informs the clamp's gain; the
  sample-level hard clip remains the backstop that actually enforces ≤ 0.1 dBTP (invariant 4). This ADR
  does not make the clamp depend on the estimator being exact — only on it being within tolerance.
- **A per-setting accuracy obligation is created.** Three distinct estimator input paths (own 4×,
  compose-to-≥ 4×, direct read) mean three distinct sources of error; the OS-matrix sweep is the price of
  not double-resampling, and it is charged to P3.
- **CPU shape.** At the 4× budget point the region carries one up/down boundary (≤ 1.5% OS resampling)
  and the detector reads the oversampled signal for free; at **Off** and **2×** the detector's own
  interpolator is charged against the ≤ 1.5% limiter + TP-detection allocation instead — the budget
  total (§9) does not move, its distribution does. 16× is a user or Force-Max election, not a default.
- **Memory is sized for the worst case up front:** oversampler instances for every factor plus the FIR
  kernels exist from `prepare()`, and the dry ring is sized
  `maxLookahead(10 ms) + maxOsLatency(16×, linear) + maxBlock + 1` (§3.3).
- **Linear-phase mode's cost is user-elected and honestly reported:** FIR group delay enters
  `osLatency(factor, phaseMode)` and therefore the reported figure, and pre-ringing is accepted as the
  thing the user asked for. It is not the default.
- **Deliberate divergence from the sibling product, recorded so it is not "fixed" later.** Anamorph
  gates its OS wrap on drive/algorithm and reports 0 latency when the chain is linear; Anabasis does
  not, because that predicate would make PDC a function of an automatable parameter. Code adapted from
  Anamorph under ADR-0009 (this repository's) must not carry the engagement predicate across.
- **Forecloses:** pulling the ceiling clamp or the metering taps inside the OS region; a signal-dependent
  engagement predicate; an in-path true-peak limiter; a detector that double-resamples at ≥ 4×;
  designing FIR kernels off the message thread. Each re-enters the Architecture Review Gate as an
  oversampling-scope or latency change.
- **Doc-sync obligation:** the invariant 2 + 5 edits land in `DSP_POLICY.md` with this ADR, which is
  registered in `ADR_INDEX.md` (`ADR_POLICY.md` rule 1); `architecture/LATENCY_MODEL.md` records the
  *measured* sample counts at P2 (C2 — nothing above is a measurement), and `procedures/TESTING.md`
  gains the OS-matrix stimulus for the true-peak accuracy test, per
  `DOCUMENTATION_LIFECYCLE_POLICY.md`.

## Related code

None yet — P1 onward. Planned: `src/dsp/AnabasisEngine.{h,cpp}` (OS region wrap, latched factor/phase
switch, `predictLatency`), `src/dsp/TruePeak.h` (BS.1770-4 ≥ 4× estimator shared by the limiter
detector, the clamp tap and the meter), `src/dsp/ClipSat.{h,cpp}` and
`src/dsp/LookaheadLimiter.{h,cpp}` (the two stages inside the region), `src/dsp/CeilingClamp.h` and
`src/dsp/LoudnessMeter.{h,cpp}` (base-rate consumers of the estimate), `src/dsp/EngineParameters.h`
(POD snapshot carrying factor, phase mode and offline quality), `src/InternalState.h`
(`int_oversample`, `int_osPhase`, `int_offlineQuality` + the PDC-recompute callback),
`src/PluginProcessor.{h,cpp}` (single `setLatencySamples` call site, `isNonRealtime()` handling).

Evidence [Unverified] — Anabasis has no `src/`, so every runtime claim above is the contract the code
must satisfy, not a measurement (constraint C2); the 0.12 ms figure in Decision 7 is design arithmetic
awaiting the P2 impulse test (RISK-008):

- Design: `docs/DESIGN.md` §3.1 (OS region, filters, phase mode, Force-Max), §3.2 (measurement tap,
  per-setting detector rate, the rejected in-path alternative), and supporting §1.2 (chain + tap
  diagram), §1.4 (two threads, pre-built instances, precomputed kernels), §2.3 (compressor at base
  rate), §2.6 (clamp at base rate with its own tap), §2.8 (latched switch at the duck bottom), §2.9
  (base-rate metering taps), §3.3–3.4 (latency model, no zero lookahead), §4.2 rows 21/27/33 and
  footnote ⁴, §4.3 (`int_oversample`, `int_osPhase`, `int_offlineQuality`), §9 (budget allocation),
  §10 (this ADR's scope), §11 (sign-off checklist, RISK-008)
- Research: `worklogs/2026-07-30-p0-anamorph-research.md`
- Policy amended by this ADR: `docs/policies/DSP_POLICY.md` invariants 2 and 5 (`ADR_POLICY.md` rule 5)
- Precedent [Verified]: `Anamorph:src/dsp/AnamorphEngine.cpp:44-56` — pre-built, pre-`initProcessing`'d
  minimum-phase polyphase IIR half-band oversamplers with `useIntegerLatency=true`, one per factor, so a
  factor switch never allocates
- Precedent [Verified]: `Anamorph:docs/architecture/design-decisions/ADR-0003-oversampling-strategy.md`
  — nonlinear-only OS scope, exact integer PDC, latched engagement, and the IIR-over-FIR trade
  (no pre-ringing); its signal-dependent engagement predicate is the part deliberately **not** copied
- Precedent [Verified]: `Anamorph:src/dsp/AnamorphEngine.cpp:290-307` — forced-duck dry-fill engages only
  when `predictLatency == latched latency`, the mechanism that makes host-hidden OS factor/phase the only
  latency source acceptable
- Precedent [Verified]: `Anamorph:docs/architecture/design-decisions/ADR-0004-clickfree-transition-strategy.md`
  — the duck/crossfade taxonomy the latched factor/phase change is routed through
