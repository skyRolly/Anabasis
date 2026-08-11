#pragma once

// ============================================================================
//  StageTrace — KI-009's per-stage, per-channel diagnostic tap.
//
//  COMPILED OUT UNLESS `ANABASIS_STAGE_TRACE` IS DEFINED. With the flag off the
//  macro below expands to `((void) 0)` and nothing here is instantiated, so the
//  shipped plugin carries no trace code, no storage and no branch. The flag is
//  set by exactly one thing: `-DANABASIS_STAGE_TRACE=ON` on a DIAGNOSTIC build,
//  which CI produces on the native-Intel macOS runner.
//
//  WHY IT EXISTS. KI-009 reproduces on macOS x86_64 and on nothing else the
//  project can run: not arm64, not Linux, not the standalone. Every oracle used
//  so far reads the plugin's OUTPUT, which answers "did a channel die" and not
//  "where". This answers where: it accumulates per-channel energy at each tap of
//  the chain, so the first tap at which channel 0 stops matching channel 1 is
//  visible directly instead of inferred.
//
//  It is deliberately NOT realtime-safe-by-luck: the accumulators are plain
//  doubles written from the audio thread and read at `releaseResources`, which
//  is a message-thread call made after processing has stopped. That is sound for
//  a diagnostic build and is not offered as a pattern for shipping code —
//  REALTIME_AUDIO_POLICY still governs everything the plugin actually ships.
// ============================================================================

#if ANABASIS_STAGE_TRACE

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <cstdio>

namespace anabasis
{

struct StageTrace
{
    // The chain, in order. Adding a tap means adding a name here and one
    // ANABASIS_TRACE call: the two lists are meant to be read side by side.
    enum Tap
    {
        rawIn = 0, postEqPre, compOut, regionIn, clipOut, wetRingWrite,
        detectorTap, limiterGain, delayedWet, postLimiter, stageEIn, finalOut,
        kTaps
    };

    static constexpr int kMaxCh = 2;

    static const char* tapName (int t) noexcept
    {
        static const char* names[kTaps] = {
            "raw in", "post EQ-pre", "comp out", "region in", "clip out",
            "wet ring write", "detector tap", "limiter gain", "delayed wet",
            "post limiter", "stage E in", "final out"
        };
        return (t >= 0 && t < kTaps) ? names[t] : "?";
    }

    // The configuration this window belongs to. Kept HERE rather than in the
    // processor so that enabling the trace adds no member to a shipping class:
    // with the flag off this whole struct does not exist. `prepareToPlay` is
    // the only writer, and it writes through ANABASIS_TRACE_CONFIGURE.
    double preparedRate = 0.0;
    int preparedBlock = 0, preparedChannels = 0;

    double sumSq[kTaps][kMaxCh] = {};
    long long count[kTaps][kMaxCh] = {};
    long long nonFinite[kTaps][kMaxCh] = {};
    long long exactZero[kTaps][kMaxCh] = {};

    void add (int tap, int ch, float v) noexcept
    {
        if (tap < 0 || tap >= kTaps || ch < 0 || ch >= kMaxCh)
            return;
        if (! std::isfinite (v))      ++nonFinite[tap][ch];
        else                          sumSq[tap][ch] += (double) v * v;
        // Deliberately a BIT-EXACT comparison, not a tolerance: the question is
        // "did the stage write literal zero", which is what a dead channel looks
        // like and what a merely quiet one does not. juce::exactlyEqual says so
        // to the compiler as well as to the reader (-Wfloat-equal is on).
        if (juce::exactlyEqual (v, 0.0f)) ++exactZero[tap][ch];
        ++count[tap][ch];
    }

    void reset() noexcept
    {
        const auto rate = preparedRate;
        const auto block = preparedBlock;
        const auto chans = preparedChannels;
        *this = StageTrace{};
        preparedRate = rate; preparedBlock = block; preparedChannels = chans;
    }

    void configure (double rate, int block, int chans) noexcept
    {
        preparedRate = rate; preparedBlock = block; preparedChannels = chans;
    }

    // Printed to stderr so it interleaves with, but is separable from, whatever
    // the host is writing to stdout.
    bool empty() const noexcept
    {
        for (int t = 0; t < kTaps; ++t)
            for (int ch = 0; ch < kMaxCh; ++ch)
                if (count[t][ch] != 0)
                    return false;
        return true;
    }

    void dump (const char* label) const
    {
        // Silence, not an empty table: the dump fires from two places (see
        // AnabasisAudioProcessor) precisely so that neither can miss a window,
        // which means one of them routinely has nothing to say.
        if (empty())
            return;

        std::fprintf (stderr, "\n=== ANABASIS STAGE TRACE (%s) %.0f Hz / %d smp / %d ch ===\n",
                      label, preparedRate, preparedBlock, preparedChannels);
        std::fprintf (stderr, "%-16s %14s %14s %10s  %s\n",
                      "tap", "rms L", "rms R", "L/R dB", "notes");
        for (int t = 0; t < kTaps; ++t)
        {
            if (count[t][0] == 0 && count[t][1] == 0)
                continue;
            double rms[kMaxCh] = { 0.0, 0.0 };
            for (int ch = 0; ch < kMaxCh; ++ch)
                rms[ch] = count[t][ch] > 0 ? std::sqrt (sumSq[t][ch] / (double) count[t][ch]) : 0.0;

            const double ratioDb = (rms[0] > 0.0 && rms[1] > 0.0)
                                       ? 20.0 * std::log10 (rms[0] / rms[1])
                                       : (rms[1] > 0.0 ? -999.0 : 999.0);

            char notes[160] = {};
            std::snprintf (notes, sizeof (notes), "%s%s%s",
                           (nonFinite[t][0] || nonFinite[t][1]) ? "NON-FINITE " : "",
                           (rms[0] <= 0.0 && rms[1] > 0.0) ? "<<< CHANNEL 0 DEAD HERE " : "",
                           (rms[1] <= 0.0 && rms[0] > 0.0) ? "<<< CHANNEL 1 DEAD HERE " : "");

            std::fprintf (stderr, "%-16s %14.9f %14.9f %10.2f  %s\n",
                          tapName (t), rms[0], rms[1], ratioDb, notes);
        }
        std::fprintf (stderr, "=== END STAGE TRACE ===\n\n");
        std::fflush (stderr);
    }
};

// One instance for the process. A host may run several plugin instances, but the
// diagnostic build is driven by the probe, which runs one at a time.
inline StageTrace& trace() noexcept
{
    static StageTrace t;
    return t;
}

} // namespace anabasis

 #define ANABASIS_TRACE(tap, ch, v) ::anabasis::trace().add ((tap), (ch), (v))
 #define ANABASIS_TRACE_DUMP(label)  ::anabasis::trace().dump (label)
 #define ANABASIS_TRACE_RESET()      ::anabasis::trace().reset()
 #define ANABASIS_TRACE_CONFIGURE(rate, block, chans) \
     ::anabasis::trace().configure ((rate), (block), (chans))

#else

 #define ANABASIS_TRACE(tap, ch, v) ((void) 0)
 #define ANABASIS_TRACE_DUMP(label) ((void) 0)
 #define ANABASIS_TRACE_RESET()     ((void) 0)
 #define ANABASIS_TRACE_CONFIGURE(rate, block, chans) ((void) 0)

#endif
