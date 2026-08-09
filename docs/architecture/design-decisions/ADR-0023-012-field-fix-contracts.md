# ADR-0023 — The 0.1.2 field-fix contracts: no reduction below the engagement level, an unfiltered limiter detector, three bus layouts, and the fixed-scale GR history

**Status:** Accepted — gate cleared 2026-08-09 (the owner's 0.1.2 release directive, which
names each behaviour below as a required fix and instructs: *"Do not apply superficial fixes or
work around symptoms"*, *"If a DSP component such as SC HPF is responsible, implement the
correct design solution to prevent unintended GR while preserving intended functionality"*,
*"The plugin should support a mono-to-mono I/O layout … Use the existing plugin with different
supported bus layouts"*, *"GR history display must always use a fixed scale"*, and — for the
switch, the names and the Advanced layout — *"If a better UI solution is identified, directly
apply it"*.)

## Context

The owner's 0.1.2 field report described gain reduction under the Default preset on stereo
material that never exceeds 0 dBFS (item 2), a GR history display that zooms while it fills and
jumps backward across pause/resume (items 3/6), a mode switch whose active segment swallowed
clicks (items 4/5), and a dual-mono-shaped hole in the bus contract (item 13, following the
KI-009 lineage). Investigation confirmed three real gain-reduction mechanisms at factory
defaults, all applying real gain, none a metering artifact:

1. The compressor's 6 dB soft knee was **centred** on the threshold (`MasteringComp.h`), so its
   lower half computed gain from `T − W/2` up — at the 0 dBFS default threshold, from −3 dBFS
   (detector level) up. The stage's own header conceded the all-defaults null held only "below
   the knee bottom".
2. The shared `scHpfFreq` sidechain HPF also fed the **limiter's** detector (brief §3), and the
   §5.4 adaptive scHpf trim engages that filter **at factory defaults** (its target saturates at
   the +30 Hz bound on real masters). A 2nd-order high-pass is unity-magnitude only in steady
   state: its transient response overshoots an LF edge by up to ~6 dB (≈ `b0·2A` on a −A→+A
   step), so the limiter drew reduction on samples that never crossed the ceiling — and in the
   other direction the same filter under-read bass overs, which then reached the unconditional
   `CeilingClamp` as hard clips, the failure the engine's own comments call the one a maximizer
   can least afford.
3. The limiter's threshold **is** the Ceiling (ADR-0002/0006), so peaks in (−0.1, 0] dBFS draw
   up to 0.1 dB of by-definition reduction at the −0.1 dB default. The owner's directive accepts
   this branch explicitly ("If Ceiling defaults to −0.1 dB, behavior should follow that
   definition consistently"); it is documented here, not changed.

## Problem

Which behaviours are the *contracts* and which are the defects — and the minimal set of
contract-level changes that makes "no GR when the signal is below every engagement level" true
by construction rather than by stimulus luck.

## Options

- **A. Reduce sensitivity / bias the meters** — hides the symptom, directive forbids it.
- **B. Move defaults** (threshold above 0, knee 0, ceiling 0.0) — the threshold range ends at
  0 dBFS (a range change is a compatibility break), knee 0 forfeits the §2.3 glue knee at every
  loudness, ceiling 0.0 forfeits the safety margin. All rejected.
- **C. Contract-level fixes**: knee above the threshold; the limiter detector unfiltered; the
  comp's filtered detector magnitude clamped to a raw-magnitude ceiling. Chosen.

## Decision

1. **Static curve (comp):** zero gain at or below the threshold; the knee softens the onset
   over `[T, T+W]` (quadratic, C1 at both ends; `W → 0` degenerates to the hard-knee line at
   `T` exactly). Where the old centred knee put −1.125 dB at `T` (12 dB knee, 4:1), the new
   curve puts 0 at `T` and −1.125 dB at `T + W/2`.
2. **Limiter detector:** unfiltered — magnitude (or true-peak estimate) of the tapped sample.
   `LookaheadLimiter` loses `setDetectorHpf`, its biquad state and `sanitiseDetectorState`
   (nothing recursive remains to repair). The shared `scHpfFreq` becomes **comp-only**, and the
   §5.4 scHpf trim therefore reaches the comp's detector only.
3. **Comp detector overshoot ceiling:** with the HPF engaged, the filtered magnitude is clamped
   against a **peak envelope of the raw magnitude** (instantaneous attack, 500 ms release) —
   `det = min(|filtered|, ceilEnv(|raw|))`. A sidechain high-pass may only *deafen* the detector,
   never sharpen it; overshoot above the raw envelope is filter ringing, not programme.

   > **Amended 2026-08-09, same round, before any tag.** This item first specified the pointwise
   > form `det = min(|filtered|, |raw|)`, and the round's own review found it defective: the two
   > operands are time-domain samples the filter has put out of phase, so a bass-dominated `|raw|`
   > passing through zero twice per cycle dragged the detector to ~0 at the BASS rate — restoring
   > the low-frequency coupling `scHpfFreq` exists to remove, and gating whatever passband content
   > the detector should have seen at those instants. It also bound on pure passband content
   > (~43° of filter phase at 2·fc reads ~0.4 dB under the unity passband, and lowers the
   > integrated RMS further). **The contract is unchanged** — "may only deafen" is a statement
   > about LEVELS, and the defect was enforcing it against an instantaneous sample instead of a
   > level. Measured on a 30 Hz fundamental under 3 kHz programme with the HPF at 300 Hz: peak-to-
   > peak reduction ripple 1.295 dB unfiltered · **0.291 dB pointwise** · **0.0026 dB** with the
   > envelope ceiling. Pinned by `testLimiterDetectorIsUnfiltered`, whose overshoot half is
   > unchanged and still passes against either form — the two properties are asserted separately.

4. **Ceiling-definition GR stands:** peaks above the Ceiling draw reduction — that is the
   ADR-0002/0006 design, unchanged, now the *only* reduction reachable at defaults.
5. **Bus layouts:** `stereo→stereo`, `mono→stereo` (KI-009, 0.1.1) and — new — `mono→mono`,
   the same binary at `nCh = 1` (dual-mono/multi-mono racks). `stereo→mono` stays refused (no
   downmix rule is defined). Latency, parameters and serialization are channel-count-free.

   **METERING IS NOT, and that is intended** (reviewed and accepted 2026-08-09). `LoudnessMeter`
   implements BS.1770 as specified — it SUMS the weighted channel energies rather than averaging
   them — so a one-channel instance measures the same programme about **3 LU quieter** than the
   dual-mono stereo layout. That is the standard's reading for a mono presentation, not a defect,
   and it is why no channel-count normalisation is applied: adding one would make Anabasis
   disagree with every other BS.1770 meter on the same material. Consequences, stated so a field
   report is not mistaken for a bug — the LUFS rows (M/S/I) and LRA read ~3 LU lower in
   mono→mono; PLR keeps its character (both terms move together); and the §2.7 measured
   compensation is a dry-vs-wet DIFFERENCE, so the offset cancels there. `RmsMeter`, the §5.4
   features and the true-peak estimate all normalise by `nCh` or are per-channel, so they are
   layout-invariant.
6. **GR history:** fixed right-anchored scale (pitch = width over one full window's bucket
   count), the unmeasured region drawn as zero data, and the ring cleared at `prepareToPlay`
   only when the (rate, block) pair actually changed — a transport-start re-prepare keeps the
   timeline.
7. **Graph-well switch:** the GR|SPEC pill moves to the bottom-left (the least informative
   corner in both modes; the old top-right sat on the newest GR data), GR is the left segment
   and the default mode (`int_spectrumOn` default flips to `false` — a default change only,
   not a semantic or domain change; stored sessions are honoured as before), and the whole pill
   is one toggle — no press on it is a no-op.
8. **Names:** editor captions drop the stage prefix inside captioned panels (both links read
   "Stereo Link", the limiter's gain/release read "Gain"/"Release"); automation names keep the
   prefix, and the limiter's registry NAME becomes "Limiter Stereo Link" (ID `stereoLink`
   unchanged; snapshot re-frozen per PARAMETER_COMPATIBILITY_POLICY rule 2). Accessibility
   titles keep the registry names (brief §8's wording contract).
9. **Advanced layout:** the read-only macro row is removed; `kAdvancedH` = 822 (was 900 — the
   family-number coincidence is recorded as lost in the header comment). The macro layer itself
   is untouched: mapping, detach, re-engage and the badges all live below the view layer, and
   `testModeSwitchIsSoundNeutral` still pins the sound contract.
10. **Per-channel per-stage GR publication** (item 12): `compGrDbCh[2]` / `limGrDbCh[2]` beside
    the combined figures (which the history ring, `pubGrDb` and the §2.7 predict floor keep
    reading); the panel meters draw L/R lanes. Beyond the display, this is the KI-009 field
    disambiguator — one channel silent with its lane pinned deep points at per-channel gain
    collapse; silent at zero GR points away from the dynamics stages.

## Consequences

- The all-defaults null now holds **for any sub-ceiling input** — `testNullWithDefaults` drives
  a 30 Hz square at −0.6 dBFS with the adaptive trims live and asserts bit-exactness, zero comp
  GR, zero limiter GR, and that **Delta at defaults is exact digital silence** (the owner's
  item-2 observation, pinned out by construction).
- Compression onset at any given threshold/knee arrives `W/2` dB later than the centred-knee
  curve; the ⊕ listening pass judges the macro loudness ramp with this curve in place.
- A bass-heavy over is now *limited* rather than clamp-clipped whatever the SC HPF setting —
  the recorded "hard clipping instead of limiting" trade is gone; users who raised SC HPF
  expecting the limiter to ignore bass no longer get that (deliberately: the limiter's job is
  the ceiling; the comp keeps the bass-deafening role).
- The deviation from brief §3's "shared" HPF wording and from `DESIGN.md` §2.3's centred knee is
  governed by this ADR (an ADR outranks both).
- `sanitiseDetectorState` and the limiter-HPF tests (engage, stale-state re-entry, glide) left
  with the filter; the new tests pin the unfiltered detector and the comp clamp, each verified
  against its own mutant.
- The Advanced window is 940×822; Settings/About/save overlays centre via
  `withSizeKeepingCentre` in `resized()` and follow any size by construction.

## Related code

- `src/dsp/MasteringComp.h` (static curve; detector clamp)
- `src/dsp/LookaheadLimiter.h` (unfiltered detector; removed HPF machinery)
- `src/dsp/AnabasisEngine.{h,cpp}` (per-channel GR publication; ditherErr sweep; scHpf trim scope)
- `src/PluginProcessor.{h,cpp}` (bus layouts; conditional GR-ring clear; per-channel getters)
- `src/gui/GrHistoryView.{h,cpp}` (fixed-scale geometry; zero region)
- `src/gui/LookAndFeel.h` `graph_switch` · `src/gui/SpectrumView.cpp` (pill, toggle)
- `src/gui/PluginEditor.{h,cpp}` (captions/a11y split; utility-row baseline; 822 layout; GR lanes)
- `src/gui/CurveView.h` `GrMiniMeter` (two lanes)
- `src/PluginParameters.cpp` ("Limiter Stereo Link")
- `src/InternalState.h` (`int_spectrumOn` default)

Evidence [Verified]:
- Source: the files above at 0.1.2
- Test: `testNullWithDefaults` (both passes), `testCompStaticCurve`,
  `testLimiterDetectorIsUnfiltered`, `testGrHistoryWindowNeverAsksForTheHeadSlot` (fixed-scale
  section), `testGrRingResetEpoch`, the graph-switch click tests, `testMeterPublication`
  (per-channel lanes), `testBothChannelsCarryAudioThroughTheWrapper` (mono→mono + the six
  KI-009 diagnostic configurations) — every new assertion verified to fail with its fix
  disabled (eight mutants across two control builds)
