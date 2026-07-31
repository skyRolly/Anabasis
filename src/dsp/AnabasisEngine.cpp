#include "AnabasisEngine.h"

namespace anabasis
{

void AnabasisEngine::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    sr           = sampleRate;
    delaySamples = maxLookaheadSamples (sampleRate);
    ringSize     = delaySamples + maxBlockSize;   // room for one block past the tap

    const int chans = juce::jlimit (1, kMaxChannels, numChannels);
    wetRing.setSize (chans, ringSize);
    dryRing.setSize (chans, ringSize);

    // 20 ms parameter smoothing: long enough to kill zipper noise, short
    // enough to feel immediate on a gain control.
    inputGain.reset (sampleRate, 0.020);
    pushGain.reset (sampleRate, 0.020);

    limiter.prepare (sampleRate, delaySamples);
    reset();
}

void AnabasisEngine::reset() noexcept
{
    wetRing.clear();
    dryRing.clear();
    writePos = 0;
    limiter.reset();
    inputGain.setCurrentAndTargetValue (inputGain.getTargetValue());
    pushGain.setCurrentAndTargetValue (pushGain.getTargetValue());
}

void AnabasisEngine::process (juce::AudioBuffer<float>& buffer, const EngineParameters& p) noexcept
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), wetRing.getNumChannels());
    if (numSamples <= 0 || numChannels <= 0 || ringSize <= 0)
        return;

    // ---- adopt the snapshot (once per block, ADR-0011) --------------------
    inputGain.setTargetValue (juce::Decibels::decibelsToGain (p.inputGainDb));
    pushGain.setTargetValue  (juce::Decibels::decibelsToGain (p.limGainDb));

    const float ceilingLin = juce::Decibels::decibelsToGain (p.ceilingDbTp);
    const float lookMs     = juce::jlimit ((float) kMinLookaheadMs, (float) kMaxLookaheadMs,
                                           p.lookaheadMs);
    limiter.setPerBlock (ceilingLin, lookMs, juce::jmax (1.0f, p.limReleaseMs));
    clamp.setCeilingDb (p.ceilingDbTp);

    if (p.bypass != bypassTarget)
    {
        bypassTarget = p.bypass;
        // ~10 ms linear crossfade (§2.8's always-running output crossfade,
        // minimal P1 form; exact endpoints below keep both nulls bit-exact).
        bypassStep = 1.0f / (float) juce::jmax (1, (int) (0.010 * sr));
    }

    bool sawNonFinite = false;

    for (int n = 0; n < numSamples; ++n)
    {
        const float gIn   = inputGain.getNextValue();
        const float gPush = pushGain.getNextValue();

        // Write into both fixed 10 ms lines; find the stereo-max magnitude of
        // the sample ENTERING the wet line for the pre-emptive gain computer.
        float stereoMax = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float in = buffer.getSample (ch, n);
            if (! std::isfinite (in))
                sawNonFinite = true;

            const float wet = (juce::exactlyEqual (gIn, 1.0f) ? in : in * gIn)
                            * (juce::exactlyEqual (gPush, 1.0f) ? 1.0f : gPush);
            wetRing.setSample (ch, writePos, std::isfinite (wet) ? wet : 0.0f);
            dryRing.setSample (ch, writePos, std::isfinite (in)  ? in  : 0.0f);

            const float mag = std::abs (wet);
            if (std::isfinite (mag) && mag > stereoMax)
                stereoMax = mag;
        }

        const float gr = limiter.processSample (stereoMax);

        int readPos = writePos - delaySamples;
        if (readPos < 0)
            readPos += ringSize;

        // Advance the §2.8 crossfade once per sample.
        const float targetMix = bypassTarget ? 1.0f : 0.0f;
        if (bypassMix < targetMix)      bypassMix = juce::jmin (targetMix, bypassMix + bypassStep);
        else if (bypassMix > targetMix) bypassMix = juce::jmax (targetMix, bypassMix - bypassStep);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float delayedWet = wetRing.getSample (ch, readPos);
            const float delayedDry = dryRing.getSample (ch, readPos);

            // gr == 1 multiplies exactly; the clamp passes sub-ceiling samples
            // untouched — together that is inv 7's bit-exact identity path.
            float processed = clamp.processSample (juce::exactlyEqual (gr, 1.0f) ? delayedWet : delayedWet * gr);

            // P1 dither: Off (the §4.2 default) is a true no-op. The 16/24-bit
            // TPDF stage lands with the metering work at P2 (DESIGN §2.9);
            // selecting it early must not silently alter the signal, so until
            // then the modes are inert by construction.

            float out;
            if (bypassMix <= 0.0f)      out = processed;                        // exact endpoint
            else if (bypassMix >= 1.0f) out = delayedDry;                       // exact endpoint
            else                        out = processed + (delayedDry - processed) * bypassMix;

            if (! std::isfinite (out))
            {
                sawNonFinite = true;
                out = 0.0f;
            }
            buffer.setSample (ch, n, out);
        }

        if (++writePos >= ringSize)
            writePos = 0;
    }

    // Invariant 9 self-heal: a non-finite anywhere resets the affected state
    // so one bad buffer cannot poison the envelope or the rings forever.
    if (sawNonFinite)
        limiter.reset();
}

} // namespace anabasis
