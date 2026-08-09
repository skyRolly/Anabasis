#pragma once

#include "EngineParameters.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

// ============================================================================
//  MasteringComp — the §2.3 glue compressor.
//
//  Feed-forward, LOG-DOMAIN gain computer (level → dB → static curve →
//  ballistics on the gain-reduction signal → linear gain), the topology whose
//  behaviour is documented in the dynamics literature rather than improvised:
//  the GR signal is smoothed in dB, attack when reduction deepens, release
//  when it relaxes.
//
//  - Detector: stereo link ADJUSTABLE since ADR-0019 (0.1.1) — the limiter's
//    blend, applied at the same point: per-channel magnitude, then
//    linked = link·max(all) + (1−link)·own, BEFORE the RMS integrator, so at
//    full link every channel integrates the identical maximum and the
//    per-channel envelopes compute identical gains — bit-for-bit the single
//    shared-gain glue this stage always was (mastering glue must not wander
//    the image; 100 % stays the default). Per-channel sidechain HPF
//    (20–300 Hz, RBJ Butterworth, detector-side ONLY — the audio path never
//    passes through it). RMS mode squares through a fixed 10 ms window;
//    Peak mode uses the magnitude directly.
//  - Static curve: ZERO at or below the threshold; a soft knee of width W dB
//    ABOVE it (quadratic over [T, T+W], then the ratio line; W below a
//    millidB is treated as hard to keep the 1/2W term finite). The knee was
//    centred on T until 0.1.2 (ADR-0023): its lower half computed real gain
//    from T − W/2 up, so the 0 dBFS default threshold drew reduction on
//    legal material above −3 dBFS.
//  - Release: manual = one pole at `compRelease`. Auto = TWO parallel poles
//    (fast ≈ 80 ms, slow ≈ 900 ms), averaged in dB — the "two-pole
//    program-dependent release" of DESIGN §2.3: a short peak recovers mostly
//    through the fast pole, sustained reduction leaves both poles deep so the
//    tail is held by the slow one. The two constants are P6 listening-test
//    material; the SHAPE (fast initial, slow tail) is the contract the test
//    pins.
//  - Mix: parallel blend with exact endpoints — mix == 1 applies the wet
//    branch alone, and a unity-gain wet branch returns the input SAMPLE
//    UNTOUCHED (gr == 0 short-circuits), which is what keeps the
//    all-defaults null bit-exact (threshold 0 dBFS: nothing AT or below the
//    threshold ever computes a gain — since 0.1.2 for any legal level, not
//    only below the old centred knee's −3 dBFS bottom).
//
//  Runs at base rate (DESIGN §2.3: the gain signal is band-limited by the
//  5 ms minimum attack; the P2 aliasing measurement revisits this and moving
//  it inside the oversampled region would be an ADR-0003 amendment).
// ============================================================================

namespace anabasis
{

class MasteringComp
{
public:
    static constexpr int kMaxChannels = 2;

