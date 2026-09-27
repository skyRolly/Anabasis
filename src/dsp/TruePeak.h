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
//  its ceiling on ClampTruePeakDetector (below), which includes this one.
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

    // The coefficient design, callable on its own: the ceiling clamp's
    // detector (below) evaluates these SAME phases so that the meter reading
    // is one of the readings it holds under the ceiling. Index k multiplies
    // x[n−k]; phase 0 is unused (the sample itself is the seed).
    static void designPhases (float (&c)[kPhases][kTaps]) noexcept
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
                c[p][k] = wnd * sinc ((float) k - d);
                sum += c[p][k];
            }
            // Exact unity DC response — a plain windowed sinc is a hair off,
            // and that hair would be straight passband error.
            for (int k = 0; k < kTaps; ++k)
                c[p][k] /= sum;
        }
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

// ============================================================================
//  ClampTruePeakDetector — the ceiling clamp's detector (ADR-0041).
//
//  WHY NOT THE ESTIMATOR ABOVE. That one is a 4× measurement tap sized for the
//  limiter's detector and the meter: 12 taps under a Blackman window. It is
//  not a hard bound in EITHER direction on programme with real top-octave
//  energy — measured on the engine's own TP-mode output (worklog 2026-09-27)
//  it reads up to ~1.4 dB UNDER the order-48 example filter of BS.1770
//  Annex 2 and a high-accuracy reference, and on dense broadband material up
//  to ~0.24 dB OVER an accurate interpolator. True-peak meters disagree with
//  each other by that much because each is a finite approximation of the
//  continuous waveform; a clamp driven by any ONE of them holds the ceiling
//  on that meter only.
//
//  SO THE CLAMP HOLDS THE CEILING ON THREE READINGS AT ONCE, all taken on the
//  SAME 32-sample window and describing the SAME segment — x[j] and the
//  continuous waveform between x[j] and x[j+1], j = n − 16 — and the reading
//  is the largest of the three:
//    1. accurate: a 32-tap Kaiser (β = 8) windowed-sinc interpolator designed
//       here (unity DC gain per phase), evaluated at the quarter points and
//       REFINED by a parabola through the largest grid point and its two
//       neighbours — within ~0.05 dB of the continuous peak up to ~0.45·fs,
//       at a fifth of the cost of a 16-point grid;
//    2. the product's own meter: the 4× phases of TruePeakEstimator, evaluated
//       on the same window, so the dBTP display reads the clamp's output at
//       the ceiling — to within a few thousandths of a dB, because the gain
//       the clamp then applies moves inside the meter's interpolation window
//       (measured; KNOWN_ISSUES KI-020);
//    3. the Recommendation's own example: the order-48, 4-phase FIR of
//       ITU-R BS.1770-5 Annex 2 (identical in BS.1770-4), the table below.
//  What none of them resolves — content in the last few percent below
//  Nyquist, which a longer reference kernel still sees — is recorded in
//  KNOWN_ISSUES KI-020, not claimed.
//
//  REPORTING LAG = 16 (reading x[n−31 .. n]). LAYOUT for the audio thread:
//  the history is kept twice over (a double-length ring) so the window is
//  always contiguous; every filter is folded over its mirror symmetry
//  (`processFrame`), so a mirror pair of phases costs one half-length dot
//  product; both channels accumulate in the same loops; and every sum runs in
//  a fixed order, so a reading is reproducible to the bit.
// ============================================================================
class ClampTruePeakDetector
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kTaps        = 32;
    static constexpr int kLag         = kTaps / 2;   // 16

    // ITU-R BS.1770-5 (11/2023) Annex 2, "one set of filter coefficients (for
    // the order 48, 4-phase, FIR interpolating) that would satisfy the
    // requirements", phase-major; index k multiplies x[n−k], and the four
    // phases read the points 1/8, 3/8, 5/8, 7/8 of the way from x[n−6] to
    // x[n−5]. Transcribed, so pinned by testClampTruePeakDetector (the
    // Recommendation's own symmetry — phase 3 is phase 0 reversed, phase 2 is
    // phase 1 reversed — and every phase's DC gain).
    static constexpr float kItuAnnex2[4][12] = {
        {  0.0017089843750f,  0.0109863281250f, -0.0196533203125f,  0.0332031250000f,
          -0.0594482421875f,  0.1373291015625f,  0.9721679687500f, -0.1022949218750f,
           0.0476074218750f, -0.0266113281250f,  0.0148925781250f, -0.0083007812500f },
        { -0.0291748046875f,  0.0292968750000f, -0.0517578125000f,  0.0891113281250f,
          -0.1665039062500f,  0.4650878906250f,  0.7797851562500f, -0.2003173828125f,
           0.1015625000000f, -0.0582275390625f,  0.0330810546875f, -0.0189208984375f },
        { -0.0189208984375f,  0.0330810546875f, -0.0582275390625f,  0.1015625000000f,
          -0.2003173828125f,  0.7797851562500f,  0.4650878906250f, -0.1665039062500f,
           0.0891113281250f, -0.0517578125000f,  0.0292968750000f, -0.0291748046875f },
        { -0.0083007812500f,  0.0148925781250f, -0.0266113281250f,  0.0476074218750f,
          -0.1022949218750f,  0.9721679687500f,  0.1373291015625f, -0.0594482421875f,
           0.0332031250000f, -0.0196533203125f,  0.0109863281250f,  0.0017089843750f } };

    ClampTruePeakDetector() = default;

    void prepare()
    {
        // The accurate kernel's quarter phases. Window slot i holds x[n−31+i];
        // phase p reads the point p/4 of the way from x[n−16] (slot 15) to
        // x[n−15]. Only phases 1 and 2 are designed: phase 3 is phase 1
        // mirrored (d₃(31−i) = −d₁(i), and both the sinc and the Kaiser window
        // are even), and phase 2 is its own mirror — the folding below uses
        // exactly that.
        double c[3][kTaps] = {};
        for (int p = 1; p <= 2; ++p)
        {
            constexpr double beta = 8.0;
            constexpr int    half = kTaps / 2;
            double sum = 0.0;
            for (int i = 0; i < kTaps; ++i)
            {
                const double d = (double) (half - 1 - i) + (double) p / 4.0;
                const double u = d / (double) half;
                const double w = std::abs (u) >= 1.0 ? 0.0
                               : besselI0 (beta * std::sqrt (1.0 - u * u)) / besselI0 (beta);
                const double pd = juce::MathConstants<double>::pi * d;
                c[p][i] = w * (std::abs (d) < 1.0e-12 ? 1.0 : std::sin (pd) / pd);
                sum += c[p][i];
            }
            for (int i = 0; i < kTaps; ++i)
                c[p][i] /= sum;                                  // unity DC gain per phase
        }
        for (int i = 0; i < kFold; ++i)
        {
            accHalf[i]  = (float) c[2][i];                                        // × (x[i] + x[31−i])
            accQSum[i]  = (float) (0.5 * (c[1][i] + c[1][kTaps - 1 - i]));        // × (x[i] + x[31−i])
            accQDiff[i] = (float) (0.5 * (c[1][i] - c[1][kTaps - 1 - i]));        // × (x[i] − x[31−i])
        }

        // The two 12-tap meter filters read x[j−5 .. j+6] = window slots 10..21
        // (their tap k multiplies x[j+6−k], i.e. slot 21 − k), which is again
        // symmetric about the segment. TruePeakEstimator's phase 2 is its own
        // mirror and its phases 1 and 3 mirror each other (the Blackman window
        // and the sinc are both even); the Annex 2 table mirrors phase 0 with 3
        // and 1 with 2, as the Recommendation prints it. Folded over the six
        // pairs (slot 10+t, slot 21−t), t = 0..5, where tap k = 11 − t.
        float std4x[TruePeakEstimator::kPhases][TruePeakEstimator::kTaps] = {};
        TruePeakEstimator::designPhases (std4x);
        for (int t = 0; t < 6; ++t)
        {
            const int k = 11 - t, km = t;                               // slot 10+t, its mirror 21−t
            meterHalf[t]      = std4x[2][k];
            meterQSum[t]      = 0.5f * (std4x[1][k] + std4x[1][km]);
            meterQDiff[t]     = 0.5f * (std4x[1][k] - std4x[1][km]);
            ituOuterSum[t]    = 0.5f * (kItuAnnex2[0][k] + kItuAnnex2[0][km]);
            ituOuterDiff[t]   = 0.5f * (kItuAnnex2[0][k] - kItuAnnex2[0][km]);
            ituInnerSum[t]    = 0.5f * (kItuAnnex2[1][k] + kItuAnnex2[1][km]);
            ituInnerDiff[t]   = 0.5f * (kItuAnnex2[1][k] - kItuAnnex2[1][km]);
        }
        reset();
    }

    void reset() noexcept
    {
        for (auto& h : hist)
            for (auto& v : h)
                v = 0.0f;
        for (auto& v : lastQuarter)
            v = 0.0f;
        writeIdx = 0;
    }

    // Push one frame; per channel, the largest of the three readings of the
    // segment kLag steps back. Audio-thread, allocation-free.
    //
    // FOLDED: every filter here is symmetric about the segment or pairs with
    // its own mirror, so the window is folded into pair sums and differences
    // and each mirror PAIR of phases costs one dot product over half the taps —
    // about a third of the multiplies of evaluating the ten phases directly.
    void processFrame (const float* x, int numCh, float* tpOut) noexcept
    {
        writeIdx = (writeIdx + 1) % kTaps;
        const int nCh = juce::jmin (numCh, kMaxChannels);
        const float* w[kMaxChannels] = {};
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto& h = hist[ch];
            h[(size_t) writeIdx] = x[ch];
            h[(size_t) (writeIdx + kTaps)] = x[ch];
            w[ch] = &h[(size_t) (writeIdx + 1)];               // oldest → newest
        }
        // BOTH CHANNELS IN ONE PASS: their accumulations are independent, so
        // interleaving them doubles the work in flight per loop step. A mono
        // frame runs the same loops on a zero history for the second lane.
        if (nCh < kMaxChannels)
            w[1] = zeros;

        // Accurate kernel: y2 (half), y1/y3 (quarters) = (A ± B).
        float y2[2] = {}, qs[2] = {}, qd[2] = {};
        for (int i = 0; i < kFold; ++i)
        {
            const float ch = accHalf[i], cs = accQSum[i], cd = accQDiff[i];
            for (int c = 0; c < 2; ++c)
            {
                const float sg = w[c][i] + w[c][kTaps - 1 - i];
                const float df = w[c][i] - w[c][kTaps - 1 - i];
                y2[c] += ch * sg;
                qs[c] += cs * sg;
                qd[c] += cd * df;
            }
        }
        // Meters: the product's 4× phases 1..3 and the four Annex 2 phases.
        float mh[2] = {}, ms[2] = {}, md[2] = {}, os[2] = {}, od[2] = {}, is[2] = {}, id[2] = {};
        for (int t = 0; t < 6; ++t)
            for (int c = 0; c < 2; ++c)
            {
                const float sg = w[c][10 + t] + w[c][21 - t], dg = w[c][10 + t] - w[c][21 - t];
                mh[c] += meterHalf[t] * sg;
                ms[c] += meterQSum[t] * sg;       md[c] += meterQDiff[t] * dg;
                os[c] += ituOuterSum[t] * sg;     od[c] += ituOuterDiff[t] * dg;
                is[c] += ituInnerSum[t] * sg;     id[c] += ituInnerDiff[t] * dg;
            }

        for (int ch = 0; ch < nCh; ++ch)
        {
            float meterMax = juce::jmax (std::abs (mh[ch]), std::abs (ms[ch] + md[ch]), std::abs (ms[ch] - md[ch]));
            meterMax = juce::jmax (meterMax, std::abs (os[ch] + od[ch]), std::abs (os[ch] - od[ch]));
            meterMax = juce::jmax (meterMax, std::abs (is[ch] + id[ch]), std::abs (is[ch] - id[ch]));

            // The accurate quarter-point grid of this segment, one point either
            // side for the refinement: g[0] = the last quarter point of the
            // previous segment, g[1] = x[j], g[2..4] = the interpolated
            // quarters, g[5] = x[j+1].
            const float y1 = qs[ch] + qd[ch], y3 = qs[ch] - qd[ch];
            const float g[6] = { lastQuarter[ch], w[ch][kLag - 1], y1, y2[ch], y3, w[ch][kLag] };
            lastQuarter[ch] = y3;
            int m = 1;
            for (int i = 2; i <= 4; ++i)
                if (std::abs (g[i]) > std::abs (g[m]))
                    m = i;
            const float refined = juce::jmax (std::abs (g[m]), refinedPeak (g[m - 1], g[m], g[m + 1]));
            tpOut[ch] = juce::jmax (refined, meterMax);
        }
    }

