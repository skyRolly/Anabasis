#pragma once

#include <cmath>
#include <cstddef>

// ============================================================================
//  ClampTruePeakDetector.h — the ceiling clamp's true-peak detector, and the
//  product meter's 4× phase design it shares with TruePeakEstimator.
//
//  THIS HEADER INCLUDES NO JUCE MODULE, deliberately, and must keep it that way.
//  It sits under CeilingClamp.h, which is one of the engine's JUCE-free leaf
//  headers that `tests/realtime_effects.cpp` compiles with `-Wfunction-effects`
//  (ADR-0029): Clang can only prove an audio routine non-blocking through a
//  call graph whose definitions it can see, and JUCE carries none, so one JUCE
//  include here would take the whole ceiling stage out of that gate. Until
//  0.2.13 the detector lived in TruePeak.h, which includes juce_audio_basics —
//  and that include is what failed the `realtime` CI job on the PR that added
//  it. The helpers below reproduce JUCE's `jmin`/`jmax` and `MathConstants`
//  expression for expression, so moving the code here changed no arithmetic:
//  a bitwise fingerprint of the estimator's and the detector's outputs is
//  identical before and after the move (the PR #42 review worklog).
// ============================================================================

namespace anabasis
{
namespace truepeak
{
    // JUCE's MathConstants<float>::pi / twoPi and MathConstants<double>::pi,
    // spelled with the SAME expressions so the rounded values are identical.
    inline constexpr float  kPi    = static_cast<float>  (3.141592653589793238L);
    inline constexpr float  kTwoPi = static_cast<float>  (2 * 3.141592653589793238L);
    inline constexpr double kPiD   = static_cast<double> (3.141592653589793238L);

    // JUCE's jmin / jmax (two and three arguments), expression for expression —
    // the same comparison order, so a NaN operand resolves the same way too.
    template <typename T> constexpr T min2 (T a, T b) noexcept { return b < a ? b : a; }
    template <typename T> constexpr T max2 (T a, T b) noexcept { return a < b ? b : a; }
    template <typename T> constexpr T max3 (T a, T b, T c) noexcept
    { return a < b ? (b < c ? c : b) : (a < c ? c : a); }

    // The product meter's 4× interpolator (TruePeakEstimator, TruePeak.h — the
    // dBTP display and the limiter's true-peak detector): 12 taps per phase,
    // a Blackman-windowed sinc designed here rather than copied from a table.
    // Index k multiplies x[n−k]; phase 0 is unused (the sample itself is the
    // seed); phases 1..3 read the points 5.75 / 5.5 / 5.25 samples back, i.e.
    // between x[n−6] and x[n−5]. The clamp's detector evaluates these SAME
    // phases, so the meter reading is one of the readings it holds.
    inline constexpr int kMeterTaps   = 12;
    inline constexpr int kMeterPhases = 4;

    inline float meterSinc (float t) noexcept
    {
        if (std::abs (t) < 1.0e-6f) return 1.0f;
        const float pt = kPi * t;
        return std::sin (pt) / pt;
    }

    inline void designMeterPhases (float (&c)[kMeterPhases][kMeterTaps]) noexcept
    {
        for (int p = 1; p < kMeterPhases; ++p)
        {
            // Fractional delays 5.75 / 5.5 / 5.25: the points between
            // x[n−6] and x[n−5].
            const float d = 6.0f - (float) p / (float) kMeterPhases;
            float sum = 0.0f;
            for (int k = 0; k < kMeterTaps; ++k)
            {
                const float u = ((float) k - d + 6.0f) / (float) kMeterTaps;   // window position
                const float wnd = (u <= 0.0f || u >= 1.0f) ? 0.0f
                    : 0.42f - 0.5f * std::cos (kTwoPi * u)
                            + 0.08f * std::cos (2.0f * kTwoPi * u);
                c[p][k] = wnd * meterSinc ((float) k - d);
                sum += c[p][k];
            }
            // Exact unity DC response — a plain windowed sinc is a hair off,
            // and that hair would be straight passband error.
            for (int k = 0; k < kMeterTaps; ++k)
                c[p][k] /= sum;
        }
    }
} // namespace truepeak

// ============================================================================
//  ClampTruePeakDetector — the ceiling clamp's detector (ADR-0041).
//
//  WHY NOT TruePeakEstimator (TruePeak.h). That one is a 4× measurement tap
//  sized for the limiter's detector and the meter: 12 taps under a Blackman
//  window. It is
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
                const double pd = truepeak::kPiD * d;
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
        float std4x[truepeak::kMeterPhases][truepeak::kMeterTaps] = {};
        truepeak::designMeterPhases (std4x);
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
        const int nCh = truepeak::min2 (numCh, kMaxChannels);
        if (nCh <= 0)
            return;                                    // nothing to read (and w[0] would stay null)
        const float* w[kMaxChannels] = {};
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto& h = hist[ch];
            h[(std::size_t) writeIdx] = x[ch];
            h[(std::size_t) (writeIdx + kTaps)] = x[ch];
            w[ch] = &h[(std::size_t) (writeIdx + 1)];               // oldest → newest
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
            float meterMax = truepeak::max3 (std::abs (mh[ch]), std::abs (ms[ch] + md[ch]), std::abs (ms[ch] - md[ch]));
            meterMax = truepeak::max3 (meterMax, std::abs (os[ch] + od[ch]), std::abs (os[ch] - od[ch]));
            meterMax = truepeak::max3 (meterMax, std::abs (is[ch] + id[ch]), std::abs (is[ch] - id[ch]));

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
            const float refined = truepeak::max2 (std::abs (g[m]), refinedPeak (g[m - 1], g[m], g[m + 1]));
            tpOut[ch] = truepeak::max2 (refined, meterMax);
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
        return truepeak::min2 (vertex, mag * 1.0592537f);  // +0.5 dB
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

    // Non-copyable, spelled out: this header includes no JUCE module (the
    // banner), so the usual ownership macro is not available here.
public:
    ClampTruePeakDetector (const ClampTruePeakDetector&) = delete;
    ClampTruePeakDetector& operator= (const ClampTruePeakDetector&) = delete;
};

} // namespace anabasis
