#include "AnabasisEngine.h"

namespace anabasis
{

void AnabasisEngine::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    sr           = sampleRate;
    delaySamples = maxLookaheadSamples (sampleRate);
    ringSize     = delaySamples + juce::jmax (1, maxBlockSize);
    // CORRECTNESS NEEDS ONLY delaySamples + 1, and it is independent of the
    // block size. process() is a per-SAMPLE circular delay line: each step
    // writes at writePos and reads at writePos - delaySamples, so the slot read
    // at step t was written at t - delaySamples and is next overwritten at
    // t - delaySamples + ringSize — later than the read whenever
    // ringSize >= delaySamples + 1. A host that delivers MORE samples than it
    // declared in prepareToPlay therefore cannot lap the tap; nothing here is
    // block-structured. The `+ maxBlockSize` is slack, not the invariant, and
    // the jmax guards a prepareToPlay(sr, 0) host, where ringSize ==
    // delaySamples would make readPos == writePos and silently zero the group
    // delay. Stated explicitly because the previous wording ("room past the
    // tap") reads as a block-size dependency and invites a clamp on
    // numSamples — which would leave the tail of an oversized block
    // unprocessed, i.e. bypassing the ceiling clamp, to fix a bug that is not
    // there.

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
    eq.prepare (sampleRate);
    comp.prepare (sampleRate);
    clip.prepare (sampleRate);
    reset();
}

void AnabasisEngine::reset() noexcept
{
    wetRing.clear();
    dryRing.clear();
    writePos = 0;
    limiter.reset();
    eq.reset();
    comp.reset();
    clip.reset();
    smoothersPrimed = false;   // the next block adopts ALL FOUR values without a glide

    // The crossfade is reset state too. Leaving a part-way `bypassMix` behind
    // meant a re-prepare mid-fade resumed that fade against freshly zeroed
    // delay lines — the fade position no longer consistent with anything else
    // the reset just cleared. Landing ON the target is the "no fade in
    // progress" state; the next block re-reads bypassTarget from the snapshot.
    bypassMix = bypassTarget ? 1.0f : 0.0f;
}

