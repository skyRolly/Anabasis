#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

// ============================================================================
//  TruePeakEstimator — the ADR-0003 measurement tap.
//
//  4× polyphase interpolation (BS.1770-4 prescribes ≥ 4× oversampled peak
//  estimation; its Annex FIR is an example implementation), 12 taps per
//  phase, windowed-sinc designed at prepare() — no coefficient table copied
//  from anywhere, so there is nothing to mis-transcribe; the accuracy TEST is
//  the compliance evidence (C2).
//
//  MEASUREMENT TAP ONLY, never in the audio path (ADR-0003): it feeds the
//  limiter's gain computer (and later the dBTP meter). Group delay =
//  (kTaps−1)/2 = 5.5 input samples — the number RISK-008 tracks: it must fit
//  inside the 0.5 ms minimum engaged lookahead (24 samples at 48 kHz), and
//  5.5 < 24 with margin. The estimate returned at step n describes the signal
//  around n − 5.5, so a limiter fed from it attacks ~5.5 samples less early
//  and holds ~5.5 samples longer — both inside the wedge window, neither
//  affecting invariant 4 (the clamp is downstream and unconditional).
//
//  Per call, the estimate covers |x[n−6]| plus the three interpolated points
//  in (n−6, n−5); consecutive calls therefore cover every sample and every
//  quarter-sample point exactly once. Known property of ANY max-reading 4×
//  estimator, recorded rather than hidden: a true peak landing between two
//  4× points under-reads by up to ~0.15 dB at fs/4 content. The grid-aligned
//  canonical ISP vectors must read within 0.1 dB (the invariant-3 test); the
//  off-grid worst case is measured and bounded in the same test.
// ============================================================================

namespace anabasis
{

class TruePeakEstimator
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kTaps        = 12;   // per phase
    static constexpr int kPhases      = 4;

    TruePeakEstimator() = default;

    void prepare()
    {
        for (int p = 1; p < kPhases; ++p)
        {
            // Fractional delays 5.75 / 5.5 / 5.25: the points between
            // x[n−6] and x[n−5].
            const float d = 6.0f - (float) p / (float) kPhases;
            float sum = 0.0f;
            for (int k = 0; k < kTaps; ++k)
            {
                const float u = ((float) k - d + 6.0f) / (float) kTaps;   // window position
                const float wnd = (u <= 0.0f || u >= 1.0f) ? 0.0f
                    : 0.42f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * u)
                            + 0.08f * std::cos (2.0f * juce::MathConstants<float>::twoPi * u);
                coeff[p][k] = wnd * sinc ((float) k - d);
                sum += coeff[p][k];
            }
            // Exact unity DC response — a plain windowed sinc is a hair off,
            // and that hair would be straight passband error.
            for (int k = 0; k < kTaps; ++k)
                coeff[p][k] /= sum;
        }
        reset();
    }

    void reset() noexcept
    {
        for (int ch = 0; ch < kMaxChannels; ++ch)
            for (int k = 0; k < kTaps; ++k)
                hist[ch][k] = 0.0f;
        writeIdx = 0;
    }

    // Push one frame (all channels of one sample step), get per-channel
    // true-peak magnitude estimates. Audio-thread, allocation-free.
    void processFrame (const float* x, int numCh, float* tpOut) noexcept
    {
        writeIdx = (writeIdx + 1) % kTaps;
        const int nCh = juce::jmin (numCh, kMaxChannels);
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto& h = hist[ch];
            h[(size_t) writeIdx] = x[ch];

            float best = std::abs (h[(size_t) ((writeIdx + kTaps - 6) % kTaps)]);
            for (int p = 1; p < kPhases; ++p)
            {
                float acc = 0.0f;
                for (int k = 0; k < kTaps; ++k)
                    acc += coeff[p][k] * h[(size_t) ((writeIdx + kTaps - k) % kTaps)];
                best = juce::jmax (best, std::abs (acc));
            }
            tpOut[ch] = best;
        }
    }

private:
    static float sinc (float t) noexcept
    {
        if (std::abs (t) < 1.0e-6f) return 1.0f;
        const float pt = juce::MathConstants<float>::pi * t;
        return std::sin (pt) / pt;
    }

    float coeff[kPhases][kTaps] = {};
    float hist[kMaxChannels][kTaps] = {};
    int   writeIdx = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TruePeakEstimator)
};

} // namespace anabasis