private:
    static constexpr int kFold = kTaps / 2;

    // The vertex of the parabola through three quarter-sample-spaced points
    // around a magnitude maximum — the continuous peak the grid straddles.
    // Only when the three points are one lobe (same sign) with the middle one
    // outermost and the curvature pointing back towards zero; otherwise the
    // grid point itself is the answer. Bounded to +0.5 dB of the grid point,
    // which the refinement of a band-limited peak never needs (≤ ~0.2 dB at
    // 0.45·fs) and which stops a near-flat, near-degenerate parabola from
    // manufacturing a reading.
    static float refinedPeak (float a, float b, float c) noexcept
    {
        if (! ((a > 0.0f && b > 0.0f && c > 0.0f) || (a < 0.0f && b < 0.0f && c < 0.0f)))
            return std::abs (b);
        const float mag = std::abs (b);
        if (std::abs (a) > mag || std::abs (c) > mag)
            return mag;
        const float curv = a - 2.0f * b + c;               // opposite in sign to b at a peak
        if (! (curv * b < 0.0f))
            return mag;
        const float slope = c - a;
        const float vertex = std::abs (b - slope * slope / (8.0f * curv));
        return juce::jmin (vertex, mag * 1.0592537f);      // +0.5 dB
    }

    static double besselI0 (double x) noexcept
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 64; ++k)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
            if (term < sum * 1.0e-17)
                break;
        }
        return sum;
    }

    float accHalf[kTaps / 2] = {}, accQSum[kTaps / 2] = {}, accQDiff[kTaps / 2] = {};
    float meterHalf[6] = {}, meterQSum[6] = {}, meterQDiff[6] = {};
    float ituOuterSum[6] = {}, ituOuterDiff[6] = {}, ituInnerSum[6] = {}, ituInnerDiff[6] = {};
    float hist[kMaxChannels][2 * kTaps] = {};
    float zeros[kTaps] = {};                           // the idle second lane of a mono frame
    float lastQuarter[kMaxChannels] = {};
    int   writeIdx = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClampTruePeakDetector)
};

} // namespace anabasis