void AnabasisEngine::process (juce::AudioBuffer<float>& buffer, const EngineParameters& p) noexcept
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), wetRing.getNumChannels());
    if (numSamples <= 0 || numChannels <= 0 || ringSize <= 0)
        return;

    // Channels past the prepared count are left UNTOUCHED — not processed, and
    // deliberately not cleared, because silencing a caller's audio is a worse
    // failure than passing it through. That makes invariant 4's ceiling
    // guarantee conditional on prepare() having been told the real channel
    // count, so the contract is asserted rather than assumed: the wrapper
    // restricts the plugin to stereo (isBusesLayoutSupported), and the DSP
    // test target is the other caller of this shared component.
    jassert (buffer.getNumChannels() <= wetRing.getNumChannels());

    // ---- adopt the snapshot (once per block, ADR-0011) --------------------
    const float inputTarget   = juce::Decibels::decibelsToGain (p.inputGainDb);
    const float pushTarget    = juce::Decibels::decibelsToGain (p.limGainDb);
    const float ceilingTarget = juce::Decibels::decibelsToGain (p.ceilingDbTp);
    const float lookMs        = juce::jlimit ((float) kMinLookaheadMs, (float) kMaxLookaheadMs,
                                              p.lookaheadMs);
    const float windowTarget  = juce::jlimit (1.0f, (float) delaySamples,
                                              (float) std::ceil (lookMs * 0.001 * sr));
    if (! smoothersPrimed)
    {
        // First block after prepare/reset: adopt, do not glide. A ramp here is
        // an artefact of construction, not a user move. ALL FOUR smoothers
        // prime together — an earlier revision primed only the ceiling and the
        // window, leaving inputGain/pushGain to ramp from their constructor
        // unity to the session's values, so the first 20 ms of audio after
        // loading a session played up to 18 dB low and slid up.
        inputGain.setCurrentAndTargetValue (inputTarget);
        pushGain.setCurrentAndTargetValue (pushTarget);
        ceilingLinear.setCurrentAndTargetValue (ceilingTarget);
        windowSamples.setCurrentAndTargetValue (windowTarget);
        smoothersPrimed = true;
    }
    else
    {
        inputGain.setTargetValue (inputTarget);
        pushGain.setTargetValue (pushTarget);
        ceilingLinear.setTargetValue (ceilingTarget);
        windowSamples.setTargetValue (windowTarget);
    }
    limiter.setRelease (juce::jmax (1.0f, p.limReleaseMs));
    limiter.setAutoRelease (p.limAutoRelease);
    limiter.setStyle (p.limStyle);
    limiter.setTransientPreserve (p.transientPreserve);
    limiter.setStereoLink (p.stereoLink);
    limiter.setTruePeakMode (p.truePeakMode);
    limiter.setDetectorHpf (p.scHpfFreqHz);   // per block: detector-side, and the
    // envelope + wedge are the smoothers downstream of it (rates-not-levels
    // family); the compressor smooths its copy per sample because its GR
    // follows the detector directly.
    eq.setTargets (p);
    comp.setPerBlock (p);
    clip.setPerBlock (p);

    // eqPosition is a discrete REWIRE, not a glide (ADR-0010's same-day note):
    // the biquad history belongs to the stream the EQ was in, so it is cleared
    // on a move. Two accepted P1-class artefacts until the §2.8 duck routes
    // this switch: the rewire itself can step, and for the next delaySamples
    // the ring still drains samples EQ'd at the old position while the new
    // position also processes them (double/none for ≤10 ms). KI-001 records
    // the same gap for the A/B swap; both close together when §2.8 lands.
    if (p.eqPosition != eqPositionNow)
    {
        eqPositionNow = p.eqPosition;
        eq.resetState();
    }
    const bool eqPre  = eqPositionNow == 0;
    const bool eqPost = ! eqPre;

    if (p.bypass != bypassTarget)
        bypassTarget = p.bypass;   // step size is rate-derived, set in prepare()

    bool sawNonFinite = false;

    for (int n = 0; n < numSamples; ++n)
    {
        const float gIn   = inputGain.getNextValue();
        const float gPush = pushGain.getNextValue();
        eq.tick();   // once per sample, whichever position processes it

        // Chain order (ADR-0002): Input Gain → EQ(Pre) → Compressor →
        // Clipper/Saturation → limiter push. Everything upstream of the wet ring is
        // what the limiter's detector sees — boosting a shelf or squeezing
        // with the comp drives the limiter, as a mastering chain must. The
        // compressor is stereo-LINKED (one gain for all channels), so it
        // processes the whole frame in place after the per-channel stages.
        float staged[kMaxChannels] = {};
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float in = buffer.getSample (ch, n);
            if (! std::isfinite (in))
                sawNonFinite = true;

            float s = juce::exactlyEqual (gIn, 1.0f) ? in : in * gIn;
            if (! std::isfinite (s))
                s = 0.0f;                      // filter state must never eat a NaN
            if (eqPre)
                s = eq.processSample (ch, s);
            staged[ch] = s;

            dryRing.setSample (ch, writePos, std::isfinite (in) ? in : 0.0f);
        }

        comp.processSample (staged, numChannels);
        clip.processSample (staged, numChannels);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float wet = juce::exactlyEqual (gPush, 1.0f) ? staged[ch] : staged[ch] * gPush;
            wetRing.setSample (ch, writePos, std::isfinite (wet) ? wet : 0.0f);
        }

        // Detector tap (the LookaheadLimiter CONTRACT): feed the sample that
        // plays W steps from now, i.e. the one written (delaySamples - W)
        // steps ago — NOT the just-written input, which is 10 ms ahead
        // regardless of the engaged lookahead. This offset is what makes the
        // `lookahead` parameter the real pre-emption time while the audio
        // delay stays at the full allowance (ADR-0004). W is the SMOOTHED
        // window, so a lookahead move slides the tap instead of jumping it by
        // hundreds of samples at a block boundary (invariant 8).
        //
        // While W glides DOWN the tap advances by more than one sample per
        // step, so a few samples are never fed to the wedge and get no
        // pre-emptive attenuation; while it glides UP some are fed twice.
        // Invariant 4 still holds — CeilingClamp is unconditional and
        // downstream — so this is the same transient quality cost as the
        // invariant-9 self-heal below, stated here rather than left for a
        // reader to assume full coverage during a lookahead move.
        const int w = juce::jlimit (1, delaySamples,
                                    juce::roundToInt (windowSamples.getNextValue()));
        engagedWindow.store (w, std::memory_order_relaxed);
        const float ceilingNow = ceilingLinear.getNextValue();

        int detPos = writePos - (delaySamples - w);
        if (detPos < 0)
            detPos += ringSize;
        float tapped[kMaxChannels] = {};
        for (int ch = 0; ch < numChannels; ++ch)
            tapped[ch] = wetRing.getSample (ch, detPos);

        // Per-channel gains: identical when stereoLink is 1 (the default),
        // partially independent below it — the limiter owns the link maths.
        float gains[kMaxChannels] = { 1.0f, 1.0f };
        limiter.processSample (tapped, numChannels, w, ceilingNow, gains);

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

            // gain == 1 multiplies exactly; the clamp passes sub-ceiling
            // samples untouched — together that is inv 7's bit-exact identity
            // path.
            const float gr = gains[ch];
            float processed = juce::exactlyEqual (gr, 1.0f) ? delayedWet : delayedWet * gr;

            // Post-position EQ sits AFTER the limiter and BEFORE the clamp —
            // the placement ADR-0002 exists for: a +12 dB post shelf can push
            // the limited signal back over the ceiling, and the clamp being
            // downstream is what keeps invariant 4 unconditional.
            if (eqPost)
                processed = eq.processSample (ch, processed);
            processed = clamp.processSample (processed, ceilingNow);

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

    // Invariant 9 self-heal: a non-finite anywhere discards the limiter's
    // sliding window so one bad buffer cannot poison it. For the next W samples
    // the maximum then covers fewer than W+1 values, and an over-ceiling peak
    // already inside the delay line is not pre-empted — it meets the final
    // clamp instead. That is a transient quality cost, not a contract break:
    // the clamp is unconditional, so invariant 4 still holds throughout.
    //
    // resetWindow(), NOT reset(): the full reset also snapped the envelope back
    // to unity, which is a second and worse effect. Recovering from a heavily
    // gain-reduced block, the very next sample would jump from (say) 0.3 to 1.0
    // with no release ramp and the clamp would flat-top it — a click on the one
    // path whose whole purpose is to degrade gracefully.
    if (sawNonFinite)
        limiter.resetWindow();
}

} // namespace anabasis
