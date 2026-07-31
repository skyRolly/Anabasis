#include "AnabasisEngine.h"

namespace anabasis
{

void AnabasisEngine::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    sr           = sampleRate;
    delaySamples = maxLookaheadSamples (sampleRate);
    ringSize     = delaySamples + juce::jmax (1, maxBlockSize);   // room past the tap;
    // the jmax guards a prepareToPlay(sr, 0) host: ringSize == delaySamples would
    // make readPos == writePos and silently zero the group delay.

    const int chans = juce::jlimit (1, kMaxChannels, numChannels);
    wetRing.setSize (chans, ringSize);
    dryRing.setSize (chans, ringSize);

    // 20 ms parameter smoothing: long enough to kill zipper noise, short
    // enough to feel immediate on a gain control.
    inputGain.reset (sampleRate, 0.020);
    pushGain.reset (sampleRate, 0.020);
    ceilingLinear.reset (sampleRate, 0.020);
    windowSamples.reset (sampleRate, 0.020);

    // The bypass fade length is derived from the sample rate, so it belongs
    // here as well as at the toggle: a host that re-prepares at a new rate
    // mid-fade would otherwise finish that fade at the old rate's step size.
    bypassStep = 1.0f / (float) juce::jmax (1, (int) (0.010 * sampleRate));

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
    smoothersPrimed = false;   // the next block adopts its values without a glide
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

    const float ceilingTarget = juce::Decibels::decibelsToGain (p.ceilingDbTp);
    const float lookMs        = juce::jlimit ((float) kMinLookaheadMs, (float) kMaxLookaheadMs,
                                              p.lookaheadMs);
    const float windowTarget  = juce::jlimit (1.0f, (float) delaySamples,
                                              (float) std::ceil (lookMs * 0.001 * sr));
    if (! smoothersPrimed)
    {
        // First block after prepare/reset: adopt, do not glide. A ramp from a
        // stale value here would be an artefact of construction, not a user
        // move, and it would break the impulse-at-the-allowance test.
        ceilingLinear.setCurrentAndTargetValue (ceilingTarget);
        windowSamples.setCurrentAndTargetValue (windowTarget);
        smoothersPrimed = true;
    }
    else
    {
        ceilingLinear.setTargetValue (ceilingTarget);
        windowSamples.setTargetValue (windowTarget);
    }
    limiter.setRelease (juce::jmax (1.0f, p.limReleaseMs));

    if (p.bypass != bypassTarget)
        bypassTarget = p.bypass;   // step size is rate-derived, set in prepare()

    bool sawNonFinite = false;

    for (int n = 0; n < numSamples; ++n)
    {
        const float gIn   = inputGain.getNextValue();
        const float gPush = pushGain.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float in = buffer.getSample (ch, n);
            if (! std::isfinite (in))
                sawNonFinite = true;

            const float wet = (juce::exactlyEqual (gIn, 1.0f) ? in : in * gIn)
                            * (juce::exactlyEqual (gPush, 1.0f) ? 1.0f : gPush);
            wetRing.setSample (ch, writePos, std::isfinite (wet) ? wet : 0.0f);
            dryRing.setSample (ch, writePos, std::isfinite (in)  ? in  : 0.0f);
        }

        // Detector tap (the LookaheadLimiter CONTRACT): feed the sample that
        // plays W steps from now, i.e. the one written (delaySamples - W)
        // steps ago — NOT the just-written input, which is 10 ms ahead
        // regardless of the engaged lookahead. This offset is what makes the
        // `lookahead` parameter the real pre-emption time while the audio
        // delay stays at the full allowance (ADR-0004). W is the SMOOTHED
        // window, so a lookahead move slides the tap instead of jumping it by
        // hundreds of samples at a block boundary (invariant 8).
        const int w = juce::jlimit (1, delaySamples,
                                    juce::roundToInt (windowSamples.getNextValue()));
        engagedWindow = w;
        const float ceilingNow = ceilingLinear.getNextValue();

        int detPos = writePos - (delaySamples - w);
        if (detPos < 0)
            detPos += ringSize;
        float stereoMax = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float mag = std::abs (wetRing.getSample (ch, detPos));
            if (mag > stereoMax)
                stereoMax = mag;
        }

        const float gr = limiter.processSample (stereoMax, w, ceilingNow);

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
            float processed = clamp.processSample (
                juce::exactlyEqual (gr, 1.0f) ? delayedWet : delayedWet * gr, ceilingNow);

            // P1 dither: Off (the §4.2 default) is a true no-op. The 16/24-bit
            // TPDF stage lands with the metering work at P2 (DESIGN §2.9);
            // selecting it early must not silently alter the signal, so until
            // then the modes are inert by construction.

            // Bypass carries the UNCLAMPED dry signal, deliberately: invariant 7
            // requires bypass to be a bit-exact delay-aligned null, so invariant
            // 4's ceiling guarantee is a property of the PROCESSED path. The two
            // can only both hold under that reading, and it is stated here
            // rather than left to be inferred.
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

    // Invariant 9 self-heal: a non-finite anywhere resets the limiter so one
    // bad buffer cannot poison the envelope forever. The reset empties the
    // sliding window, so for the next W samples the maximum covers fewer than
    // W+1 values and an over-ceiling peak already inside the delay line is not
    // pre-empted — it meets the final clamp instead. That is a transient
    // quality cost, not a contract break: the clamp is unconditional, so
    // invariant 4 still holds throughout the recovery.
    if (sawNonFinite)
        limiter.reset();
}

} // namespace anabasis