    MasteringComp() = default;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto* s : smoothers())
            s->reset (sampleRate, 0.020);
        aRms     = onePole (10.0f);        // fixed RMS integration window
        aCeilRel = onePole (kCeilingReleaseMs);
        aRelFast = onePole (kAutoFastMs);
        aRelSlow = onePole (kAutoSlowMs);
        reset();
    }

    void reset() noexcept
    {
        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            grFastDb[ch] = grSlowDb[ch] = 0.0f;
            meanSquare[ch] = 0.0f;
            ceilEnv[ch] = 0.0f;
            hpfZ1[ch] = hpfZ2[ch] = 0.0f;
        }
        primed = false;
    }

    // Invariant 9 recovery — the counterpart of LookaheadLimiter::resetWindow:
    // repair what is poisoned, CARRY what is not. The GR envelope is the state
    // whose snap to unity is audible (a block held 10 dB down would jump with
    // no release ramp — the same argument the limiter's own comment makes), so
    // it is only cleared when it is itself non-finite, which is the one case
    // where there is nothing to carry. `meanSquare` and the detector biquad go
    // independently: they poison on their own (the RMS path SQUARES, so ~1.8e19
    // is enough) without the envelope having seen anything.
    void sanitiseState() noexcept
    {
        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            if (! std::isfinite (grFastDb[ch]) || ! std::isfinite (grSlowDb[ch]))
                grFastDb[ch] = grSlowDb[ch] = 0.0f;  // paired: the auto path averages them
            if (! std::isfinite (meanSquare[ch]))
                meanSquare[ch] = 0.0f;
            // The overshoot ceiling: a non-finite value here would clamp the
            // detector to garbage rather than merely mis-level it, and it is
            // fed the raw input the engine keeps finite, so this is the same
            // defence-in-depth the states above get.
            if (! std::isfinite (ceilEnv[ch]))
                ceilEnv[ch] = 0.0f;
            if (! std::isfinite (hpfZ1[ch]) || ! std::isfinite (hpfZ2[ch]))
                hpfZ1[ch] = hpfZ2[ch] = 0.0f;
        }
    }

    // Once per block, from the POD snapshot.
    void setPerBlock (const EngineParameters& p) noexcept
    {
        auto set = [this] (juce::SmoothedValue<float>& s, float v) noexcept
        {
            if (primed) s.setTargetValue (v);
            else        s.setCurrentAndTargetValue (v);
        };
        set (thresholdDb, p.compThresholdDb);
        set (ratio,       juce::jlimit (1.01f, 40.0f, p.compRatio));
        set (kneeDb,      juce::jmax (0.0f, p.compKneeDb));
        set (mix,         juce::jlimit (0.0f, 1.0f, p.compMix));
        set (link,        juce::jlimit (0.0f, 1.0f, p.compStereoLink));
        set (hpfFreq,     juce::jlimit (20.0f, 300.0f, p.scHpfFreqHz));

        // Time constants and mode switches are per-block by the same rule as
        // the limiter's release: they set rates, not levels, so a boundary
        // change cannot step the output (the GR smoother is the smoother).
        aAtk        = onePole (juce::jmax (0.1f, p.compAttackMs));
        aRelManual  = onePole (juce::jmax (1.0f, p.compReleaseMs));
        autoRelease = p.compAutoRelease;
        rmsDetector = p.compDetector == 0;

        if (! primed)
        {
            primed = true;
            recomputeHpf();
        }
    }

    // One call per sample with all channel samples; applies the gain (and the
    // parallel mix) IN PLACE. Returns nothing — the GR meter tap is the
    // getter below.
    void processSample (float* chans, int numChannels) noexcept
    {
        const int nCh = juce::jmin (numChannels, kMaxChannels);

        // -- advance the smoothed controls ----------------------------------
        const float T = thresholdDb.getNextValue();
        const float R = ratio.getNextValue();
        const float W = kneeDb.getNextValue();
        const float M = mix.getNextValue();
        const float L = link.getNextValue();
        if (hpfFreq.isSmoothing())
        {
            hpfFreq.getNextValue();
            recomputeHpf();
        }

        // -- detector (sidechain only, never the audio path) ----------------
        // Per-channel magnitude, then the ADR-0019 stereo-link blend at the
        // limiter's point: linked = L·max(all) + (1−L)·own, BEFORE the RMS
        // integrator — at L == 1 both channels integrate the identical
        // maximum, which is bit-for-bit the pre-0.1.1 single-detector glue.
        float det[kMaxChannels] = {};
        float maxDet = 0.0f;
        for (int ch = 0; ch < nCh; ++ch)
        {
            const float x = chans[ch];
            float y = x;
            if (hpfOn)
            {
                y = hb0 * x + hpfZ1[ch];
                hpfZ1[ch] = hb1 * x - ha1 * y + hpfZ2[ch];
                hpfZ2[ch] = hb2 * x - ha2 * y;
            }
            const float rawMag = std::abs (x);

            // The raw-magnitude CEILING the overshoot guard below clamps
            // against: instantaneous attack, slow release (kCeilingReleaseMs).
            // Advanced unconditionally, not only while `hpfOn` — it is derived
            // from the input alone, and an envelope that only ran with the
            // filter would start from zero on the off→on edge and clamp the
            // detector to silence for its first samples, which is the class of
            // stale-state defect the detector states are otherwise swept for.
            ceilEnv[ch] = rawMag > ceilEnv[ch]
                              ? rawMag
                              : ceilEnv[ch] + (rawMag - ceilEnv[ch]) * aCeilRel;

            det[ch] = rawMag;
            if (hpfOn)
            {
                // The sidechain HPF may only DEAFEN the detector, never
                // sharpen it (0.1.2, ADR-0023): a 2nd-order high-pass is
                // unity-magnitude only in steady state — its transient
                // response on an LF edge overshoots the input by up to ~6 dB
                // (b0·2A on a −A→+A step), so an unclamped filtered magnitude
                // drew gain reduction on material whose samples never crossed
                // the curve's own engagement level.
                //
                // THE BOUND IS AN ENVELOPE, NOT THE INSTANTANEOUS SAMPLE, and
                // that distinction is the whole of the 0.1.2 review fix. The
                // first form of this guard was `min(|y|, |x|)`, which is a
                // pointwise operation on two signals the filter has put out of
                // phase: for bass-dominated programme `|x|` passes through
                // zero twice per cycle, so the clamp dragged the detector to
                // ~0 at the BASS rate — re-coupling the compressor to exactly
                // the low-frequency envelope this control exists to make it
                // deaf to, and gating whatever mid/high content the detector
                // should have been seeing at those instants. It also bound on
                // pure passband content, where filter phase (~43° at 2·fc)
                // makes `max_t min(|y|,|x|)` read ~0.4 dB under the unity
                // passband, and lowered the integrated RMS further still.
                //
                // The contract "may only deafen" is a statement about LEVELS,
                // so it is enforced against a level: the recent peak of the
                // raw magnitude. Overshoot is a transient excursion above the
                // local raw envelope and is still caught (on an LF edge the
                // raw peak is fresh and equals the edge's own amplitude),
                // while steady passband content is untouched — an RBJ
                // Butterworth at Q=0.707 has no magnitude peaking, so its
                // only excess over the input is transient by construction.
                det[ch] = juce::jmin (std::abs (y), ceilEnv[ch]);
            }
            maxDet  = juce::jmax (maxDet, det[ch]);
        }

        const float invR = 1.0f / R;
        float grDbMin = 0.0f;
        float grDb[kMaxChannels] = {};
        for (int ch = 0; ch < nCh; ++ch)
        {
            const float linked = juce::exactlyEqual (L, 1.0f)
                                     ? maxDet
                                     : L * maxDet + (1.0f - L) * det[ch];
            float level = linked;
            if (rmsDetector)
            {
                meanSquare[ch] += (linked * linked - meanSquare[ch]) * aRms;
                level = std::sqrt (meanSquare[ch]);
            }

            // -- static curve in dB -----------------------------------------
            // ZERO at or below the threshold — the knee softens the onset
            // ABOVE it (over [T, T+W]), it does not reach below (0.1.2 owner
            // directive, ADR-0023). The previous curve centred the knee on T,
            // so its lower half computed real gain from T − W/2 up: at the
            // 0 dBFS default threshold with the 6 dB default knee, material
            // whose detector level sat above −3 dBFS drew reduction the
            // "threshold 0 = no compression" definition says it must not.
            // C1-continuous: the quadratic's slope is 0 at T and (1/R − 1) at
            // T + W, where the full-ratio line (offset by W/2) takes over; at
            // W → 0 the curve degenerates to the hard-knee ratio line at T
            // exactly, which is what keeps the hard-knee tests byte-stable.
            const float levelDb = 20.0f * std::log10 (juce::jmax (level, 1.0e-9f));
            const float d       = levelDb - T;
            float targetGrDb;
            if (d <= 0.0f)
                targetGrDb = 0.0f;
            else if (d >= W || W < 1.0e-3f)
                targetGrDb = (d - W * 0.5f) * (invR - 1.0f);
            else
                targetGrDb = (invR - 1.0f) * d * d / (2.0f * W);

            // -- ballistics on the GR signal (dB domain) --------------------
            auto step = [this] (float& state, float target, float aRel) noexcept
            {
                state += (target - state) * (target < state ? aAtk : aRel);
            };
            if (autoRelease)
            {
                step (grFastDb[ch], targetGrDb, aRelFast);
                step (grSlowDb[ch], targetGrDb, aRelSlow);
            }
            else
            {
                step (grFastDb[ch], targetGrDb, aRelManual);
                grSlowDb[ch] = grFastDb[ch];  // keep the auto path from waking up stale
            }
            grDb[ch] = autoRelease ? 0.5f * (grFastDb[ch] + grSlowDb[ch]) : grFastDb[ch];
            grDbMin  = juce::jmin (grDbMin, grDb[ch]);
        }

        // -- apply, with exact identity paths --------------------------------
        if (grDbMin >= -1.0e-6f)
            return;   // no reduction anywhere: wet == dry, so every mix value
                      // lands on the input sample untouched — the bit-exact
                      // null path.
        for (int ch = 0; ch < nCh; ++ch)
        {
            const float gain = grDb[ch] >= -1.0e-6f
                                   ? 1.0f
                                   : std::pow (10.0f, grDb[ch] * (1.0f / 20.0f));
            const float dry = chans[ch];
            const float wet = dry * gain;
            if (juce::exactlyEqual (M, 1.0f))      chans[ch] = wet;
            else if (juce::exactlyEqual (M, 0.0f)) chans[ch] = dry;
            else                                    chans[ch] = dry + (wet - dry) * M;
        }
    }

    // Audio-thread meter tap (P3 publishes it through an atomic per the
    // THREAD_MODEL planned-edges list; tests read it single-threaded).
    // The DEEPEST channel since ADR-0019 — with the link below 100 % the two
    // envelopes can differ, and the meter reports the most reduction applied.
    float currentGainReductionDb() const noexcept
    {
        float g = 0.0f;
        for (int ch = 0; ch < kMaxChannels; ++ch)
            g = juce::jmin (g, currentGainReductionDb (ch));
        return g;
    }

    // The single-channel figure (0.1.2 item 12): the per-lane meter tap the
    // combined getter above folds. Meaningful per channel only below 100 %
    // link — at full link both envelopes are identical by construction.
    float currentGainReductionDb (int ch) const noexcept
    {
        ch &= 1;
        return autoRelease ? 0.5f * (grFastDb[ch] + grSlowDb[ch]) : grFastDb[ch];
    }

