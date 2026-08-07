#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>

// ============================================================================
//  RmsMeter — the §2.9 Waveform-Statistics RMS reading: a 50 ms HANN-WINDOWED
//  sliding RMS (0.1.1, owner directive; ADR-0020).
//
//  Why a new class rather than a reading off something already here:
//  - `LoudnessMeter` is K-WEIGHTED and integrates over 100 ms rectangular
//    sub-blocks. Its numbers are loudness, not level; they cannot answer
//    "what is the RMS level of this programme?".
//  - `MasteringComp`'s RMS is a 10 ms one-pole DETECTOR, sized for ballistics
//    and living inside the audio path's sidechain. Reading a meter off it
//    would tie the display to a compressor's time constant.
//
//  Window. `w[n] = 0.5 - 0.5·cos(2πn/(N-1))` over N = 50 ms of samples, and
//  the reading is `sqrt(Σ w·x² / Σ w)` — normalised by the window's own sum so
//  a stationary signal reads its true RMS rather than the window's average of
//  it. The walk pairs `w[0]` with the OLDEST entry and runs forward in time, so
//  the window's peak lands on the middle-aged sample: a CENTRED window. Two
//  consequences worth stating rather than discovering. (1) The reading lags the
//  signal by half a window — ~25 ms — which is the ordinary behaviour of a
//  windowed RMS meter and is invisible against the 42 ms display refresh. (2)
//  Hann being symmetric, a reversed walk would give the same answer for a
//  stationary signal, which is exactly why the tests below cannot catch a
//  reversal: they use stationary stimuli because those are the ones with a
//  closed-form level. The centring is a design statement here, not a test
//  claim.
//
//  Stereo. The frame's MEAN square across channels, so a correlated signal at
//  full scale reads the same on stereo as it would on one channel — the
//  reading an engineer expects from "RMS level", and the same convention the
//  sample-peak and true-peak rows use (both are max-of-channels of a quantity
//  that is already per-channel comparable).
//
//  Reference. This class publishes the MATHEMATICAL RMS in dB
//  (`20·log10(rms)`), under which a full-scale sine reads −3.01 dBFS. The
//  AES-17 convention (+3.01 dB, so a full-scale sine reads 0 dBFS) is applied
//  at DISPLAY time from the Settings choice — the meter must not carry a UI
//  preference, and the offset is exact, so nothing is lost by deferring it.
//
//  Cost, bounded and stated. The windowed sum is recomputed at most once per
//  10 ms of audio (the display refreshes at 24 Hz — ~42 ms — so a finer
//  cadence would not be visible), and it costs N multiply-adds. N is 50 ms of
//  samples and the cadence is 10 ms of samples, so the amortised cost is
//  0.05·sr / 0.01·sr = **5 multiply-adds per sample at every sample rate and
//  every block size** — the property a per-block recompute does not have (at
//  192 kHz with 32-sample blocks that would be 300/sample).
//
//  Audio-thread: allocation-free after prepare(); the two vectors are sized
//  there and never resized (REALTIME_AUDIO_POLICY).
// ============================================================================

namespace anabasis
{

class RmsMeter
{
public:
    static constexpr int   kMaxChannels = 2;

    // TWO constants, and their separation is the point. `kSilentDb` is a
    // SENTINEL — "nothing measured yet", no full window has been seen or the
    // state was sanitised — and it is not a level. `kFloorDb` is the lowest
    // LEVEL this meter reports: every computed reading is clamped up to it, so
    // exact digital silence (whose logarithm does not exist) still comes back
    // as a measurement rather than as an absence. A reader distinguishes the
    // two by comparing against `kFloorDb`, not by a tolerance around the
    // sentinel.
    //
    // They must not overlap, which one constant could not guarantee: a floor
    // applied to the mean square rather than to the dB reading let the computed
    // range run to 10·log10(1e-15) = −150 dB, BELOW the sentinel, so a real
    // reading of a near-silent passage could equal or undercut "nothing
    // measured yet" — a distinction no consumer could then recover.
    static constexpr float kSilentDb    = -144.0f;
    static constexpr float kFloorDb     = -140.0f;

    static_assert (kFloorDb > kSilentDb,
                   "the sentinel must sit strictly below every reading this meter can compute");

    RmsMeter() = default;

