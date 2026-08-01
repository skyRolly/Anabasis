#include "AnabasisEngine.h"

namespace anabasis
{

void AnabasisEngine::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    sr           = sampleRate;
    delaySamples = maxLookaheadSamples (sampleRate);
    maxBlock     = juce::jmax (1, maxBlockSize);
    numChans     = juce::jlimit (1, kMaxChannels, numChannels);

    // Everything is sized for the MAXIMUM factor at prepare() (ADR-0011), so
    // a runtime latch selects among existing objects and buffers: the wet
    // ring holds 10 ms + one chunk at 16x, the dry ring holds the base
    // allowance + the worst oversampler delay + one chunk.
    const int maxN = 1 << kMaxOsFactorLog2;
    wetRing.setSize (numChans, delaySamples * maxN + maxBlock * maxN + 1);
    dryRingSize = delaySamples + kMaxOsLatencySamples + maxBlock + 1;
    dryRing.setSize (numChans, dryRingSize);
    staging.setSize (numChans, maxBlock);
    ceilArr.resize ((size_t) maxBlock);
    wArr.resize ((size_t) maxBlock);

    using OS = juce::dsp::Oversampling<float>;
    for (int f = 0; f < kMaxOsFactorLog2; ++f)
        for (int ph = 0; ph < 2; ++ph)
        {
            oversamplers[f][ph] = std::make_unique<OS> (
                (size_t) numChans, (size_t) (f + 1),
                ph == 0 ? OS::filterHalfBandPolyphaseIIR : OS::filterHalfBandFIREquiripple,
                true /*maxQuality*/, true /*useIntegerLatency*/);
            oversamplers[f][ph]->initProcessing ((size_t) maxBlock);

            // The Latency.h table must equal what the pinned JUCE actually
            // built - a bump that redesigns either cascade fails here (and in
            // the matrix impulse test) instead of silently desyncing PDC.
            jassert (juce::approximatelyEqual (
                oversamplers[f][ph]->getLatencyInSamples(),
                (float) osLatencySamples ((OversampleFactor) (f + 1),
                                          (OsPhaseMode) ph, sampleRate)));
        }

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

    limiter.prepare (sampleRate, delaySamples * maxN);   // wedge sized for 16x
    eq.prepare (sampleRate);
    comp.prepare (sampleRate);
    clip.prepare (sampleRate);

    latchedFactorIdx = -1;
    latchedPhaseIdx  = 0;
    latchOsConfig (-1, 0);
    reset();
}

void AnabasisEngine::latchOsConfig (int factorIdx, int phaseIdx) noexcept
{
    // A latch is a RESET-boundary event (ADR-0004: factor/phase changes are
    // latched and applied at a reset or crossfaded boundary - the 2.8 duck
    // wraps this when it lands; until then the switch itself may step, the
    // KI-001 family). Everything here is selection and plain-float
    // recomputation: no allocation on the audio thread.
    latchedFactorIdx = factorIdx;
    latchedPhaseIdx  = phaseIdx;
    osShift    = factorIdx < 0 ? 0 : factorIdx + 1;
    osN        = 1 << osShift;
    delayOs    = delaySamples * osN;
    ringSizeOs = delayOs + maxBlock * osN + 1;
    osActive   = factorIdx < 0 ? nullptr : oversamplers[factorIdx][phaseIdx].get();
    osLatBase  = factorIdx < 0 ? 0
               : osLatencySamples ((OversampleFactor) (factorIdx + 1),
                                   (OsPhaseMode) phaseIdx, sr);
    if (osActive != nullptr)
        osActive->reset();

    wetRing.clear();
    writePosOs = 0;
    limiter.setRate (sr * osN);
    limiter.reset();
    clip.setRate (sr * osN);
    clip.reset();
}

