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
//  it. Hann is symmetric, so which end of the ring carries `w[0]` does not
//  change the result; the walk below runs newest-first for cache reasons only.
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
    static constexpr float kSilentDb    = -144.0f;   // "nothing measured yet"

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
    // absent one.
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
        publishedDb = ms > 1.0e-15
                        ? (float) (10.0 * std::log10 (ms))   // 10·log10 of a MEAN SQUARE
                        : kSilentDb;
    }

    std::vector<float> sq, window;
    double invWindowSum = 0.0;
    int    len = 0, head = 0, filled = 0;
    int    refreshLen = 480, sinceRefresh = 0;
    float  publishedDb = kSilentDb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RmsMeter)
};

} // namespace anabasis