    void prepare (double sampleRate)
    {
        // The floor of 2 keeps the Hann denominator (N−1) non-zero; no real
        // rate reaches it (50 ms is 2 samples at 40 Hz).
        const int n = juce::jmax (2, (int) std::lround (0.050 * sampleRate));
        sq.assign ((size_t) n, 0.0f);
        window.resize ((size_t) n);
        double wSum = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi
                                                   * (double) i / (double) (n - 1));
            window[(size_t) i] = (float) w;
            wSum += w;
        }
        // Guard the normaliser rather than trusting the analytic value
        // ((N−1)/2): at N = 2 the window is {0, 0} and the sum IS zero, which
        // would make every reading NaN instead of silent.
        invWindowSum = wSum > 1.0e-12 ? 1.0 / wSum : 0.0;
        len          = n;
        refreshLen   = juce::jmax (1, (int) std::lround (0.010 * sampleRate));
        reset();
    }

    void reset() noexcept
    {
        for (auto& s : sq)
            s = 0.0f;
        head = 0;
        sinceRefresh = 0;
        filled = 0;
        publishedDb = kSilentDb;
    }

    // Invariant 9's unconditional half, same contract as `LoudnessMeter`'s:
    // this meter is fed a tap and emits no audio, so no boundary can see it
    // poison itself. A non-finite square would make every later windowed sum
    // non-finite for the whole 50 ms it lingers, and `publishedDb` with it.
    void sanitiseState() noexcept
    {
        if (std::isfinite (publishedDb))
            return;                        // the common case: one compare
        for (auto& s : sq)
            if (! std::isfinite (s))
                s = 0.0f;
        publishedDb = kSilentDb;
    }

    void processFrame (const float* x, int numCh) noexcept
    {
        if (len <= 0)
            return;                        // never prepared
        const int nCh = juce::jmax (1, juce::jmin (numCh, kMaxChannels));
        float meanSq = 0.0f;
        for (int ch = 0; ch < nCh; ++ch)
            meanSq += x[ch] * x[ch];
        meanSq /= (float) nCh;
        // A non-finite frame is stored as 0 rather than dropped: dropping it
        // would leave the PREVIOUS occupant of the slot in the window, which
        // is stale data presented as current. `LoudnessMeter::finishSubBlock`
        // makes the same trade for the same reason.
        sq[(size_t) head] = std::isfinite (meanSq) ? meanSq : 0.0f;
        head = head + 1 < len ? head + 1 : 0;
        if (filled < len)
            ++filled;

        if (++sinceRefresh >= refreshLen)
        {
            sinceRefresh = 0;
            recompute();
        }
    }

    // dB re full scale, MATHEMATICAL reference (a full-scale sine reads
    // −3.01). `kSilentDb` until the first full 50 ms window has been seen —
    // a partially filled window would read low by the fraction still empty,
    // which on a transport start is a visibly wrong number rather than an
    // absent one. Once one has been seen the value is a MEASUREMENT and is
    // `>= kFloorDb`, silence included; `rmsDb() < kFloorDb` is the exact test
    // for "no reading".
    float rmsDb() const noexcept { return publishedDb; }

private:
    void recompute() noexcept
    {
        if (filled < len)
            return;
        double acc = 0.0;
        int idx = head;                    // the OLDEST entry: head has wrapped past it
        for (int i = 0; i < len; ++i)
        {
            acc += (double) window[(size_t) i] * (double) sq[(size_t) idx];
            idx = idx + 1 < len ? idx + 1 : 0;
        }
        const double ms = acc * invWindowSum;
        // `ms == 0` is EXACT DIGITAL SILENCE, and it is a MEASUREMENT: the
        // window was full, the walk ran, and the answer is "below what this
        // meter resolves" — so it reports the floor, not the sentinel. `ms`
        // cannot be negative (squares are non-negative, and so is a Hann
        // window), so the only other case is an accumulation that is not a
        // number at all, which is an absent reading and matches what
        // `sanitiseState` publishes.
        if (! std::isfinite (ms))
            publishedDb = kSilentDb;
        else
            publishedDb = ms > 0.0                           // 10·log10 of a MEAN SQUARE
                            ? juce::jmax (kFloorDb, (float) (10.0 * std::log10 (ms)))
                            : kFloorDb;
    }

    std::vector<float> sq, window;
    double invWindowSum = 0.0;
    int    len = 0, head = 0, filled = 0;
    int    refreshLen = 480, sinceRefresh = 0;
    float  publishedDb = kSilentDb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RmsMeter)
};

} // namespace anabasis