private:
    float onePole (float ms) const noexcept
    { return 1.0f - std::exp (-1.0f / (float) (ms * 0.001 * sr)); }

    void recomputeHpf() noexcept
    {
        // RBJ high-pass, Q = 0.7071 (Butterworth), detector-side. The RANGE
        // FLOOR (20 Hz) means NO detector filtering — an exact skip, the same
        // semantic as the limiter's detector (brief §3 shares one scHpfFreq
        // between both): a 2nd-order 20 Hz HPF at the floor is musically
        // indistinguishable from none, and the skip keeps the default
        // detector byte-exact.
        const bool wasOn = hpfOn;
        hpfOn = hpfFreq.getCurrentValue() > 20.001f;
        if (! hpfOn)
        {
            // Clear on the ON→OFF edge only (this runs per sample while the
            // frequency smooths): a later on→off→on cycle must re-enter with
            // an empty delay line, not one holding the old passband.
            if (wasOn)
                for (int ch = 0; ch < kMaxChannels; ++ch)
                    hpfZ1[ch] = hpfZ2[ch] = 0.0f;
            return;
        }
        const float f    = juce::jlimit (10.0f, (float) (0.49 * sr), hpfFreq.getCurrentValue());
        const float w0   = juce::MathConstants<float>::twoPi * f / (float) sr;
        const float cosw = std::cos (w0);
        const float alpha = std::sin (w0) / (2.0f * 0.7071068f);
        const float a0   = 1.0f + alpha;
        const float inv  = 1.0f / a0;
        hb0 = ((1.0f + cosw) * 0.5f) * inv;
        hb1 = (-(1.0f + cosw)) * inv;
        hb2 = hb0;
        ha1 = (-2.0f * cosw) * inv;
        ha2 = (1.0f - alpha) * inv;
    }

    std::array<juce::SmoothedValue<float>*, 6> smoothers() noexcept
    { return { &thresholdDb, &ratio, &kneeDb, &mix, &link, &hpfFreq }; }

    juce::SmoothedValue<float> thresholdDb { 0.0f }, ratio { 1.5f }, kneeDb { 6.0f },
                               mix { 1.0f }, link { 1.0f }, hpfFreq { 20.0f };

    // Detector HPF (normalised biquad) + per-channel state.
    float hb0 = 1.0f, hb1 = 0.0f, hb2 = 0.0f, ha1 = 0.0f, ha2 = 0.0f;
    float hpfZ1[kMaxChannels] = {}, hpfZ2[kMaxChannels] = {};
    bool  hpfOn = false;

    float meanSquare[kMaxChannels] = {};
    float aRms = 0.01f;

    // The sidechain HPF's overshoot ceiling: a peak envelope of the RAW
    // magnitude, instantaneous attack and a release long enough to bridge a
    // full period of the lowest programme the control addresses. 500 ms
    // against the 20 Hz bottom of the `scHpfFreq` range is a 50 ms period
    // spanning e^(−0.1) ≈ 0.905, i.e. under 0.9 dB of ceiling droop per
    // cycle — which is what keeps the CEILING from acquiring the bass-rate
    // ripple the pointwise form had. Erring LONG is the safe direction: a
    // stale-high ceiling merely stops the guard binding, which is the
    // unclamped filtered detector (correct in steady state, since the
    // Butterworth cannot peak), while a short one re-creates the defect.
    static constexpr float kCeilingReleaseMs = 500.0f;
    float ceilEnv[kMaxChannels] = {};
    float aCeilRel = 0.0f;

    // Ballistics. The auto constants are deliberately named, not exposed.
    static constexpr float kAutoFastMs = 80.0f, kAutoSlowMs = 900.0f;
    float aAtk = 0.01f, aRelManual = 0.01f;
    float aRelFast = 0.0f, aRelSlow = 0.0f;
    float grFastDb[kMaxChannels] = {}, grSlowDb[kMaxChannels] = {};

    bool  autoRelease = true, rmsDetector = true, primed = false;
    double sr = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasteringComp)
};

} // namespace anabasis
