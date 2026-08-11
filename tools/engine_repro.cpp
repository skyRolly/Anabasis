// ============================================================================
//  AnabasisEngineRepro — KI-009 reduced to the ENGINE, with no wrapper, no
//  format, no host and no GUI.
//
//  The channel probe hosts the shipped bundle, which is the right oracle and
//  the wrong debugger: every experiment costs a full plugin link. This drives
//  `anabasis::AnabasisEngine` directly with the probe's own stimulus (a 220 Hz
//  sine on the left, 330 Hz on the right, both at 0.25) and the parameter set
//  the field report names, so the same defect can be reproduced and bisected in
//  seconds.
//
//  It is built ONLY by ANABASIS_BUILD_PROBE and is not part of the release
//  gate: `AnabasisTests` owns the assertions, this owns the reproduction.
//
//  Exit: 0 = both channels survived; 1 = a channel was lost or skewed.
// ============================================================================

#include "AnabasisEngine.h"
#include "StageTrace.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <cstdio>

namespace
{

// One case = one parameter set. The list is ordered so the FIRST failure is
// also the smallest: if the engine's own defaults already lose a channel then
// no parameter is implicated at all, which is a different investigation from
// "the field settings do it".
struct Case { const char* name; bool field; };
const Case kCases[] = { { "engine defaults", false }, { "field settings", true } };

static int runCase (const Case& c)
{
    constexpr double sr = 48000.0;
    constexpr int block = 512;

    anabasis::AnabasisEngine engine;
    engine.prepare (sr, block, 2);

    anabasis::EngineParameters p;
    if (c.field)
    {
        p.clipMix        = 1.0f;
        p.clipDriveDb    = 15.4f;
        p.colourDepth    = 0.7f;
        p.dynTiltDb      = 2.0f;
        p.limGainDb      = 12.6f;
        p.compStereoLink = 0.0f;
        p.stereoLink     = 0.0f;
    }

    juce::AudioBuffer<float> buf (2, block);
    double sumSq[2] = { 0.0, 0.0 };

    const int blocks = 120;
    for (int b = 0; b < blocks; ++b)
    {
        for (int n = 0; n < block; ++n)
        {
            const double t = (double) (b * block + n) / sr;
            buf.setSample (0, n, 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * t));
            buf.setSample (1, n, 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 330.0 * t));
        }
        engine.process (buf, p);

        if (b >= blocks / 2)
            for (int n = 0; n < block; ++n)
                for (int ch = 0; ch < 2; ++ch)
                {
                    const double v = buf.getSample (ch, n);
                    sumSq[ch] += v * v;
                }
    }

    const double settled = (double) (blocks - blocks / 2) * (double) block;
    const double rmsL = std::sqrt (sumSq[0] / settled);
    const double rmsR = std::sqrt (sumSq[1] / settled);
    const double skewDb = (rmsL > 0.0 && rmsR > 0.0) ? 20.0 * std::log10 (rmsL / rmsR) : -999.0;
    const bool ok = rmsL > 0.0 && std::abs (skewDb) < 1.0;

    ANABASIS_TRACE_DUMP (c.name);
    ANABASIS_TRACE_RESET();
    std::printf ("  %-16s L=%.9f  R=%.9f  L/R=%7.2f dB  -> %s\n",
                 c.name, rmsL, rmsR, skewDb, ok ? "ok" : "REPRODUCED");
    return ok ? 0 : 1;
}

} // namespace

int main()
{
    int failures = 0;
    std::printf ("engine repro (no wrapper, no format, no host)\n");
    for (const auto& c : kCases)
        failures += runCase (c);
    std::printf ("%s\n", failures == 0 ? "engine repro: both cases kept both channels"
                                       : "engine repro: A CHANNEL WAS LOST OR SKEWED");
    return failures == 0 ? 0 : 1;
}
