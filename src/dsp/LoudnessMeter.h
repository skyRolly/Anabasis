#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

// ============================================================================
//  LoudnessMeter — BS.1770-4 / EBU R128 loudness, M / S / I with gating.
//
//  K-weighting stage adapted from the sibling product under ADR-0009
//  (Anamorph:src/dsp/LoudnessMatch.cpp:16-46 — the sample-rate-adaptive
//  pre-warped biquad design that reproduces the standard's 48 kHz reference
//  response at any rate). Provenance: RollyTech first-party code.
//
//  Structure (DESIGN §2.9):
//  - K-weighted mean square per channel, stereo weights 1.0/1.0.
//  - 100 ms sub-blocks → every 400 ms gating block is the sum of the last
//    four (75 % overlap, a new gating block every 100 ms, as the standard
//    prescribes).
//  - MOMENTARY = the newest 400 ms gating block; SHORT-TERM = the last 3 s
//    (30 sub-blocks).
//  - INTEGRATED with the two-stage gate: blocks below −70 LUFS absolute are
//    dropped; the relative threshold is (mean of surviving blocks − 10 LU);
//    the result is the mean of blocks above it. Implemented as a FIXED-SIZE
//    histogram accumulator (751 bins of 0.1 LU across −70…+5 LUFS, count +
//    mean-square sum per bin) — REALTIME_AUDIO_POLICY's named consequence
//    for exactly this spot: never a growing per-block container.
//
//  Loudness = −0.691 + 10·log10(Σ_ch z_ch). The standard's own compliance
//  point pins the calibration: a 0 dBFS 997 Hz sine in ONE channel reads
//  −3.01 LKFS (the accuracy test asserts it at ≤ 0.1 LU).
//
//  Audio-thread: allocation-free after prepare(), all state fixed-size.
//  Publication to the GUI is the caller's per-block relaxed-atomic job
//  (THREAD_MODEL); the getters here are plain audio-thread reads.
// ============================================================================

namespace anabasis
{

class LoudnessMeter
{
public:
    static constexpr int   kMaxChannels = 2;
    static constexpr float kSilentLufs  = -100.0f;   // "nothing measured yet"

    LoudnessMeter() = default;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        designKWeighting();
        subBlockLen = juce::jmax (1, (int) std::lround (0.100 * sampleRate));
        reset();
    }

    void reset() noexcept
    {
        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            s1z1[ch] = s1z2[ch] = s2z1[ch] = s2z2[ch] = 0.0f;
            subAccum[ch] = 0.0;
        }
        subCount = 0;
        subFill  = 0;
        for (auto& s : subRing) s = 0.0;
        for (auto& c : histCount) c = 0;
        for (auto& s : histSum) s = 0.0;
        totalGatedBlocks = 0;
    }

    // One frame (all channels of one sample step).
    void processFrame (const float* x, int numCh) noexcept
    {
        const int nCh = juce::jmin (numCh, kMaxChannels);
        for (int ch = 0; ch < nCh; ++ch)
        {
            // K-weighting: shelf then RLB high-pass, TDF-II.
            const float in = x[ch];
            const float y1 = (float) (s1b0 * in + s1z1[ch]);
            s1z1[ch] = (float) (s1b1 * in - s1a1 * y1 + s1z2[ch]);
            s1z2[ch] = (float) (s1b2 * in - s1a2 * y1);
            const float y2 = (float) (s2b0 * y1 + s2z1[ch]);
            s2z1[ch] = (float) (s2b1 * y1 - s2a1 * y2 + s2z2[ch]);
            s2z2[ch] = (float) (s2b2 * y1 - s2a2 * y2);
            subAccum[ch] += (double) y2 * y2;
        }

        if (++subFill >= subBlockLen)
            finishSubBlock();
    }

    // -- readings (LUFS; kSilentLufs when nothing qualifies) -----------------
    float momentaryLufs() const noexcept  { return windowLoudness (4); }
    float shortTermLufs() const noexcept  { return windowLoudness (30); }

    float integratedLufs() const noexcept
    {
        if (totalGatedBlocks == 0)
            return kSilentLufs;
        // Pass 1: mean of everything above the absolute gate (the histogram
        // only ever held those blocks).
        double sum = 0.0; long count = 0;
        for (int b = 0; b < kBins; ++b) { sum += histSum[b]; count += histCount[b]; }
        if (count == 0)
            return kSilentLufs;
        const double relThreshold = energyToLufs (sum / (double) count) - 10.0;
        // Pass 2: mean of blocks above the relative threshold.
        // Bin quantisation, recorded so a future accuracy chase does not re-derive
        // it: blocks inside the bin that STRADDLES the relative threshold are
        // dropped wholesale, so the gate is very slightly over-eager and the
        // reading marginally high. Bounded by the 0.1 LU bin width and one-sided,
        // inside the ≤ 0.1 LU contract the compliance test asserts with margin.
        const int firstBin = juce::jlimit (0, kBins - 1,
                                           (int) std::ceil ((relThreshold - kBinFloor) / kBinWidth));
        sum = 0.0; count = 0;
        for (int b = firstBin; b < kBins; ++b) { sum += histSum[b]; count += histCount[b]; }
        if (count == 0)
            return kSilentLufs;
        return (float) energyToLufs (sum / (double) count);
    }

