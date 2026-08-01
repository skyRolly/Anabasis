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
//  - Detector: stereo-linked (one gain for all channels — mastering glue must
//    not wander the image). Per-channel sidechain HPF (20–300 Hz, RBJ
//    Butterworth, detector-side ONLY — the audio path never passes through
//    it), then max-of-channels magnitude. RMS mode squares through a fixed
//    10 ms window; Peak mode uses the magnitude directly.
//  - Static curve: soft knee of total width W dB centred on the threshold
//    (quadratic interpolation inside the knee; W below a millidB is treated
//    as hard to keep the 1/2W term finite).
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
//    all-defaults null bit-exact (threshold 0 dBFS: nothing below the knee
//    bottom ever computes a gain).
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
        aRelFast = onePole (kAutoFastMs);
        aRelSlow = onePole (kAutoSlowMs);
        reset();
    }

    void reset() noexcept
    {
        grFastDb = grSlowDb = 0.0f;
        meanSquare = 0.0f;
        for (int ch = 0; ch < kMaxChannels; ++ch)
            hpfZ1[ch] = hpfZ2[ch] = 0.0f;
        primed = false;
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
        if (hpfFreq.isSmoothing())
        {
            hpfFreq.getNextValue();
            recomputeHpf();
        }

        // -- detector (sidechain only, never the audio path) ----------------
        float det = 0.0f;
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
            det = juce::jmax (det, std::abs (y));
        }
        float level = det;
        if (rmsDetector)
        {
            meanSquare += (det * det - meanSquare) * aRms;
            level = std::sqrt (meanSquare);
        }

        // -- static curve in dB ---------------------------------------------
        const float levelDb = 20.0f * std::log10 (juce::jmax (level, 1.0e-9f));
        const float invR    = 1.0f / R;
        const float d       = levelDb - T;
        float targetGrDb;
        if (2.0f * d <= -W)
            targetGrDb = 0.0f;
        else if (2.0f * d >= W || W < 1.0e-3f)
            targetGrDb = d * (invR - 1.0f);
        else
        {
            const float t = d + W * 0.5f;
            targetGrDb = (invR - 1.0f) * t * t / (2.0f * W);
        }

        // -- ballistics on the GR signal (dB domain) -------------------------
        auto step = [this] (float& state, float target, float aRel) noexcept
        {
            state += (target - state) * (target < state ? aAtk : aRel);
        };
        if (autoRelease)
        {
            step (grFastDb, targetGrDb, aRelFast);
            step (grSlowDb, targetGrDb, aRelSlow);
        }
        else
        {
            step (grFastDb, targetGrDb, aRelManual);
            grSlowDb = grFastDb;          // keep the auto path from waking up stale
        }
        const float grDb = autoRelease ? 0.5f * (grFastDb + grSlowDb) : grFastDb;

        // -- apply, with exact identity paths --------------------------------
        if (grDb >= -1.0e-6f)
            return;   // no reduction: wet == dry, so every mix value lands on
                      // the input sample untouched — the bit-exact null path.
        const float gain = std::pow (10.0f, grDb * (1.0f / 20.0f));
        for (int ch = 0; ch < nCh; ++ch)
        {
            const float dry = chans[ch];
            const float wet = dry * gain;
            if (juce::exactlyEqual (M, 1.0f))      chans[ch] = wet;
            else if (juce::exactlyEqual (M, 0.0f)) chans[ch] = dry;
            else                                    chans[ch] = dry + (wet - dry) * M;
        }
    }

    // Audio-thread meter tap (P3 publishes it through an atomic per the
    // THREAD_MODEL planned-edges list; tests read it single-threaded).
    float currentGainReductionDb() const noexcept
    { return autoRelease ? 0.5f * (grFastDb + grSlowDb) : grFastDb; }

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
        hpfOn = hpfFreq.getCurrentValue() > 20.001f;
        if (! hpfOn)
            return;
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

    std::array<juce::SmoothedValue<float>*, 5> smoothers() noexcept
    { return { &thresholdDb, &ratio, &kneeDb, &mix, &hpfFreq }; }

    juce::SmoothedValue<float> thresholdDb { 0.0f }, ratio { 1.5f }, kneeDb { 6.0f },
                               mix { 1.0f }, hpfFreq { 20.0f };

    // Detector HPF (normalised biquad) + per-channel state.
    float hb0 = 1.0f, hb1 = 0.0f, hb2 = 0.0f, ha1 = 0.0f, ha2 = 0.0f;
    float hpfZ1[kMaxChannels] = {}, hpfZ2[kMaxChannels] = {};
    bool  hpfOn = false;

    float meanSquare = 0.0f, aRms = 0.01f;

    // Ballistics. The auto constants are deliberately named, not exposed.
    static constexpr float kAutoFastMs = 80.0f, kAutoSlowMs = 900.0f;
    float aAtk = 0.01f, aRelManual = 0.01f;
    float aRelFast = 0.0f, aRelSlow = 0.0f;
    float grFastDb = 0.0f, grSlowDb = 0.0f;

    bool  autoRelease = true, rmsDetector = true, primed = false;
    double sr = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasteringComp)
};

} // namespace anabasis
