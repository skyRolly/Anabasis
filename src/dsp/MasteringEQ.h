#pragma once

#include "EngineParameters.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

// ============================================================================
//  MasteringEQ — the §2.2 static EQ: minimum-phase IIR, zero latency.
//
//  Six RBJ biquad sections in series:
//    [0][1]  Tilt ±3 dB about the ≈700 Hz pivot, implemented as the DESIGN-
//            mandated complementary shelving pair (low shelf −G + high shelf
//            +G, both at the pivot) — positive tilt brightens.
//    [2][3]  Low shelf (20–500 Hz) and high shelf (1–20 kHz), fixed Q 0.707
//            (DESIGN §2.2 ⊕).
//    [4][5]  Two bells (Freq/Gain/Q).
//
//  ALL-FLAT IS BIT-TRANSPARENT BY STRUCTURE, not by arithmetic: a section
//  whose (smoothed) gain sits at exactly 0 dB is SKIPPED — no multiply, no
//  state update — so DSP_POLICY invariant 7's null holds exactly, the way
//  DESIGN §2.2 requires ("skip when all gains are 0"). A biquad at 0 dB gain
//  is only approximately identity in float, which would fail the bit-exact
//  null. Re-engaging from 0 is click-free because the smoothed gain leaves
//  zero continuously and near-identity coefficients from cleared state
//  produce an O(gain) transient.
//
//  Every parameter that reaches this DSP is smoothed HERE (CODE_STYLE
//  §Real-time discipline): 11 SmoothedValues, 20 ms, primed on the first
//  block after prepare/reset. Coefficients recompute per sample only while
//  at least one smoother is moving; a static EQ costs six cached biquads.
// ============================================================================

namespace anabasis
{

class MasteringEQ
{
public:
    static constexpr int kMaxChannels = 2;

    MasteringEQ() = default;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto* s : allSmoothers())
            s->reset (sampleRate, 0.020);
        reset();
    }

    // Full reset: state cleared AND the smoothers re-adopt on the next block
    // (same primed pattern as the engine's four control smoothers).
    void reset() noexcept
    {
        for (auto& sec : sections)
            sec.clearState();
        primed = false;
    }

    // State-only reset, for the eqPosition rewire: the section coefficients
    // and smoothers are position-independent, but the biquad HISTORY belongs
    // to the stream the EQ was processing, and feeding the other stream
    // through it would start the new position from another signal's past.
    void resetState() noexcept
    {
        for (auto& sec : sections)
            sec.clearState();
    }

    // Once per block, from the POD snapshot (ADR-0011: never piecemeal).
    void setTargets (const EngineParameters& p) noexcept
    {
        auto set = [this] (juce::SmoothedValue<float>& s, float v) noexcept
        {
            if (primed) s.setTargetValue (v);
            else        s.setCurrentAndTargetValue (v);
        };
        set (tiltDb,   p.eqTiltDb);
        set (lsFreq,   p.eqLowShelfFreqHz);
        set (lsGain,   p.eqLowShelfGainDb);
        set (hsFreq,   p.eqHighShelfFreqHz);
        set (hsGain,   p.eqHighShelfGainDb);
        set (b1Freq,   p.eqBell1FreqHz);
        set (b1Gain,   p.eqBell1GainDb);
        set (b1Q,      p.eqBell1Q);
        set (b2Freq,   p.eqBell2FreqHz);
        set (b2Gain,   p.eqBell2GainDb);
        set (b2Q,      p.eqBell2Q);

        if (! primed)
        {
            primed = true;
            recompute();        // adopt: coefficients valid before the first tick
        }
    }

    // Once per OUTPUT SAMPLE, before the per-channel processSample calls:
    // advances the smoothers and recomputes coefficients while any is moving.
    void tick() noexcept
    {
        bool moving = false;
        for (auto* s : allSmoothers())
            if (s->isSmoothing())
            { moving = true; break; }

        if (! moving)
        {
            // Values are settled and the cached coefficients match them.
            return;
        }

        for (auto* s : allSmoothers())
            s->getNextValue();
        recompute();
    }

    float processSample (int ch, float x) noexcept
    {
        for (auto& sec : sections)
            if (sec.engaged)
                x = sec.process (ch, x);
        return x;
    }

    // True when every section is skipped — the all-flat structural null.
    bool isTransparent() const noexcept
    {
        for (const auto& sec : sections)
            if (sec.engaged)
                return false;
        return true;
    }

