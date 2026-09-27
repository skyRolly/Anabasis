# ADR-0044 — MATCH applies to the processed leg, so BYPASS plays the input at unity and the comparison is loudness-matched

**Status:** **Accepted — 2026-09-27, on the owner's direction** (the Phase 1 instruction of record:
fix MATCH's signal ordering, prove the signal-flow semantics first, and do not alter the delivered
processed signal), with the signal flow proved and measured before the code moved. ⊕ Flagged for the
owner's review of the pull request that carries it, like every decision taken on a direction rather
than on a reviewed record: the direction settled *that* a BYPASS comparison with MATCH on must be
loudness-matched and that the processed signal must not change; the design below — where the gain
goes, and what DELTA + MATCH plays — is what that review confirms. It is an Architecture Review Gate
item on two counts, named here rather than left to be found: a **DSP signal-order change** on the
monitor stage (`DSP_POLICY.md` Enforcement, `AI_AGENT_POLICY.md`), and a **conflict with Accepted
ADR-0006 decision 8**, whose mechanism sentence it amends (dated banner there). It also rewords
`DSP_POLICY.md` invariants 7, 10 and 12 (prescribed text, decision 6). Audit finding **UX-009**.

## Context

ADR-0006 decision 8: *"Bypass's crossfade target is the dry ring scaled by the same compensation, so a
bypass comparison is loudness-matched by construction."* The code did what the first half says
(`AnabasisEngine.cpp`, stage E: the bypass crossfade into `out`, then `out *= monGainNow`), and the
second half does not follow from it: scaling **both** legs by the MATCH gain g keeps their ratio, so
the comparison keeps the whole level difference MATCH exists to remove. Measured on the real engine
before any change (Loudness 70 % point, Kellet pink, decorrelated channels at −17 dBFS RMS, dry
short-term −14.3 LUFS — the audit's calibration), switching BYPASS on with MATCH on:

| | MATCH off | MATCH on, 0.2.13 | MATCH on, this record |
|---|---|---|---|
| BYPASS − matched processed, short-term (`testMatchedBypassIsLoudnessMatched`) | −6.76 LU | **−6.76 LU** | **+0.63 LU** |
| BYPASS − matched processed, momentary (probe) | −6.34 LU | −6.30 LU | +0.82 LU |
| settled BYPASS vs the input | bit-exact | **−7.33 dB, not bit-exact** | **bit-exact** |

MATCH + BYPASS played the input 5–10 dB below itself (|g|, level-dependent), so the audit's "switch
BYPASS to hear the difference" produced the level jump MATCH was switched on to remove. The sibling
product applies its match gain upstream of its blend and blends to the raw delay-aligned input at
unity (Anamorph `AnamorphEngine.cpp`, read-only reference), so decision 8 was also the family
divergence, not the family behaviour.

## Problem

Where the §2.7 loudness-compensation gain is applied relative to the bypass crossfade and the delta
substitution, so that a BYPASS comparison with MATCH on is loudness-matched — without changing the
render, the meters, the latency, a parameter or the saved state.

## Options

- **A. The gain on the processed (wet) leg, after the delta substitution and before the bypass
  crossfade.** BYPASS plays the delay-aligned input at unity; the matched processed signal sits at the
  input's loudness; DELTA + MATCH stays g·(dry − processed) as today. One multiply moves; every other
  path is bit-identical. **Chosen.**
- **B. Keep the post-mix gain and divide the bypass leg by g.** Mathematically the same output as A
  away from rounding, with a division on the audio path and a bypass leg that is no longer the input
  sample — invariant 7's bit-exact null lost for nothing. Rejected.
- **C. Keep the order and document it.** Leaves the product's primary comparison tool reproducing the
  loudness bias it exists to remove, and `DEVELOPMENT_BRIEF.md`'s "loudness-matched bypass"
  requirement unmet. Rejected.
- **D. Raise the bypass leg to the processed loudness instead** (BYPASS louder than the input). Makes
  BYPASS something other than the input and inverts ADR-0006's "the compensation never gets louder".
  Rejected.
- **E. The gain on the processed leg BEFORE the delta substitution** (DELTA + MATCH = dry − g·processed).
  Changes what DELTA plays with MATCH on — a behaviour change nobody asked for, and a difference signal
  that no longer nulls at the matched level. Rejected; DELTA + MATCH keeps its 0.2.13 definition.

## Decision

1. **The §2.7 gain g multiplies the processed leg after the delta substitution and before the bypass
   crossfade**, with the exact skip at unity kept (MATCH off, and always offline, where the engine
   snaps g to 1). The post-mix multiply is removed.
2. **Settled BYPASS is the bit-exact delay-aligned input in every monitor state** — MATCH, DELTA, both
   or neither. Invariant 7's bypass null no longer needs a monitor carve-out.
3. **The matched processed signal is what MATCH scales.** With MATCH on, BYPASS against the processed
   signal is the loudness-matched comparison; the ~10 ms bypass crossfade runs between g·processed and
   the input, both near the input's loudness.
4. **DELTA + MATCH plays g·(dry − processed)** — unchanged from 0.2.13. DELTA alone, and DELTA under
   BYPASS, are unchanged.
5. **Nothing else moves:** the render tap and every meter (they read the bypass-mixed programme with no
   delta and no monitor gain), the render itself (g ≡ 1 offline), the latency, the dither and clamp
   stages, the parameters, the saved state, the A/B/undo/preset tiers (MATCH, DELTA and BYPASS are view
   tier), and the measure/predict law of ADR-0006 decision 7.
6. **Policy amendment (prescribed text, `ADR_POLICY.md` rule 5).**
   - `DSP_POLICY.md` invariant 7's scope paragraph: the Loudness Comp carve-out ("scales the bypass leg
     too, by design: the monitor gain is applied POST-mix …") is replaced by *"Since ADR-0044
     (2026-09-27) the §2.7 monitor gain is applied to the processed leg before the bypass crossfade,
     so settled bypass is a bit-exact null in every monitor state — MATCH, DELTA or both — and the
     loudness-matched comparison comes from the processed leg meeting the input's loudness, not from
     scaling the input."*
   - Invariant 10 names where the gain is applied: *"a measurement-driven gain applied to the
     monitoring path's processed leg, before the bypass crossfade (ADR-0044), so BYPASS plays the
     input at unity"*.
   - Invariant 12's first downstream-leg bullet reads *"the §2.7 loudness-compensation gain, applied
     to the processed leg before the bypass crossfade (ADR-0044)"* in place of "applied post-mix so a
     loudness-matched bypass carries the same gain"; its third bullet's "§2.9 delta substitution" is
     corrected to §2.7 (drift).

## Consequences

- **A user's MATCH + BYPASS level rises by |g|** — 5–10 dB at typical settings — to the input's own
  level. It is never louder than a plain BYPASS; release-noted.
- **Toggling MATCH while bypassed is inaudible**, by design (BYPASS is the input). The bypass dim
  already covers the MATCH and DELTA pills in that state.
- **A realtime print that automates BYPASS with MATCH on now prints unity input** in the bypassed
  sections instead of an attenuated one. Offline renders are unaffected (MATCH is inert there).
- **What is left of the gap is MATCH's own estimator bias** (audit DSP-005): the predict floor counts
  only the limiter's reduction, and `min(measure, predict)` keeps the too-deep floor, so the matched
  processed signal settles slightly below the input — +0.63 LU short-term at the calibration above,
  up to +1.7 LU on hotter programme. Measured this round: at that point the **clipper's** loss is the
  larger term (clip drive 0 → +0.30 LU; removing the compressor changes nothing there), which
  contradicts the audit's recommendation to drop the clipper term. Recorded as `KNOWN_ISSUES.md`
  KI-023 and sequenced as the next MATCH item; not addressed here.
- **Drift recorded, not addressed:** ADR-0006's Consequences and DESIGN §7 promise "per-slot
  compensation memory restores at the duck bottom"; no per-slot compensation state exists (the MATCH
  measure and gain are single engine members). The dated banner on ADR-0006 says so.
