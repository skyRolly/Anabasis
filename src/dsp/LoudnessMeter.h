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
//  - INTEGRATED, UNGATED — the BS.1770-**1** reading (0.1.1, ADR-0020): the
//    plain mean energy of every gating block, no absolute gate and no
//    relative gate, because -1 defined no gating at all; the two-stage gate
//    arrived in BS.1770-**2** and is what every later revision kept. Both
//    figures are computed always and published side by side; WHICH ONE THE
//    USER SEES is a Settings choice resolved on the message thread, so the
//    meter itself carries no preference.
//  - LOUDNESS RANGE (LRA, EBU Tech 3342, 0.1.1): a second fixed-size
//    histogram over SHORT-TERM values sampled at the 100 ms sub-block
//    cadence (far above the standard's ≥ 1 Hz), absolute-gated at −70 LUFS,
//    then relative-gated at (mean − **20 LU** — LRA's threshold, not the
//    integrated reading's −10), and read as the 95th minus the 10th
//    percentile of what survives.
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
        integratedFrom = 0;
        for (auto& s : subRing) s = 0.0;
        clearSessionCumulative();
    }

    // Invariant 9, the unconditional half — called once per block by the
    // engine rather than from its recovery flag, for the same reason the
    // limiter's detector state is: this meter poisons itself from a FINITE
    // input and emits no audio at all, so no boundary can see it. `dryMeter`
    // is fed the raw delay-aligned input, which the engine keeps finite but
    // does not bound, and the K-weighting shelf overflows on it (|b1| ≈ 2.7,
    // so ~FLT_MAX/3 is enough). The TDF-II states then hold inf, the next
    // sample makes them NaN, and every reading is NaN for the rest of the
    // session: `momentaryLufs()`, the §2.7 gate that compares it against
    // −70 LUFS (false for NaN, so the compensation freezes), and the
    // published M/S/I.
    //
    // Only the K-weighting states and the sub-block accumulator are repaired.
    // The ring needs no scan because finishSubBlock() keeps a non-finite mean
    // out of it in the first place (see there — the reason is how long a
    // stored one would linger, not the integrated reading). A
    // finite-but-astronomical block DOES enter the histogram and biases the
    // integrated reading for the session; that is the meter correctly
    // recording an absurd measurement, and the P5 meter reset is its escape
    // hatch, not a repair belonging here.
    void sanitiseState() noexcept
    {
        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            if (! std::isfinite (s1z1[ch]) || ! std::isfinite (s1z2[ch]))
                s1z1[ch] = s1z2[ch] = 0.0f;
            if (! std::isfinite (s2z1[ch]) || ! std::isfinite (s2z2[ch]))
                s2z1[ch] = s2z2[ch] = 0.0f;
            if (! std::isfinite (subAccum[ch]))
                subAccum[ch] = 0.0;
        }
    }

    // The SESSION-CUMULATIVE half only: the gated histogram and its block
    // count, i.e. what integratedLufs() reads. The sliding windows keep
    // running — momentary and short-term are rolling measurements with
    // nothing session-scoped in them, and clearing them would blank the
    // display for 3 s to answer a request about the integrated figure.
    // Called from the audio thread (the P5 meter-reset request lands at the
    // top of processBlock): bounded stores, no allocation.
    void resetIntegrated() noexcept
    {
        clearSessionCumulative();
        // The watermark is the half that is easy to miss: gating blocks are
        // assembled from the last FOUR 100 ms sub-blocks, so the first ones
        // committed after this call straddle up to 300 ms of PRE-reset
        // material. Without the watermark, a reset issued during loud
        // playback puts one loud straddling block into the fresh histogram —
        // and the −10 LU relative gate then excludes every quieter block
        // measured after it, so the "reset" integrated figure reads the OLD
        // programme's loudness for the rest of the session. Only gating
        // blocks whose four sub-blocks all post-date the reset may enter.
        //
        // The +1 is the sub-block IN PROGRESS, and it is the whole difference
        // between the rule above and the rule this used to implement: at this
        // instant `subCount` sub-blocks are complete and sub-block number
        // `subCount` is accumulating with `subFill` pre-reset samples already
        // in it (deliberately not cleared — dropping them would notch the
        // rolling windows). A watermark of subCount + 4 admits the gating block
        // at subCount + 4, which averages sub-blocks subCount+3 … subCount —
        // the straddling one included, i.e. up to 100 ms of the old programme
        // at a quarter of the block's energy, which is exactly the bias the
        // watermark exists to prevent. Land the reset on a sub-block boundary
        // (subFill == 0) and there is no straddler, so +4 is right there.
        integratedFrom = subCount + 4 + (subFill > 0 ? 1 : 0);
        // The LRA watermark is the SAME rule at the short-term window's
        // length: an LRA sample IS a 3 s window, so the first one carrying no
        // pre-reset material is 30 sub-blocks out, plus the straddler. Getting
        // this wrong would not merely bias the number — one retained loud
        // pre-reset short-term value sets the 95th percentile for the rest of
        // the session, and LRA has no averaging to dilute it.
        lraFrom = subCount + 30 + (subFill > 0 ? 1 : 0);
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

    // CACHED, and the reason is a real audio-thread cost. `computeIntegratedLufs`
    // walks the 751-bin histogram TWICE (~1500 iterations) and `computeLraLu`
    // walks it three times (~2250), while the wrapper reads both once per
    // `processBlock`. At 48 kHz/512 that is 94 blocks/s and invisible; at
    // 192 kHz with 32-sample buffers it is ~6000 blocks/s × ~3750 iterations
    // ≈ 22 M/s, which is the same order as DESIGN §9's entire ≤ 0.5 % metering
    // allocation — a meter whose cost scales with the host's buffer size, the
    // very shape `RmsMeter`'s 10 ms cadence was chosen to avoid.
    //
    // Both figures are pure functions of the session-cumulative accumulators,
    // and those change in exactly TWO places — `finishSubBlock` (a gating
    // block commits, at most once per 100 ms) and `clearSessionCumulative`
    // (every reset path routes through it). So a second call inside the same
    // sub-block cannot observe different inputs, and the cached value is
    // BIT-IDENTICAL to the recomputed one: this is not an approximation, and
    // the update timing does not move. The walks become 10 Hz.
    //
    // `mutable`, and deliberately NOT atomic: every reader of these two and
    // every writer of the accumulators is on the AUDIO thread — the publish at
    // the end of `processBlock`, the gating-block commit inside it, and the
    // meter-reset consume at its top. `momentaryLufs`/`shortTermLufs` are the
    // readings called from elsewhere (the engine's §2.7 compensation) and are
    // deliberately NOT cached: they walk 4 and 30 sub-blocks, and they move
    // every sub-block anyway, so a cache would pay for itself with nothing.
    // (They are not thread-safe either — nothing on this class is — but they
    // only READ audio-thread state, where these two also WRITE it, which is
    // what makes `const` misleading here. `AnabasisEngine::outputLoudness()`
    // hands out a public `const LoudnessMeter&`, so the compiler will not stop
    // a GUI-side reader from being added through it: the invariant is recorded
    // in THREAD_MODEL.md §"Audio-thread-only state behind a `const` accessor",
    // together with the published atomics such a reader should use instead.)
    float integratedLufs() const noexcept
    {
        if (! integratedValid)
        {
            integratedCache = computeIntegratedLufs();
            integratedValid = true;
        }
        return integratedCache;
    }

    // The BS.1770-1 reading: the plain mean energy of every gating block with
    // NO gate of either kind. It is a separate accumulator rather than a
    // second pass over the histogram because the histogram never sees the
    // sub-−70 blocks at all — the absolute gate is applied at INSERT, so the
    // ungated figure is not recoverable from it. Two doubles and a counter.
    float integratedUngatedLufs() const noexcept
    {
        if (ungatedCount == 0)
            return kSilentLufs;
        return (float) energyToLufs (ungatedSum / (double) ungatedCount);
    }

    // LOUDNESS RANGE in LU (EBU Tech 3342). `0` is a legitimate reading (a
    // perfectly steady programme), so "not measured yet" is the negative
    // sentinel — a range cannot be negative, and the callers that display it
    // test for it rather than for a count they cannot see.
    static constexpr float kNoLra = -1.0f;

    float lraLu() const noexcept
    {
        if (! lraValid)
        {
            lraCache = computeLraLu();
            lraValid = true;
        }
        return lraCache;
    }

private:
    float computeIntegratedLufs() const noexcept
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

    float computeLraLu() const noexcept
    {
        if (lraCount == 0)
            return kNoLra;
        // Pass 1: the relative threshold, 20 LU below the mean of everything
        // that cleared the absolute gate.
        const double relThreshold = energyToLufs (lraSum / (double) lraCount) - 20.0;
        const int firstBin = juce::jlimit (0, kBins - 1,
                                           (int) std::ceil ((relThreshold - kBinFloor) / kBinWidth));
        int64_t total = 0;
        for (int b = firstBin; b < kBins; ++b)
            total += lraHist[(size_t) b];
        if (total < 2)
            return kNoLra;               // a single surviving value has no range

        // Pass 2: the 10th and 95th percentiles of the surviving distribution,
        // by cumulative count. Bin CENTRES are the reported values, so the
        // figure is quantised to the 0.1 LU bin width — the same bound the
        // integrated reading carries, and the same reason: a fixed-size
        // accumulator is the only allocation-free way to hold a distribution
        // whose length is the session's.
        const auto percentileLufs = [&] (double fraction) noexcept
        {
            const int64_t want = (int64_t) std::ceil (fraction * (double) total);
            int64_t cum = 0;
            for (int b = firstBin; b < kBins; ++b)
            {
                cum += lraHist[(size_t) b];
                if (cum >= juce::jmax<int64_t> (1, want))
                    return kBinFloor + ((double) b + 0.5) * kBinWidth;
            }
            return kBinFloor + ((double) (kBins - 1) + 0.5) * kBinWidth;
        };
        return (float) juce::jmax (0.0, percentileLufs (0.95) - percentileLufs (0.10));
    }

    // The four session-cumulative accumulators, cleared as ONE unit — they are
    // fed together and every caller that clears one must clear all four, which
    // is exactly the kind of rule that rots when it is written twice.
    void clearSessionCumulative() noexcept
    {
        for (auto& c : histCount) c = 0;
        for (auto& s : histSum) s = 0.0;
        totalGatedBlocks = 0;
        ungatedSum = 0.0;
        ungatedCount = 0;
        for (auto& c : lraHist) c = 0;
        lraSum = 0.0;
        lraCount = 0;
        lraFrom = 0;
        invalidateReadings();
    }

    // The two cached readings are functions of the accumulators above and of
    // nothing else, so this belongs beside every write to them — here, and at
    // the end of `finishSubBlock`, which is the only other writer. Both are
    // unconditional: a `finishSubBlock` that commits no gating block clears a
    // still-valid cache, which costs one recompute at 10 Hz and cannot be
    // wrong, where a conditional invalidation has to stay in step with three
    // separate insert branches and would rot the first time one moves.
    void invalidateReadings() const noexcept
    {
        integratedValid = lraValid = false;
    }

    void finishSubBlock() noexcept
    {
        double frameSum = 0.0;
        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            frameSum += subAccum[ch];          // stereo weights 1.0
            subAccum[ch] = 0.0;
        }
        // A non-finite sub-block mean is dropped to 0.0 rather than stored, and
        // the reason is RECOVERY LATENCY, not correctness of the integrated
        // reading: the gated histogram rejects a NaN anyway (`lufs >= -70.0` is
        // false for it), but the sliding windows do not — one stored NaN makes
        // momentary and short-term read NaN until it ages out, which is up to
        // kSubRing sub-blocks ≈ 3.2 s, and for all of that time the §2.7
        // compensation's `> -70 LUFS` gate is false and the compensation is
        // frozen. 0.0 is below the absolute gate, so the block is simply not
        // counted, and it pulls a 30-entry window down by ~0.15 dB while it
        // sits there. Seconds of a frozen monitor gain against a tenth of a dB
        // for one window: the trade is not close.
        const double subMean = frameSum / (double) subBlockLen;
        subRing[(size_t) (subCount % kSubRing)] = std::isfinite (subMean) ? subMean : 0.0;
        ++subCount;
        subFill = 0;

        // A gating block exists once four sub-blocks have accumulated; its
        // mean square is the mean of the last four sub-block means.
        if (subCount >= 4 && subCount >= integratedFrom)
        {
            double z = 0.0;
            for (int k = 0; k < 4; ++k)
                z += subRing[(size_t) ((subCount - 1 - k) % kSubRing)];
            z *= 0.25;
            const double lufs = energyToLufs (z);
            // BS.1770-1: every gating block, no gate of either kind. Fed here
            // rather than beside the histogram insert so the ungated figure
            // cannot silently inherit a gate someone adds to that branch.
            // Non-finite energies are excluded — `finishSubBlock` keeps them
            // out of the ring, so `z` can only be non-finite if a stored
            // finite value overflowed the sum, and one such block would make
            // the mean NaN for the session with no gate to reject it.
            if (std::isfinite (z))
            {
                ungatedSum += z;
                ++ungatedCount;
            }
            if (lufs >= -70.0)                 // absolute gate
            {
                // The clamp is a HARD RANGE LIMIT, not a rounding detail:
                // every gating block above +5 LUFS lands in the top bin. Pass 1
                // stays exact (it sums the ENERGIES, not bin centres), but
                // `firstBin` in integratedLufs() is derived from the relative
                // threshold and clamps the same way, so a pass-1 mean above
                // +15 LUFS leaves only the top bin surviving pass 2.
                // Unreachable through the render tap, which is ceiling-bounded,
                // and reachable for the dry/wet meters only on input no DAW
                // produces — but it is the constraint to remember before
                // pointing a meter at an unbounded tap.
                const int bin = juce::jlimit (0, kBins - 1,
                                              (int) ((lufs - kBinFloor) / kBinWidth));
                ++histCount[(size_t) bin];
                histSum[(size_t) bin] += z;
                ++totalGatedBlocks;
            }
        }

        // LRA sample (EBU Tech 3342): one SHORT-TERM value per sub-block, so
        // 10 Hz — far above the standard's ≥ 1 Hz floor, and free, because
        // `windowLoudness` is the same 30-entry walk the display already does.
        // Its own watermark, not `integratedFrom`: a short-term window reaches
        // ten times further back, so sharing the integrated one would admit
        // 2.9 s of pre-reset programme.
        if (subCount >= 30 && subCount >= lraFrom)
        {
            const float st = windowLoudness (30);
            if (st >= -70.0f && std::isfinite (st))       // the absolute gate
            {
                const int bin = juce::jlimit (0, kBins - 1,
                                              (int) (((double) st - kBinFloor) / kBinWidth));
                ++lraHist[(size_t) bin];
                // The relative threshold is computed in the ENERGY domain like
                // the integrated reading's, so the sum accumulates energies —
                // averaging the dB values instead would answer a different
                // question and would not match the integrated gate's grammar.
                lraSum += lufsToEnergy (st);
                ++lraCount;
            }
        }
        invalidateReadings();          // see the note beside the definition
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

    // The exact inverse of the line above, for the LRA gate's energy-domain
    // mean. Written as the inverse rather than re-deriving the constant so a
    // change to one is a compile-visible mismatch with the other.
    static double lufsToEnergy (double lufs) noexcept
    {
        return std::pow (10.0, (lufs + 0.691) * 0.1);
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
    int64_t integratedFrom = 0;   // resetIntegrated watermark (see there)
    int32_t histCount[kBins] = {};
    double  histSum[kBins] = {};
    int64_t totalGatedBlocks = 0;

    // BS.1770-1 (ungated) — two scalars, no histogram: with no gate there is
    // no threshold to re-derive later, so the running mean IS the answer.
    double  ungatedSum = 0.0;
    int64_t ungatedCount = 0;

    // LRA (Tech 3342): counts only. The energy sum lives beside it for the
    // relative gate; the percentiles need the distribution, which is what the
    // bins are, and nothing needs the per-bin energy the integrated histogram
    // carries — so this one is ~3 KB against that one's ~9 KB.
    int64_t lraFrom = 0;
    int32_t lraHist[kBins] = {};
    double  lraSum = 0.0;
    int64_t lraCount = 0;

    // The two histogram-walk readings, held between gating blocks. Seeded
    // INVALID rather than at a sentinel value, so the first call computes:
    // a seeded value would be a second copy of `kSilentLufs`/`kNoLra` to keep
    // in step with the compute functions that already return them.
    mutable bool  integratedValid = false, lraValid = false;
    mutable float integratedCache = kSilentLufs, lraCache = kNoLra;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessMeter)
};

} // namespace anabasis