private:
    // RBJ Audio-EQ-Cookbook biquad, transposed direct form II, per-channel
    // state. Coefficients are normalised (a0 divided out).
    struct Section
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float z1[kMaxChannels] = {}, z2[kMaxChannels] = {};
        bool  engaged = false;

        float process (int ch, float x) noexcept
        {
            const float y = b0 * x + z1[ch];
            z1[ch] = b1 * x - a1 * y + z2[ch];
            z2[ch] = b2 * x - a2 * y;
            return y;
        }

        void clearState() noexcept
        {
            for (int ch = 0; ch < kMaxChannels; ++ch)
                z1[ch] = z2[ch] = 0.0f;
        }

        // Engage/disengage with the state rule: a section that goes to sleep
        // clears its history, so re-engaging never replays another era's tail.
        void setEngaged (bool shouldRun) noexcept
        {
            if (engaged && ! shouldRun)
                clearState();
            engaged = shouldRun;
        }
    };

    enum { tiltLo = 0, tiltHi, lowShelf, highShelf, bell1, bell2, kNumSections };

    static constexpr float kTiltPivotHz = 700.0f;   // DESIGN §2.2 / brief §4.4
    static constexpr float kShelfQ      = 0.7071068f;

    void recompute() noexcept
    {
        const float tilt = tiltDb.getCurrentValue();
        const bool tiltOn = ! juce::exactlyEqual (tilt, 0.0f);
        sections[tiltLo].setEngaged (tiltOn);
        sections[tiltHi].setEngaged (tiltOn);
        if (tiltOn)
        {
            computeLowShelf  (sections[tiltLo], kTiltPivotHz, -tilt, kShelfQ);
            computeHighShelf (sections[tiltHi], kTiltPivotHz,  tilt, kShelfQ);
        }

        const float lsG = lsGain.getCurrentValue();
        sections[lowShelf].setEngaged (! juce::exactlyEqual (lsG, 0.0f));
        if (sections[lowShelf].engaged)
            computeLowShelf (sections[lowShelf], lsFreq.getCurrentValue(), lsG, kShelfQ);

        const float hsG = hsGain.getCurrentValue();
        sections[highShelf].setEngaged (! juce::exactlyEqual (hsG, 0.0f));
        if (sections[highShelf].engaged)
            computeHighShelf (sections[highShelf], hsFreq.getCurrentValue(), hsG, kShelfQ);

        const float g1 = b1Gain.getCurrentValue();
        sections[bell1].setEngaged (! juce::exactlyEqual (g1, 0.0f));
        if (sections[bell1].engaged)
            computeBell (sections[bell1], b1Freq.getCurrentValue(), g1, b1Q.getCurrentValue());

        const float g2 = b2Gain.getCurrentValue();
        sections[bell2].setEngaged (! juce::exactlyEqual (g2, 0.0f));
        if (sections[bell2].engaged)
            computeBell (sections[bell2], b2Freq.getCurrentValue(), g2, b2Q.getCurrentValue());
    }

    // The angular frequency, clamped so w0 stays inside (0, pi): a 20 kHz
    // bell at a 32 kHz session must detune rather than blow up.
    float omega (float freqHz) const noexcept
    {
        const float f = juce::jlimit (10.0f, (float) (0.49 * sr), freqHz);
        return juce::MathConstants<float>::twoPi * f / (float) sr;
    }

    void computeBell (Section& s, float freqHz, float gainDb, float q) noexcept
    {
        const float A     = std::pow (10.0f, gainDb * (1.0f / 40.0f));
        const float w0    = omega (freqHz);
        const float cosw  = std::cos (w0);
        const float alpha = std::sin (w0) / (2.0f * juce::jmax (0.05f, q));
        const float a0    = 1.0f + alpha / A;
        const float inv   = 1.0f / a0;
        s.b0 = (1.0f + alpha * A) * inv;
        s.b1 = (-2.0f * cosw) * inv;
        s.b2 = (1.0f - alpha * A) * inv;
        s.a1 = s.b1;                       // identical term in the cookbook form
        s.a2 = (1.0f - alpha / A) * inv;
    }

    void computeLowShelf (Section& s, float freqHz, float gainDb, float q) noexcept
    {
        const float A     = std::pow (10.0f, gainDb * (1.0f / 40.0f));
        const float w0    = omega (freqHz);
        const float cosw  = std::cos (w0);
        const float alpha = std::sin (w0) / (2.0f * q);
        const float sqA2a = 2.0f * std::sqrt (A) * alpha;
        const float a0    = (A + 1.0f) + (A - 1.0f) * cosw + sqA2a;
        const float inv   = 1.0f / a0;
        s.b0 = (A * ((A + 1.0f) - (A - 1.0f) * cosw + sqA2a)) * inv;
        s.b1 = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw)) * inv;
        s.b2 = (A * ((A + 1.0f) - (A - 1.0f) * cosw - sqA2a)) * inv;
        s.a1 = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cosw)) * inv;
        s.a2 = ((A + 1.0f) + (A - 1.0f) * cosw - sqA2a) * inv;
    }

    void computeHighShelf (Section& s, float freqHz, float gainDb, float q) noexcept
    {
        const float A     = std::pow (10.0f, gainDb * (1.0f / 40.0f));
        const float w0    = omega (freqHz);
        const float cosw  = std::cos (w0);
        const float alpha = std::sin (w0) / (2.0f * q);
        const float sqA2a = 2.0f * std::sqrt (A) * alpha;
        const float a0    = (A + 1.0f) - (A - 1.0f) * cosw + sqA2a;
        const float inv   = 1.0f / a0;
        s.b0 = (A * ((A + 1.0f) + (A - 1.0f) * cosw + sqA2a)) * inv;
        s.b1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw)) * inv;
        s.b2 = (A * ((A + 1.0f) + (A - 1.0f) * cosw - sqA2a)) * inv;
        s.a1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * cosw)) * inv;
        s.a2 = ((A + 1.0f) - (A - 1.0f) * cosw - sqA2a) * inv;
    }

    // The 11 smoothed inputs, iterable so tick()/prepare() cannot miss one.
    juce::SmoothedValue<float> tiltDb { 0.0f },
                               lsFreq { 100.0f },  lsGain { 0.0f },
                               hsFreq { 8000.0f }, hsGain { 0.0f },
                               b1Freq { 300.0f },  b1Gain { 0.0f }, b1Q { 1.0f },
                               b2Freq { 3000.0f }, b2Gain { 0.0f }, b2Q { 1.0f };

    std::array<juce::SmoothedValue<float>*, 11> allSmoothers() noexcept
    {
        return { &tiltDb, &lsFreq, &lsGain, &hsFreq, &hsGain,
                 &b1Freq, &b1Gain, &b1Q, &b2Freq, &b2Gain, &b2Q };
    }

    Section sections[kNumSections];
    double  sr     = 48000.0;
    bool    primed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasteringEQ)
};

} // namespace anabasis
