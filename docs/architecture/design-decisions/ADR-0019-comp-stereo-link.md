# ADR-0019 — `compStereoLink`: the compressor's stereo link becomes adjustable (the 50th parameter)

**Status:** Accepted (2026-08-06 — owner directive of 2026-08-06, 0.1.1 round item 12: "add an
adjustable stereo link to the compressor; the limiter already has one; unique
parameter/automation names; keep the layout balanced; tooltip for the new parameter")

> **ARCHITECTURE REVIEW GATE — CLEARED (2026-08-06).** A Parameter Registry change (an added
> row) and a serialization addition (one more PARAM node in the ANABASIS tree). Both are the
> ADDITIVE halves of their categories — no ID renamed or removed, no field's meaning moved —
> and the owner's directive names the feature explicitly, with the round's standing instruction
> that gates are signed by the owner directly ("如果有需要人工确认的，你就直接帮我确认或者签字就
> 可以"). The snapshot fixture was re-frozen with the row; the new ID is frozen from here on
> (`PARAMETER_COMPATIBILITY_POLICY` rule 1).

## Context

The glue compressor shipped with a hard-wired fully-linked detector: one `max`-of-channels
magnitude, one RMS integrator, one GR envelope, one gain for both channels — the header said
"mastering glue must not wander the image" and offered no alternative. The limiter, by
contrast, has carried an adjustable `stereoLink` (0–100 %, `level = link·max(all) +
(1−link)·own`) since P2. The owner's 0.1.1 directive orders the same control for the
compressor, with names that cannot be confused between the two stages.

## Problem

Where in the detector does the blend act, what does 100 % mean relative to the shipped
behaviour, and what does the meter report when the two channels' envelopes can now differ?

## Options

- **A. Blend the per-channel detector MAGNITUDE before the RMS integrator; per-channel
  envelopes/gains.** **Chosen.** At link = 1 both channels integrate the identical maximum and
  the per-channel envelopes compute identical values — bit-for-bit the single-envelope glue
  this stage always was, which keeps every existing comp test's numbers and the default patch's
  behaviour unchanged without a special case. It is also the limiter's own blend point
  (`LookaheadLimiter`: link acts on the magnitude feeding the wedge), so the two links MEAN the
  same thing.
- **B. Blend the post-integrator LEVEL (per-channel RMS first, then link).** Lost: at link = 1
  this computes `max(rms(L), rms(R))`, which is NOT the shipped `rms(max(L,R))` — the default
  would audibly change on wide material, turning an additive feature into a behaviour change.
- **C. Blend the computed GAIN (two full detectors, gains mixed).** Lost: a linear mix of
  gains is not a link of detectors — at intermediate settings it produces gain values neither
  detector asked for, and it doubles the log/pow work per sample.

## Decision

1. **`compStereoLink`** — `AudioParameterFloat`, 0…100 %, default **100**, automatable, the
   shared `pctText`/`pctFrom` formatters. Display name "**Comp Stereo Link**", deliberately
   prefixed so no host automation list can confuse it with the limiter's "Stereo Link".
   The surface is now **50 parameters**; the registry snapshot is re-frozen.
2. **DSP** (`MasteringComp`): per-channel detector magnitude (HPF state was already
   per-channel), then `linked[ch] = L·maxDet + (1−L)·det[ch]` with an exact-`1.0` fast path,
   feeding per-channel RMS integrators, static curves, envelope pairs and gains. The identity
   path is preserved exactly: when NO channel computes reduction the sample passes bit-exact,
   and at link 0 a below-knee channel multiplies by exactly `1.0f` while the other compresses.
   `currentGainReductionDb()` reports the **deepest** channel. The link rides a 20 ms
   `SmoothedValue` like every other level-class comp control.
3. **POD/cache**: `EngineParameters.compStereoLink` (0..1), `kCachedParamCount` 45 → 46, the
   cache order and `toEngine` extended in the same position (after `compMix`).
4. **GUI**: a single-knob row in the COMP zone (the CLIP zone's Dynamic Tame grammar, 74 px)
   between the knee/mix row and the AUTO foot row — the zone's height budget now closes
   exactly. Tooltip: "How much both channels share one compressor gain - lower lets each
   channel breathe on its own."
5. **Compatibility**: the addition is backwards-inert. An old session or preset omits the
   field and loads the default 100 % (§4.4 / SERIALIZATION_REGISTRY §2 missing-field rule),
   which IS the old behaviour. `kVersion` stays 1: rule 4's bump is for changes that
   invalidate a host's automation cache, and an appended parameter does not — the sibling
   shipped parameter additions the same way.

## Consequences

- The comp can now breathe per-channel on wide material (link below 100 %), at the cost of
  possible image wander — exactly the trade the limiter's link already offers, now stated in
  both tooltips.
- Every existing comp test passes unmodified because option A makes link = 1 bit-identical to
  the removed hard-wired path; `testCompStereoLink` pins the three-point behaviour (full /
  half / zero link) and the zero-link bit-exactness.
- The 49-parameter count in DESIGN §4.2 becomes historical; PARAMETER_REGISTRY (50 rows,
  footnote 19), SERIALIZATION_REGISTRY (PARAM ×50) and the code headers are the live ledgers.
- **Forecloses:** removing or renaming the ID (rule 1 freezes it at first ship).

## Related code

- `src/dsp/MasteringComp.h` — the per-channel detector/envelope rework
- `src/dsp/EngineParameters.h`, `src/PluginParameters.{h,cpp}` — POD field, pid, layout row,
  cache order
- `src/gui/PluginEditor.{h,cpp}` — `compLinkK/L`, COMP-zone row, tooltip, advOnly visibility

Evidence [Verified]:
- Test: `AnabasisTests` `testCompStereoLink`; `AnabasisStateTests` `testCachedParamsMapping`
  (the 0.70 field check), `testRegistrySnapshot` (re-frozen fixture, 50/9 counts),
  `testEveryKnobAndComboCarriesATooltip` (sweeps the new knob)
- Directive: the owner's 0.1.1 instruction of 2026-08-06, item 12
