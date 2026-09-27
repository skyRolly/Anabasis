#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "ClampTruePeakDetector.h"   // the shared 4× phase design (JUCE-free)
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
//  limiter's gain computer (and later the dBTP meter). REPORTING LAG = **6**
//  input samples, and that is the number RISK-008 tracks. The FIR's nominal
//  group delay is (kTaps−1)/2 = 5.5, but the estimate returned at step n is a
//  MAXIMUM over x[n−6] (the `best` seed) and the three interpolated points at
//  n−5.75 / n−5.5 / n−5.25 — so the oldest sample it can be describing is
//  n−6, which is the figure a lookahead-margin argument has to use. It must
//  fit inside the 0.5 ms minimum engaged lookahead (24 samples at 48 kHz):
//  6 < 24, with margin. A limiter fed from it attacks up to 6 samples less
//  early and holds up to 6 samples longer — both inside the wedge window,
//  neither affecting invariant 4 (the clamp is downstream and unconditional).
//
//  Per call, the estimate covers |x[n−6]| plus the three interpolated points
//  in (n−6, n−5); consecutive calls therefore cover every sample and every
//  quarter-sample point exactly once. Known property of ANY max-reading 4×
//  estimator, recorded rather than hidden: a true peak landing between two
//  4× points under-reads by up to ~0.15 dB at fs/4 content — and by more above
//  it, up to ~0.69 dB for a sinusoid near Nyquist from the grid alone. The
//  grid-aligned canonical ISP vectors must read within 0.1 dB (the invariant-3
//  test); the off-grid worst case is measured and bounded in the same test.
//  (Corrected 2026-09-27, audit DSP-001 sub-item (d): this said "~0.15 dB at
//  fs/4" and stopped, which is true at 12 kHz and understates everything above.)
//
//  WHAT THE 12-TAP BLACKMAN KERNEL ADDS, measured on the engine's own output
//  (worklog 2026-09-27, KNOWN_ISSUES KI-020): on HF-rich programme this
//  estimator reads up to ~1.4 dB BELOW the BS.1770 Annex 2 example filter, and
//  on dense broadband material up to ~0.24 dB ABOVE an accurate interpolator.
//  It stays the meter's and the limiter's estimator; the ceiling clamp holds
//  its ceiling on ClampTruePeakDetector (ClampTruePeakDetector.h), which
//  includes this one's phases.
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
        designPhases (coeff);
        reset();
    }

    // The coefficient design, callable on its own. It lives in the JUCE-free
    // ClampTruePeakDetector.h (`truepeak::designMeterPhases`) because the
    // ceiling clamp's detector evaluates these SAME phases, so that the meter
    // reading is one of the readings it holds under the ceiling — and the clamp
    // must stay a JUCE-free leaf (see that header). Index k multiplies x[n−k];
    // phase 0 is unused (the sample itself is the seed).
    static void designPhases (float (&c)[kPhases][kTaps]) noexcept
    {
        static_assert (kPhases == truepeak::kMeterPhases && kTaps == truepeak::kMeterTaps);
        truepeak::designMeterPhases (c);
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
    float coeff[kPhases][kTaps] = {};
    float hist[kMaxChannels][kTaps] = {};
    int   writeIdx = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TruePeakEstimator)
};

} // namespace anabasis