void AnabasisEngine::reset() noexcept
{
    wetRing.clear();
    dryRing.clear();
    writePosOs  = 0;
    dryWritePos = 0;
    limiter.reset();
    eq.reset();
    comp.reset();
    clip.reset();
    if (osActive != nullptr)
        osActive->reset();
    for (auto& e : ditherErr) e = 0.0f;
    rngState = 0x9E3779B9u;           // deterministic dither per render
    smoothersPrimed = false;          // the next block adopts ALL FOUR values without a glide

    // The crossfade is reset state too: landing ON the target is the "no
    // fade in progress" state; the next block re-reads bypassTarget.
    bypassMix = bypassTarget ? 1.0f : 0.0f;
}

void AnabasisEngine::process (juce::AudioBuffer<float>& buffer, const EngineParameters& p) noexcept
{
    const int totalSamples = buffer.getNumSamples();
    const int numChannels  = juce::jmin (buffer.getNumChannels(), wetRing.getNumChannels());
    if (totalSamples <= 0 || numChannels <= 0 || ringSizeOs <= 0)
        return;

    // Channels past the prepared count are left UNTOUCHED - not processed,
    // and deliberately not cleared, because silencing a caller's audio is a
    // worse failure than passing it through. The contract is asserted.
    jassert (buffer.getNumChannels() <= wetRing.getNumChannels());

    // ---- latch the OS config at the block boundary (never mid-block) ------
    const auto effF     = effectiveFactor (p);
    const int  wantIdx  = effF == OversampleFactor::off ? -1 : (int) effF - 1;
    const int  wantPh   = (int) p.osPhase;
    if (wantIdx != latchedFactorIdx || (wantIdx >= 0 && wantPh != latchedPhaseIdx))
        latchOsConfig (wantIdx, wantPh);

    // ---- adopt the snapshot (once per block, ADR-0011) ---------------------
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
        // prime together.
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
    // ADR-0003 item 6: at >=4x the region signal is already oversampled enough
    // for inter-sample peaks to be sample-visible - read it directly; below
    // that the tap runs its own 4x estimator (8x effective at 2x).
    limiter.setTruePeakMode (p.truePeakMode && osN < 4);
    limiter.setDetectorHpf (p.scHpfFreqHz);
    eq.setTargets (p);
    comp.setPerBlock (p);
    clip.setPerBlock (p);

    // eqPosition is a discrete REWIRE, not a glide (ADR-0010's same-day
    // note): state is cleared on a move; a step and <=10 ms of old-position
    // drain are the accepted KI-001-class artefacts until 2.8 lands.
    if (p.eqPosition != eqPositionNow)
    {
        eqPositionNow = p.eqPosition;
        eq.resetState();
    }
    const bool eqPre  = eqPositionNow == 0;
    const bool eqPost = ! eqPre;

    if (p.bypass != bypassTarget)
        bypassTarget = p.bypass;   // step size is rate-derived, set in prepare()

    // ---- chunked processing: oversize host blocks degrade to extra chunk
    //      overhead, never to unprocessed audio -----------------------------
    for (int start = 0; start < totalSamples; start += maxBlock)
        processChunk (buffer, start, juce::jmin (maxBlock, totalSamples - start),
                      p, eqPre, eqPost);
}

