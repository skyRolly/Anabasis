# ADR-0006 — Ceiling guarantee: a separate final clamp, monitoring never in the render path

**Status:** Accepted (2026-07-31 — owner sign-off on `docs/DESIGN.md`)

> **Amended by [ADR-0015](ADR-0015-pre-ship-contract-refreeze.md) (2026-08-06)** — the two
> DEFAULTS this record quotes in passing have moved: `ceiling` is **−0.1** (Context, "default
> ⊕ −1.0") and `truePeakMode` is **off** (option E, "row 33, default ⊕ on"). Nothing in the
> Decision changes: the clamp is still a separate stage, still last before dither, still with its
> own true-peak tap, and item 3's **mode-conditional rule is exactly what now governs the shipped
> default** — with true-peak mode off the clamp decides on the sample peak, which is why ADR-0015
> also makes the Ceiling's displayed unit follow the mode instead of asserting dBTP.

## Context

`DSP_POLICY.md` invariant 4 states the product promise: *the output never exceeds the ceiling*,
held by a final safety clamp after the limiter and before dither, **under every condition** — any
input, any parameter combination, any automation rate, any sample rate, during and after every
transition — with tolerance ≤ 0.1 dBTP in true-peak mode. The policy calls it the product's core
promise and makes weakening it an `ARCHITECTURE_REVIEW_GATE.md` item in its own right. Invariant 10
is its counterpart on the listening side: loudness-compensated monitoring and loudness-matched
bypass are a *monitoring* gain that must never alter the rendered output and must not become a
continuous AGC; delta monitoring is likewise monitoring-only.

ADR-0002 settled **where** the clamp sits — `… Limiter → [EQ(post)] → Ceiling → Dither`, always the
last stage before dither in both EQ positions. This ADR settles **what the clamp is**: a stage of
its own rather than a property of the limiter, what signal it decides on, and — because the two
questions share one invariant pair — how the monitoring layer that makes a maximizer comparable is
kept structurally incapable of reaching the render. `ADR_POLICY.md` names "the true-peak ceiling
guarantee" among the expected P0 decisions and makes an ADR mandatory for DSP stage placement.

Two facts from the surrounding design frame it. First, `ceiling` (§4.2 row 6, −20…0 dBTP,
default ⊕ −1.0) **is** the limiter's threshold — `DESIGN.md` §2.5 deliberately exposes no separate
limiter threshold — so the limiter and the clamp share a number without sharing a stage. Second,
the sibling product has **no output clipper anywhere**, a headroom decision recorded in Anamorph's
ADR-0009; Anabasis inverts it on purpose, because a maximizer's contract *is* the ceiling.

## Problem

**(a) Is the guarantee a property of the limiter, or a stage of its own?** A second stage looks
redundant: the limiter already reads the true-peak envelope (§3.2), its threshold already *is* the
ceiling, and a well-behaved lookahead limiter is expected not to overshoot. Three things break that
reading. The limiter's detector reads *its own* input point, so once a Post-position EQ sits
between the limiter and the output (ADR-0002) a shelf of up to +12 dB re-introduces overshoot the
limiter never saw. The limiter is a *quality* stage whose behaviour is tunable by ear —
`limStyle`, `limRelease`, `transientPreserve`, `stereoLink` (rows 28–32) all shape how gently it
acts — and a guarantee that is a function of a taste parameter is not a guarantee. And invariant 4
demands the property hold *during and after every transition*, where a gain computer with an attack
window and a smoothed gain trajectory is exactly the thing that can be caught mid-glide.

**(b) Where does loudness compensation live?** A maximizer's whole job is to come back louder, so
any honest A/B needs a compensating gain, and — unlike the sibling, where Drive could push either
way — that gain is essentially always an *attenuation*. A gain that large sitting anywhere in the
chain couples the ceiling guarantee to a monitoring toggle: downstream of the clamp it breaks
invariant 4 outright, upstream of it makes the clamp's workload depend on whether the user is
listening or comparing. Anamorph's Measure+Predict is the nearest precedent but was built with the
opposite polarity and freezes on silence against a −60 dBFS mean-square threshold — which the P0
research pass flagged as too high for mastering material, since quiet classical passages sit below
it and would freeze compensation on exactly the programme this plugin exists for.

## Options

**The clamp.**

- **A. No separate clamp — rely on the limiter alone.** The cheapest option and the one most
  maximizers imply they take: the limiter's threshold is the ceiling, so its output is at the
  ceiling by construction. **Lost:** it fails all three tests above. In the Post EQ position a
  +12 dB shelf downstream of the limiter escapes it entirely and invariant 4 is void by stage order,
  not by bug; the guarantee would be conditioned on `limStyle`/`transientPreserve` tuning; and
  "under every condition … during and after every transition" is not a property any gain-computer
  trajectory can be asserted to have at the sample. It also leaves nothing to write
  `testOutputNeverExceedsCeiling` against other than the limiter's own tuning.
- **B. Clamp inside the limiter — fold the final clip into the limiter's output stage.** Keeps one
  stage, one true-peak estimator, one gain path, and satisfies invariant 4 in the Pre position.
  **Lost:** a post-limiter EQ boost escapes it — the clamp would sit upstream of the Post-position
  EQ, which is precisely the arrangement ADR-0002 rejected as making invariant 4 unsatisfiable. It
  also entangles a safety stage with a tunable one: every future limiter-style change becomes a
  change to the product promise, and the Architecture Review Gate would have to treat limiter voicing
  work as ceiling work.
- **C. A structurally separate stage, always last before dither, with its own true-peak estimator
  tap on its own input.** Costs a second estimator instance and a second stage's state. **Chosen.**
- **D. Clamp at the oversampled rate, inside the OS region.** Would let the clamp decide on the same
  ≥ 4× signal the limiter works on. **Lost:** the clamp must be downstream of the Post-position EQ,
  which is a base-rate stage outside the OS region (§3.1), so pulling the clamp inside puts it
  upstream of the EQ — option B's failure — and would additionally make the guarantee's mechanism a
  function of `int_oversample`, a host-hidden setting (§4.3).
- **E. Sample-peak-only clamp — a plain hard clip at the ceiling, no estimate.** Simple, exact, and
  cheap. **Lost as the whole mechanism:** with `truePeakMode` on (row 33, default ⊕ on) the ceiling
  is interpreted as dBTP (invariant 3) and the tolerance is stated in dBTP, so clipping samples
  leaves inter-sample overshoot the clamp never measured. Retained *inside* option C as the
  sample-level backstop beneath the true-peak-driven gain, and as the whole mechanism when true-peak
  mode is off.

**The monitoring layer.**

- **F. Apply the compensation to the render — a real output trim.** Makes the compared signal and
  the delivered signal the same thing. **Lost:** it contradicts invariant 10 as written, and it has
  no safe position: downstream of the clamp it breaks invariant 4, upstream of it the ceiling's
  workload becomes a function of a monitoring toggle. A bounce would also depend on which listening
  aid was engaged when it was started.
- **G. Continuous adaptive AGC on the monitor path.** Always matched, no measurement lag.
  **Lost:** invariant 10 explicitly forbids the compensation becoming a continuous AGC; the sibling
  rejected the same option for the same reason (Anamorph ADR-0007, option A — "not transparent;
  constantly moving").
- **H. Measured loudness only.** Ground truth, no prediction machinery. **Lost:** it lags the
  gesture and drifts on silence — Anamorph's recorded failure mode. On a maximizer the lag has a
  direction: while the measurement catches up, the wet is audibly louder, which is the precise
  artefact the compensation exists to remove, so the A/B is unfair in the wet's favour exactly
  during the seconds a user forms an opinion.
- **I. Measure + absolute Predict, adapted from Anamorph ADR-0007 with the polarity inverted,
  floor-only predict, silence-freeze referenced to the BS.1770 −70 LUFS absolute gate.**
  **Chosen.**
- **J. As I, but copy the sibling's −60 dBFS mean-square freeze threshold verbatim.** Zero
  adaptation risk; the code is proven in a shipped product. **Lost:** the P0 research pass measured
  the consequence for this programme material — quiet passages sit below that threshold, so the
  compensation would freeze on wide-dynamic-range material, which is the material a mastering plugin
  is used on. The BS.1770 gate is also already implemented for the integrated meter (§2.9), so the
  adapted version reuses a mechanism rather than adding one.

## Decision

1. **The ceiling clamp is a structurally separate stage** (`CeilingClamp.h`), never folded into the
   limiter and never sharing its gain path. It is **always the last stage before dither**, in both
   EQ positions, downstream of the Post-position EQ (ADR-0002). It runs at **base rate**, outside
   the oversampled region (§3.1), because it must be downstream of a base-rate EQ.

2. **The clamp carries its own true-peak estimator tap on its own input.** The limiter's detector
   observed a different signal point once Post-EQ can sit between them, so the clamp does not reuse
   the limiter's true-peak envelope. Both instances come from the shared BS.1770-4 estimator
   (`TruePeak.h`, §3.2) and both are measurement taps — the clamp never resamples the audio it
   passes.

3. **Decision rule.** With `truePeakMode` on, the clamp's gain acts on its own TP estimate, with a
   sample-level hard clip as the backstop; with it off, on the sample peak. Tolerance **≤ 0.1 dBTP**
   (invariant 4). The gain carries the accuracy, the clip carries the absoluteness: the backstop
   alone cannot meet a dBTP tolerance at base rate, and the estimate alone cannot promise "never".

4. **The clamp is a safety stage, not a quality stage.** Driving it hard — for instance boosting the
   Post-position EQ into it after the limiter has already reached the ceiling — trades fidelity for
   the guarantee. That is the correct priority for a safety clamp and is accepted, not mitigated.

5. **One ceiling value, two consumers.** The clamp's threshold is `ceiling` (row 6), the same
   parameter that is the limiter's threshold (§2.5). There is no independent limiter threshold, so
   "how loud can it get" stays a one-knob question and the clamp can never be set below the level
   the limiter is aiming at.

6. **Loudness compensation and delta are monitoring-layer only** (invariant 10). The render is the
   chain output through clamp and dither; the monitoring layer is a separate stage downstream of it
   whose only inputs are that render, the delay-aligned dry ring (§1.2) and the compensation gain.
   Engaging either toggle changes **no** value inside the chain — no parameter, no gain stage, no
   clamp decision — and with both off the monitoring stage is bit-exact identity.

7. **Compensation mechanism** (adapted from Anamorph ADR-0007, polarity inverted — the wet is
   essentially always louder, so the compensation is essentially always attenuation):
   - **Measure** — K-weighted short-term loudness of the delay-aligned dry against the wet;
     `compGainDb = LUFS(dry) − LUFS(wet)`. K-weighting coefficients are the published ones reused
     from the sibling (§2.9). On silence the measurement **freezes** (holds the last trusted value)
     rather than drifting, with the silence gate referenced to the **BS.1770 −70 LUFS absolute
     gate** — the same gate the integrated meter already applies — and explicitly **not** the
     sibling's −60 dBFS mean-square threshold.
   - **Predict** — an absolute, stateless feed-forward estimate from the deterministic gain lift
     (input gain + limiter gain − expected GR), **floor-only**: it only ever lowers the monitor
     gain. Cranking the macro therefore pre-ducks the monitor immediately, without ratchet.

8. **Loudness-matched bypass and delta.** Bypass's crossfade target is the dry ring scaled by the
   same compensation, so a bypass comparison is loudness-matched by construction. Delta is
   (delay-aligned dry − wet) on the monitor path with its own crossfade. All three toggles are
   always-running output crossfades (~10 ms, bit-exact at the endpoints, §2.8), not ducks.

9. **Guards.** `testOutputNeverExceedsCeiling` runs its hostile-input sweep across **both** EQ
   positions, the Post case including a full +12 dB shelf boost (invariant 4).
   `testLoudnessCompensationDoesNotAlterRender` guards item 6 (invariant 10). The ≤ 0.1 dB true-peak
   accuracy test (invariant 11) must cover the **clamp's** tap as well as the limiter's, because
   their input paths differ. The `loudnessComp`, `deltaMonitor` and `bypass` crossfades each get a
   click-free path test (invariant 8).

10. **No policy amendment.** Invariants 4 and 10 are adopted as written; ADR-0002 already carried
    the invariant 1 wording change that makes invariant 4 satisfiable. This ADR adds guards and
    structure beneath both, and amends nothing.

## Consequences

- **The product promise becomes enforceable at the sample**, independent of limiter voicing, EQ
  position, oversampling factor, automation rate and transition state. `testOutputNeverExceedsCeiling`
  gains something real to test: a stage that is supposed to be inaudible in normal use and absolute
  in abnormal use.
- **Limiter voicing is decoupled from the guarantee.** `limStyle`, `limRelease`,
  `transientPreserve` and `stereoLink` can be re-tuned at P4 by ear without re-entering the
  Architecture Review Gate, because none of them is load-bearing for invariant 4.
- **A second true-peak estimator instance exists** (limiter detector + clamp tap). Its cost is
  charged against §9's ≤ 1.5% limiter + TP-detection allocation, and the accuracy test matrix grows
  by one input path.
- **Cost — fidelity under abuse, by design.** Nothing softens the clamp when it is driven; the
  ceiling wins over the sound. The base-rate hard-clip backstop is itself a nonlinearity outside the
  oversampled region, which is acceptable only because the TP-driven gain is what normally keeps it
  from engaging.
- **Deliberate inversion of the sibling product.** Anamorph's ADR-0009 (*not* this repository's
  ADR-0009, which is the code-reuse decision) records "no output clipper anywhere". Any DSP source
  adapted from Anamorph must not carry that assumption across.
- **The ceiling guarantee is a property of the render, not of what is audible.** The delta signal
  (dry − wet) and a compensated monitor gain are outside the clamp's scope by construction — delta
  is a difference signal and is not subject to invariant 4 — which is only sound because item 6
  makes the render measurable independently of the monitoring stage.
- **The compensation never gets louder**, so a user cannot use it to make the wet flattering; the
  floor-only predict means the monitor ducks ahead of the measurement rather than catching up to it.
- **The −70 LUFS-referenced gate keeps compensation alive on quiet programme** that the sibling's
  threshold would have frozen, at the cost of measuring K-weighted loudness on very quiet passages
  where the estimate is noisier — the deliberate trade for mastering material.
- **A/B loudness-matched comparison works out of the box** (§7): because compensation is
  monitoring-layer, per-slot compensation memory restores at the duck bottom and no slot's stored
  parameters carry a monitor gain.
- **Forecloses:** folding the clamp into the limiter; any stage after the clamp other than dither;
  a limiter parameter that can weaken the ceiling; an independent limiter threshold below the
  ceiling (adding one later is a `kVersion` bump, §2.5); applying the compensation to the render
  (would require amending invariant 10); a continuously-adapting AGC on either path.
- **Doc-sync obligation:** registration in `ADR_INDEX.md` (`ADR_POLICY.md` rule 1) and
  `procedures/TESTING.md` entries for `testOutputNeverExceedsCeiling` (both-positions stimulus, +12
  dB Post shelf) and `testLoudnessCompensationDoesNotAlterRender`, per
  `DOCUMENTATION_LIFECYCLE_POLICY.md`.

## Related code

None yet — P1 onward. Planned: `src/dsp/CeilingClamp.h` (the final clamp, invariant 4),
`src/dsp/TruePeak.h` (shared BS.1770-4 ≥ 4× estimator; the clamp instantiates its own tap),
`src/dsp/LoudnessComp.{h,cpp}` (Measure+Predict monitoring compensation),
`src/dsp/LoudnessMeter.{h,cpp}` (K-weighting, gated LUFS, the −70 LUFS gate),
`src/dsp/AnabasisEngine.{h,cpp}` (chain owner, dry ring, monitoring stage and its crossfades),
`src/dsp/LookaheadLimiter.{h,cpp}`, `src/dsp/Dither.h`, `src/dsp/EngineParameters.h` (POD snapshot
carrying `ceiling`, `truePeakMode`, `loudnessComp`, `deltaMonitor`),
`src/PluginParameters.{h,cpp}` (rows 6, 8, 9, 33).

Evidence [Unverified] — Anabasis has no `src/`, so every runtime claim above is the contract the
code must satisfy, not a measurement (constraint C2):

- Design: `docs/DESIGN.md` §2.6 (separate clamp, always last before dither, own TP tap, ≤ 0.1 dBTP,
  safety-not-quality, inversion of Anamorph's no-clipper decision), §2.7 (Measure+Predict with
  inverted polarity, floor-only predict, −70 LUFS-referenced silence gate, monitoring-only
  application, loudness-matched bypass, delta)
- Design (supporting): `docs/DESIGN.md` §1.2 (monitoring taps, dry ring, monitoring layer never in
  the render path), §2.5 (the ceiling *is* the limiter threshold), §2.8 (~10 ms output crossfades,
  bit-exact endpoints), §2.9 (K-weighting reuse, −70 LUFS absolute gate, TP meter), §3.1 (clamp at
  base rate, outside the OS region), §3.2 (true peak as a measurement tap), §4.2 rows 6/8/9/33 and
  §4.2 rows 28–32, §7 (per-slot compensation memory), §10 (this ADR's scope), §11
- Research: `worklogs/2026-07-30-p0-anamorph-research.md` — the −60 dBFS mean-square freeze
  threshold flagged as too high for mastering material, with the recommendation to gate against
  BS.1770 −70 LUFS instead
- Precedent [Verified]: `Anamorph:src/dsp/LoudnessMatch.cpp:126-185` — the Measure+Predict structure
  adapted here
- Precedent [Verified]: `Anamorph:src/dsp/LoudnessMatch.cpp:16-46` — published K-weighting
  coefficients, reused (§2.9)
- Precedent [Verified]: `Anamorph:src/dsp/LoudnessMatch.cpp:127-128`,
  `Anamorph:src/dsp/AnamorphEngine.cpp:1159` — the −60 dBFS mean-square freeze threshold this
  design deliberately does not copy
- Precedent [Verified]: `Anamorph:src/dsp/AnamorphEngine.cpp:777-846,1302-1327` — always-running
  chain with an output crossfade for bypass, the structure the monitoring stage plugs into
- Precedent [Verified]:
  `Anamorph:docs/architecture/design-decisions/ADR-0007-levelmatch-measure-predict.md` — Measure +
  absolute Predict, the rejected AGC and measured-only options, silence-hold
- Precedent [Verified]:
  `Anamorph:docs/architecture/design-decisions/ADR-0009-nan-selfheal-nyquist-clamp.md` — "no output
  clipper", the decision Anabasis deliberately inverts
- Depends on: this repository's ADR-0002 (clamp placement, invariant 1 amendment) and ADR-0003
  (true peak as a measurement tap, ≥ 4× at every OS setting)