private:
    void finishSubBlock() noexcept
    {
        double frameSum = 0.0;
        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            frameSum += subAccum[ch];          // stereo weights 1.0
            subAccum[ch] = 0.0;
        }
        subRing[(size_t) (subCount % kSubRing)] = frameSum / (double) subBlockLen;
        ++subCount;
        subFill = 0;

        // A gating block exists once four sub-blocks have accumulated; its
        // mean square is the mean of the last four sub-block means.
        if (subCount >= 4)
        {
            double z = 0.0;
            for (int k = 0; k < 4; ++k)
                z += subRing[(size_t) ((subCount - 1 - k) % kSubRing)];
            z *= 0.25;
            const double lufs = energyToLufs (z);
            if (lufs >= -70.0)                 // absolute gate
            {
                const int bin = juce::jlimit (0, kBins - 1,
                                              (int) ((lufs - kBinFloor) / kBinWidth));
                ++histCount[(size_t) bin];
                histSum[(size_t) bin] += z;
                ++totalGatedBlocks;
            }
        }
    }

    float windowLoudness (int subBlocks) const noexcept
    {
        if (subCount < (int64_t) subBlocks)
            return kSilentLufs;
        double z = 0.0;
        for (int k = 0; k < subBlocks; ++k)
            z += subRing[(size_t) ((subCount - 1 - k) % kSubRing)];
        return (float) energyToLufs (z / (double) subBlocks);
    }

    static double energyToLufs (double z) noexcept
    {
        return -0.691 + 10.0 * std::log10 (juce::jmax (z, 1.0e-12));
    }

    void designKWeighting() noexcept
    {
        // Stage 1: high-shelf "head" (BS.1770), pre-warped constants —
        // Anamorph:src/dsp/LoudnessMatch.cpp:16-46 (ADR-0009).
        {
            const double f0 = 1681.974450955533;
            const double G  = 3.999843853973347;
            const double Q  = 0.7071752369554196;
            const double K  = std::tan (juce::MathConstants<double>::pi * f0 / sr);
            const double Vh = std::pow (10.0, G / 20.0);
            const double Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            s1b0 = (Vh + Vb * K / Q + K * K) / a0;
            s1b1 = 2.0 * (K * K - Vh) / a0;
            s1b2 = (Vh - Vb * K / Q + K * K) / a0;
            s1a1 = 2.0 * (K * K - 1.0) / a0;
            s1a2 = (1.0 - K / Q + K * K) / a0;
        }
        // Stage 2: RLB high-pass. The numerator is [1, −2, 1] UN-normalised
        // against a0 while the denominator is normalised — deliberate, not an
        // oversight: that is BS.1770-4's own coefficient set, whose ~+0.043 dB
        // passband offset at 48 kHz is part of the reference filter.
        // "Fixing" it shifts every LUFS reading by 0.04 LU and breaks the
        // 997 Hz −3.01 LKFS compliance vector the calibration test pins.
        {
            const double f0 = 38.13547087602444;
            const double Q  = 0.5003270373238773;
            const double K  = std::tan (juce::MathConstants<double>::pi * f0 / sr);
            const double a0 = 1.0 + K / Q + K * K;
            s2b0 = 1.0;
            s2b1 = -2.0;
            s2b2 = 1.0;
            s2a1 = 2.0 * (K * K - 1.0) / a0;
            s2a2 = (1.0 - K / Q + K * K) / a0;
        }
    }

    static constexpr int    kSubRing  = 32;      // > 30 needed for short-term
    static constexpr int    kBins     = 751;     // −70…+5 LUFS at 0.1 LU
    static constexpr double kBinFloor = -70.0;
    static constexpr double kBinWidth = 0.1;

    double sr = 48000.0;
    double s1b0 = 1, s1b1 = 0, s1b2 = 0, s1a1 = 0, s1a2 = 0;
    double s2b0 = 1, s2b1 = 0, s2b2 = 0, s2a1 = 0, s2a2 = 0;
    float  s1z1[kMaxChannels] = {}, s1z2[kMaxChannels] = {};
    float  s2z1[kMaxChannels] = {}, s2z2[kMaxChannels] = {};

    double  subAccum[kMaxChannels] = {};
    double  subRing[kSubRing] = {};
    int     subBlockLen = 4800, subFill = 0;
    int64_t subCount = 0;

    // The fixed-size integrated-gating accumulator.
    int32_t histCount[kBins] = {};
    double  histSum[kBins] = {};
    int64_t totalGatedBlocks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessMeter)
};

} // namespace anabasis