void AnabasisEngine::processChunk (juce::AudioBuffer<float>& buffer, const int start,
                                   const int num, const EngineParameters& p,
                                   const bool eqPre, const bool eqPost) noexcept
{
    const int nCh = juce::jmin (buffer.getNumChannels(), wetRing.getNumChannels());
    bool sawNonFinite = false;

    // ======== Stage A - base rate: input gain -> EQ(Pre) -> compressor =====
    // Also fills the per-base-sample control arrays the region indexes, so
    // the SAME instantaneous ceiling the gain computer uses reaches the
    // clamp, exactly as before the restructure. The EQ ticks in whichever
    // stage processes it - ticking here while Post processes in stage E
    // would hand every Post sample the block's final coefficients, a
    // block-length step that breaks the smoothing contract.
    for (int n = 0; n < num; ++n)
    {
        const float gIn   = inputGain.getNextValue();
        const float gPush = pushGain.getNextValue();
        ceilArr[(size_t) n] = ceilingLinear.getNextValue();
        const int wBase = juce::jlimit (1, delaySamples,
                                        juce::roundToInt (windowSamples.getNextValue()));
        wArr[(size_t) n] = wBase;
        engagedWindow.store (wBase, std::memory_order_relaxed);
        if (eqPre)
            eq.tick();

        float staged[kMaxChannels] = {};
        for (int ch = 0; ch < nCh; ++ch)
        {
            const float in = buffer.getSample (ch, start + n);
            if (! std::isfinite (in))
                sawNonFinite = true;

            float s = juce::exactlyEqual (gIn, 1.0f) ? in : in * gIn;
            if (! std::isfinite (s))
                s = 0.0f;                      // filter state must never eat a NaN
            if (eqPre)
                s = eq.processSample (ch, s);
            staged[ch] = s;

            dryRing.setSample (ch, dryWritePos, std::isfinite (in) ? in : 0.0f);
        }

        comp.processSample (staged, nCh);      // stereo-linked, in place

        for (int ch = 0; ch < nCh; ++ch)
        {
            const float wet = juce::exactlyEqual (gPush, 1.0f) ? staged[ch]
                                                               : staged[ch] * gPush;
            staging.setSample (ch, n, std::isfinite (wet) ? wet : 0.0f);
        }
        if (++dryWritePos >= dryRingSize)
            dryWritePos = 0;
    }

    // ======== Stages B/C/D - the oversampled region =========================
    // Region = Clipper/Sat -> lookahead line -> limiter (DSP_POLICY inv 5).
    // At Off the region IS the staging block: no filters, no added latency,
    // and the arithmetic below is bit-identical to the pre-OS engine.
    juce::dsp::AudioBlock<float> baseBlock (staging);
    auto stagedBlock = baseBlock.getSubBlock (0, (size_t) num);
    juce::dsp::AudioBlock<float> region = stagedBlock;
    if (osActive != nullptr)
        region = osActive->processSamplesUp (stagedBlock);

    const int regionSamples = num << osShift;
    for (int i = 0; i < regionSamples; ++i)
    {
        const int   b          = i >> osShift;
        const float ceilingNow = ceilArr[(size_t) b];
        const int   wOs        = wArr[(size_t) b] << osShift;

        float frame[kMaxChannels] = {};
        for (int ch = 0; ch < nCh; ++ch)
            frame[ch] = region.getSample ((size_t) ch, (size_t) i);

        clip.processSample (frame, nCh);       // Clipper/Sat, inside the region

        for (int ch = 0; ch < nCh; ++ch)
            wetRing.setSample (ch, writePosOs,
                               std::isfinite (frame[ch]) ? frame[ch] : 0.0f);

        // Detector tap (the LookaheadLimiter CONTRACT): feed the sample that
        // plays W steps from now - written (delayOs - wOs) steps ago. W is
        // the SMOOTHED window scaled to the region rate, so a lookahead move
        // slides the tap instead of jumping it (invariant 8).
        int detPos = writePosOs - (delayOs - wOs);
        if (detPos < 0)
            detPos += ringSizeOs;
        float tapped[kMaxChannels] = {};
        for (int ch = 0; ch < nCh; ++ch)
            tapped[ch] = wetRing.getSample (ch, detPos);

        float gains[kMaxChannels] = { 1.0f, 1.0f };
        limiter.processSample (tapped, nCh, wOs, ceilingNow, gains);

        int readPos = writePosOs - delayOs;
        if (readPos < 0)
            readPos += ringSizeOs;
        for (int ch = 0; ch < nCh; ++ch)
        {
            const float delayedWet = wetRing.getSample (ch, readPos);
            region.setSample ((size_t) ch, (size_t) i,
                              juce::exactlyEqual (gains[ch], 1.0f) ? delayedWet
                                                                   : delayedWet * gains[ch]);
        }

        if (++writePosOs >= ringSizeOs)
            writePosOs = 0;
    }
    if (osActive != nullptr)
        osActive->processSamplesDown (stagedBlock);

    // ======== Stage E - base rate: EQ(Post) -> clamp -> dither -> bypass ===
    const bool ditherOn = p.ditherMode != 0;
    const float q = p.ditherMode == 1 ? 3.0517578125e-5f          // 2^-15 (16-bit)
                                      : 1.1920928955078125e-7f;   // 2^-23 (24-bit)
    if (! (ditherOn && p.ditherShaping))
        for (auto& e : ditherErr) e = 0.0f;

    // The dry read trails the dry write by the chunk length plus the total
    // reported delay: allowance + oversampler group delay (Latency.h - the
    // SAME numbers the wrapper reports, which is the bypass-null contract).
    int dryReadPos = dryWritePos - num - (delaySamples + osLatBase);
    while (dryReadPos < 0)
        dryReadPos += dryRingSize;

    for (int n = 0; n < num; ++n)
    {
        const float ceilingNow = ceilArr[(size_t) n];
        if (eqPost)
            eq.tick();

        // Advance the 2.8 crossfade once per base sample.
        const float targetMix = bypassTarget ? 1.0f : 0.0f;
        if (bypassMix < targetMix)      bypassMix = juce::jmin (targetMix, bypassMix + bypassStep);
        else if (bypassMix > targetMix) bypassMix = juce::jmax (targetMix, bypassMix - bypassStep);

        for (int ch = 0; ch < nCh; ++ch)
        {
            float processed = staging.getSample (ch, n);

            // Post-position EQ sits AFTER the limiter and BEFORE the clamp -
            // the placement ADR-0002 exists for: a +12 dB post shelf can push
            // the limited signal back over the ceiling, and the clamp being
            // downstream is what keeps invariant 4 unconditional.
            if (eqPost)
                processed = eq.processSample (ch, processed);
            processed = clamp.processSample (processed, ceilingNow);

            // Dither (4.5): AFTER the clamp (invariant 1/12), processed path
            // only - bypass must stay a bit-exact null. TPDF at +-1 LSB of
            // the target depth, optional first-order error-feedback shaping,
            // deterministic RNG (offline renders repeat exactly). Off is a
            // true no-op branch, so selecting it early alters nothing.
            if (ditherOn)
            {
                rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
                const float r1 = (float) (rngState >> 8) * (1.0f / 16777216.0f);
                rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
                const float r2 = (float) (rngState >> 8) * (1.0f / 16777216.0f);
                float x = processed;
                if (p.ditherShaping)
                    x -= ditherErr[ch];
                const float y = q * std::nearbyint (x / q + (r1 - r2));
                if (p.ditherShaping)
                    ditherErr[ch] = y - x;
                processed = y;
            }

            // Bypass carries the UNCLAMPED, UNDITHERED dry signal: invariant 7
            // requires bypass to be a bit-exact delay-aligned null, so the
            // ceiling guarantee is a property of the PROCESSED path.
            const float delayedDry = dryRing.getSample (ch, dryReadPos);
            float out;
            if (bypassMix <= 0.0f)      out = processed;                        // exact endpoint
            else if (bypassMix >= 1.0f) out = delayedDry;                       // exact endpoint
            else                        out = processed + (delayedDry - processed) * bypassMix;

            if (! std::isfinite (out))
            {
                sawNonFinite = true;
                out = 0.0f;
            }
            buffer.setSample (ch, start + n, out);
        }
        if (++dryReadPos >= dryRingSize)
            dryReadPos = 0;
    }

    // Invariant 9 self-heal: discard the limiter's sliding window, carry the
    // envelope (see resetWindow's comment for why not a full reset).
    if (sawNonFinite)
        limiter.resetWindow();
}

} // namespace anabasis