- **Forecloses** re-applying the monitor gain after the bypass crossfade, and scaling the bypass leg by
  any monitor function, without superseding this record.

## Related code

- `src/dsp/AnabasisEngine.cpp` (stage E: the gain on `wetLeg` before the bypass crossfade), `src/dsp/AnabasisEngine.h` (the §2.7 member comment)
- `tests/dsp_tests.cpp`: `testMatchLeavesBypassAtUnity`, `testMatchedBypassIsLoudnessMatched`,
  `testMonitorTogglesAreClickFree`, the new offline block of `testLoudnessCompensationDoesNotAlterRender`
- `tests/state_tests.cpp`: `testBypassPlaysTheInputAtUnityWithMatchOn`

## Evidence

Confidence: **Verified** at engine and processor level; **not heard** (no listening was performed);
the realtime path was not exercised in a DAW (see the worklog's host section).

- The regression tests fail on 0.2.13's order and pass on this one: `matchBypass` ×2, `matchJump`
  (−6.76 LU → +0.63 LU) and `matchWrapper`; removing MATCH altogether fails the existing inv-10 checks
  and every premise; each of the three toggle ramps removed fails its own `monitorClick` case.
- Old engine vs new, sample by sample, on pink and on a kick-and-pink bed (32 s each, BYPASS switched at
  10 s): MATCH off, DELTA alone and offline MATCH + DELTA across a BYPASS toggle identical over every
  sample; MATCH on and MATCH + DELTA identical up to the first bypassed block and different after it;
  the render-tap short-term loudness identical per block in every run —
  `worklogs/2026-09-27-phase1-match-statistics-observability.md`.
- Audit finding UX-009 (`docs/reports/2026-09-26-anabasis-product-ux-audit/findings-ux.md`); DSP-005
  (`findings-dsp-tech.md`).
- Depends on: ADR-0006 (decisions 6–9, amended here at 8), ADR-0003 (the render tap is the measurement
  tap).
