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
    // +2, not +1: the read trails the write by num + delaySamples + osLatBase,
    // which at the worst case (full block, 16× linear) is exactly size − 1 —
    // correct, but with zero slack, so one grown table entry would have the
    // oldest read slot land on the slot being written. The spare slot costs
    // four bytes per channel and removes the coincidence.
    dryRingSize = delaySamples + kMaxOsLatencySamples + maxBlock + 2;
    dryRing.setSize (numChans, dryRingSize);
    staging.setSize (numChans, maxBlock);
    ceilArr.resize ((size_t) maxBlock);
    wArr.resize ((size_t) maxBlock);
    pushArr.resize ((size_t) maxBlock);

    using OS = juce::dsp::Oversampling<float>;
    osTableMatchesJuce = true;
    for (int f = 0; f < kMaxOsFactorLog2; ++f)
        for (int ph = 0; ph < 2; ++ph)
        {
            oversamplers[f][ph] = std::make_unique<OS> (
                (size_t) numChans, (size_t) (f + 1),
                ph == 0 ? OS::filterHalfBandPolyphaseIIR : OS::filterHalfBandFIREquiripple,
                true /*maxQuality*/, true /*useIntegerLatency*/);
            oversamplers[f][ph]->initProcessing ((size_t) maxBlock);

            // The Latency.h table must equal what the pinned JUCE actually
            // built - a bump that redesigns either cascade would desync PDC
            // AND splice the bypass leg, so the check is recorded
            // UNCONDITIONALLY (a jassert alone verifies nothing in the Release
            // builds that ship and that CI tests). The suite asserts the flag;
            // the impulse matrix independently measures the group delay.
            // NOT self-healing by design: clamping the engine to the measured
            // value would make the engine's delay disagree with the WRAPPER's
            // reported figure, which comes from the same const table via
            // predictLatencySamples - i.e. it would manufacture the desync it
            // was meant to prevent. The pin is frozen by DEPENDENCY_POLICY and
            // a bump is an Architecture Review Gate item; this flag exists so
            // such a bump fails a test rather than shipping quietly.
            const bool tableOk = juce::approximatelyEqual (
                oversamplers[f][ph]->getLatencyInSamples(),
                (float) osLatencySamples ((OversampleFactor) (f + 1),
                                          (OsPhaseMode) ph, sampleRate));
            osTableMatchesJuce = osTableMatchesJuce && tableOk;
            jassert (tableOk);
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
    duckOutInc = 1.0f / (float) juce::jmax (1, (int) (0.006 * sampleRate));   // §2.8: ~6 ms out
    duckInInc  = 1.0f / (float) juce::jmax (1, (int) (0.028 * sampleRate));   // ~28 ms in

    limiter.prepare (sampleRate, delaySamples * maxN);   // wedge sized for 16x
    dryMeter.prepare (sampleRate);
    wetMeter.prepare (sampleRate);
    outMeter.prepare (sampleRate);
    outTp.prepare();
    adaptiveEngine.prepare (sampleRate, maxBlock);
    monitorGain.reset (sampleRate, 0.200);
    deltaStep = bypassStep;                  // same ~10 ms always-running fade
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

    // The dry read trails the write by chunk + delaySamples + osLatBase; the
    // ring was sized at prepare() with kMaxOsLatencySamples standing in for
    // osLatBase, so every latch must stay inside that envelope. This trips
    // the moment a table entry outgrows the Latency.h ceiling.
    jassert (delaySamples + osLatBase + maxBlock < dryRingSize - 1);
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
    duckState = DuckState::idle;
    duckGain  = 1.0f;
    duckPhase = 0.0f;
    bottomHoldSamples = 0;
    duckAskedWhileOut = false;
    dryMeter.reset();
    wetMeter.reset();
    outMeter.reset();
    outTp.reset();
    renderTpMaxCall = renderPeakCall = 0.0f;
    adaptiveEngine.reset();
    compMeasureDb = 0.0f;
    monitorGain.setCurrentAndTargetValue (1.0f);
    deltaMix = deltaTarget ? 1.0f : 0.0f;
    smoothersPrimed = false;          // the next block adopts ALL FOUR values without a glide

    // The crossfade is reset state too: landing ON the target is the "no
    // fade in progress" state; the next block re-reads bypassTarget.
    bypassMix = bypassTarget ? 1.0f : 0.0f;
}

bool AnabasisEngine::process (juce::AudioBuffer<float>& buffer, const EngineParameters& p) noexcept
{
    const int totalSamples = buffer.getNumSamples();
    const int numChannels  = juce::jmin (buffer.getNumChannels(), wetRing.getNumChannels());
    if (totalSamples <= 0 || numChannels <= 0 || ringSizeOs <= 0)
        return false;   // nothing processed — the meter taps hold last block's values

    // Channels past the prepared count are left UNTOUCHED - not processed,
    // and deliberately not cleared, because silencing a caller's audio is a
    // worse failure than passing it through. The contract is asserted.
    jassert (buffer.getNumChannels() <= wetRing.getNumChannels());

    // ---- §2.8: discrete rewires go through the duck ------------------------
    // Wanted vs APPLIED configuration. On the first block after prepare/reset
    // the wanted config is adopted directly (a duck there would dip the first
    // 40 ms of every render for no transition at all — same rule as the
    // smoother priming). Afterwards, any difference — or a wrapper duck
    // request — ducks out; the rewire executes at the SILENT BOTTOM, at a
    // block boundary, so the OS latency still never changes mid-block
    // (ADR-0004) and no rewire happens at audible gain.
    const auto effF     = effectiveFactor (p);
    const int  wantIdx  = effF == OversampleFactor::off ? -1 : (int) effF - 1;
    const int  wantPh   = (int) p.osPhase;
    const int  wantEq   = p.eqPosition;
    const int  wantModel = juce::jlimit (0, 3, p.colourModel);
    const bool duckAsked = duckRequested.exchange (false, std::memory_order_relaxed);

    // Learn commands + learned-target restore, consumed at the block top. ONE
    // staged record, so the LAST restore staged before this block wins — see
    // restoreLearnedTargets() for why two flags could not express that.
    if (adaptivePending.exchange (false, std::memory_order_acquire))
    {
        if (pendingLearned.load (std::memory_order_relaxed))
            adaptiveEngine.setLearnedTargets (pendingRefOnset.load (std::memory_order_relaxed),
                                              pendingRefTilt.load (std::memory_order_relaxed));
        else
            adaptiveEngine.clearLearnedTargets();
    }
    if (learnStartReq.exchange (false, std::memory_order_relaxed))
        adaptiveEngine.startLearn();
    if (learnStopReq.exchange (false, std::memory_order_relaxed))
        adaptiveEngine.commitLearn();
    const bool rewireWanted = wantIdx != latchedFactorIdx
                           || (wantIdx >= 0 && wantPh != latchedPhaseIdx)
                           || wantEq != appliedEqPos
                           || wantModel != appliedModel;

    // A realtime↔offline flip is a RESET-class event, not an audible
    // transition: the host re-reads PDC across it (setNonRealtime is an
    // ADR-0004 recompute trigger) and, at Force Max, effectiveFactor changes
    // with it. Ducking there would fade the HEAD OF A BOUNCE — ~45 ms of
    // envelope written into the rendered file — for a transition no one is
    // listening to. Adopting directly makes the no-re-prepare path behave
    // exactly like the re-prepare path most hosts take.
    const bool offlineFlip = p.nonRealtime != lastNonRealtime;
    lastNonRealtime = p.nonRealtime;

    if (! smoothersPrimed || offlineFlip)
    {
        if (wantIdx != latchedFactorIdx || (wantIdx >= 0 && wantPh != latchedPhaseIdx))
            latchOsConfig (wantIdx, wantPh);
        appliedEqPos = wantEq;
        appliedModel = wantModel;
        duckState = DuckState::idle;
        duckGain  = 1.0f;
        // A render that STARTS with an empty pipeline is not a transition:
        // the reported latency is the promise that those samples are absent,
        // so no hold, and nothing is owed from before the reset. `duckAsked`
        // is consumed above and DELIBERATELY discarded here — a request that
        // arrived before the first block (a setStateInformation ahead of
        // prepareToPlay, which updateLatency's 48 kHz fallback shows is a
        // real ordering) has nothing to fade. This is the one dropped request
        // that is correct; the three that were not are in the branches below.
        bottomHoldSamples = 0;
        duckAskedWhileOut = false;
    }
    else if (duckState == DuckState::bottom)
    {
        // Silent, and a block boundary: execute everything pending.
        if (wantIdx != latchedFactorIdx || (wantIdx >= 0 && wantPh != latchedPhaseIdx))
        {
            latchOsConfig (wantIdx, wantPh);
            // The latch emptied the lookahead ring and reset the oversampler:
            // the processed path now emits EXACT silence until both refill.
            // Hold the bottom that long (the counter runs down per processed
            // base sample) so the in-leg starts from real audio at zero gain
            // instead of splicing it in partway up the ramp.
            bottomHoldSamples = delaySamples + osLatBase;
        }
        if (wantEq != appliedEqPos)
        {
            appliedEqPos = wantEq;
            eq.resetState();
        }
        appliedModel = wantModel;

        // A request seen at (or during) the bottom holds it one more block:
        // the bulk swap it guards reaches the snapshot NEXT block and must be
        // adopted at zero gain too. Dropping it stepped the new config
        // mid-recovery — the same defect in the `out` state fed the flag here
        // through duckAskedWhileOut rather than discarding it.
        const bool holdForRequest = duckAsked || duckAskedWhileOut;
        duckAskedWhileOut = false;
        if (! holdForRequest && bottomHoldSamples <= 0)
        {
            duckState = DuckState::in;
            duckPhase = 0.0f;
        }
    }
    else if ((rewireWanted || duckAsked) && duckState != DuckState::out)
    {
        // Enter (or re-enter from recovery) the duck-out leg, CONTINUOUSLY
        // from the current gain: the out-leg gain is 0.5(1+cos(π·p)), so
        // p = acos(2g−1)/π IS the out-leg phase (g=1 → p=0, g=0 → p=1). An
        // earlier revision added a spurious 1−p on top, which sent a fresh
        // duck straight to the bottom in ONE sample — the exact step the
        // smoothness test exists to catch, and did.
        duckState = DuckState::out;
        duckPhase = std::acos (juce::jlimit (-1.0f, 1.0f, 2.0f * duckGain - 1.0f))
                    / juce::MathConstants<float>::pi;
    }
    else if (duckAsked)
    {
        // Only reachable with duckState == out (idle/in are caught above):
        // the fade is already running, so there is nothing to re-enter — but
        // the request must not evaporate. Remember it and spend it as one
        // held bottom block, which is what it was asking for.
        duckAskedWhileOut = true;
    }

    // The stages see the APPLIED discrete config, not the wanted one.
    EngineParameters pApplied = p;
    pApplied.eqPosition  = appliedEqPos;
    pApplied.colourModel = appliedModel;

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

    // §5.4 adaptive trims: bounded deltas around the CURRENT values, applied
    // to the per-block settings only — never parameter writes, never
    // lookahead or the OS factor (policy inv 4). All four are inert when
    // their host stages are inert, which is why the bit-exact null test runs
    // with adaptation LIVE. A ceiling lock is irrelevant here (ceiling is not
    // a trim member); the §9 lockable set is {ceiling} in v1.
    {
        const auto& t = adaptiveEngine.currentTrims();
        pApplied.limReleaseMs = juce::jlimit (1.0f, 1000.0f,
                                              p.limReleaseMs * std::pow (2.0f, t.releaseOctaves));
        pApplied.stereoLink   = juce::jlimit (0.0f, 1.0f, p.stereoLink + t.stereoLink);
        pApplied.scHpfFreqHz  = juce::jlimit (20.0f, 300.0f, p.scHpfFreqHz + t.scHpfHz);
        pApplied.dynTiltDb    = juce::jlimit (0.0f, 2.0f, p.dynTiltDb + t.dynTiltDb);
    }

    limiter.setRelease (juce::jmax (1.0f, pApplied.limReleaseMs));
    limiter.setAutoRelease (p.limAutoRelease);
    limiter.setStyle (p.limStyle);
    limiter.setTransientPreserve (p.transientPreserve);
    limiter.setStereoLink (pApplied.stereoLink);
    // ADR-0003 item 6: at >=4x the region signal is already oversampled enough
    // for inter-sample peaks to be sample-visible - read it directly; below
    // that the tap runs its own 4x estimator (8x effective at 2x).
    limiter.setTruePeakMode (p.truePeakMode && osN < 4);
    limiter.setDetectorHpf (pApplied.scHpfFreqHz);
    eq.setTargets (pApplied);
    comp.setPerBlock (pApplied);
    clip.setPerBlock (pApplied);

    const bool eqPre  = appliedEqPos == 0;
    const bool eqPost = ! eqPre;

    if (p.bypass != bypassTarget)
        bypassTarget = p.bypass;   // step size is rate-derived, set in prepare()

    // ---- §2.7 monitor functions (inert under nonRealtime — invariant 10) --
    deltaTarget = p.deltaMonitor && ! p.nonRealtime;
    const bool compOn = p.loudnessComp && ! p.nonRealtime;
    if (p.nonRealtime)
    {
        // SNAP, don't slew: a realtime→offline flip mid-stream must leave no
        // residual monitor gain or delta fade bleeding into the render — the
        // offline output has to be bit-identical with the toggles off.
        monitorGain.setCurrentAndTargetValue (1.0f);
        deltaMix = 0.0f;
    }
    {
        // Measure (frozen while either side is under the absolute gate).
        const float dryM = dryMeter.momentaryLufs();
        const float wetM = wetMeter.momentaryLufs();
        if (dryM > -70.0f && wetM > -70.0f)
            compMeasureDb = juce::jlimit (-24.0f, 6.0f,
                                          dryMeter.shortTermLufs() - wetMeter.shortTermLufs());
        // Predict floor: the deterministic gain lift, GR-corrected by the
        // previous block's DEEPEST reduction (grMinLinear is a per-call
        // minimum, not an average — so the floor is slightly more aggressive
        // than a mean would make it, which is the safe direction for a
        // monitor-only attenuation). The P3 form of §2.7's "expected GR"; the
        // P4 adaptive engine refines it. Only ever attenuation.
        const float grDbNow  = juce::Decibels::gainToDecibels (
                                   grMinLinear.load (std::memory_order_relaxed), -60.0f);
        const float predictDb = -juce::jmax (0.0f, p.inputGainDb + p.limGainDb + grDbNow);
        const float appliedDb = compOn ? juce::jmin (compMeasureDb, predictDb) : 0.0f;
        monitorGain.setTargetValue (juce::Decibels::decibelsToGain (appliedDb));
    }

    // ---- chunked processing: oversize host blocks degrade to extra chunk
    //      overhead, never to unprocessed audio -----------------------------
    grMinThisCall   = 1.0f;
    renderTpMaxCall = 0.0f;
    renderPeakCall  = 0.0f;
    for (int start = 0; start < totalSamples; start += maxBlock)
        processChunk (buffer, start, juce::jmin (maxBlock, totalSamples - start),
                      p, eqPre, eqPost);
    grMinLinear.store (grMinThisCall, std::memory_order_relaxed);
    adaptiveEngine.finishBlock (p.freeze);
    return true;
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

        // The limiter push is NOT applied here: it belongs to the limiter
        // stage, downstream of Clip/Sat (invariant 1, ADR-0002, DESIGN §2.5 —
        // "drives signal into a fixed threshold that equals ceiling"). It is
        // carried per base sample and multiplied in inside the region, after
        // the clipper. Applying it here made the macro's primary push (up to
        // +18 dB) drive the CLIPPER as well, moving its clip point down by the
        // same amount — invisible at defaults, since clipDrive 0 skips the
        // stage entirely, which is why the null tests never saw it.
        pushArr[(size_t) n] = gPush;
        for (int ch = 0; ch < nCh; ++ch)
            staging.setSample (ch, n, std::isfinite (staged[ch]) ? staged[ch] : 0.0f);
        if (++dryWritePos >= dryRingSize)
            dryWritePos = 0;
    }

    // ======== Stages B/C/D - the oversampled region =========================
    // Region = Clipper/Sat -> lookahead line -> limiter (DSP_POLICY inv 5).
    // At Off the region IS the staging block: no filters, no added latency,
    // and the arithmetic below is bit-identical to the pre-OS engine.
    // Stage A only wrote channels [0, nCh); the oversampler processes every
    // PREPARED channel, so a caller handing fewer channels than prepare() was
    // told would filter last block's leftovers in the rest. They cannot reach
    // the output (the region loop and stage E both bound at nCh) — this keeps
    // a future reader from finding garbage there. Stereo in, stereo out is
    // enforced by isBusesLayoutSupported, so this branch is test-only.
    for (int ch = nCh; ch < numChans; ++ch)
        staging.clear (ch, 0, num);

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

        // Limiter push, at its documented place in the chain: after Clip/Sat,
        // before the lookahead line, so the detector and the delayed signal
        // both carry it. Exact-1 skip keeps the null path untouched.
        if (const float gPushNow = pushArr[(size_t) b]; ! juce::exactlyEqual (gPushNow, 1.0f))
            for (int ch = 0; ch < nCh; ++ch)
                frame[ch] *= gPushNow;

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
        grMinThisCall = juce::jmin (grMinThisCall, gains[0], gains[nCh - 1]);

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

        // Advance the §2.8 duck once per base sample. Out: gain follows
        // 0.5(1+cos(pi*phase)) to zero in ~6 ms, then HOLDS at the bottom
        // until the next block top executes the rewire; in: the ~28 ms
        // recovery leg. Idle is gain == 1.0 exactly (the null path).
        if (duckState == DuckState::out)
        {
            duckPhase += duckOutInc;
            if (duckPhase >= 1.0f)
            {
                duckPhase = 1.0f;
                duckGain  = 0.0f;
                duckState = DuckState::bottom;
            }
            else
                duckGain = 0.5f * (1.0f + std::cos (juce::MathConstants<float>::pi * duckPhase));
        }
        else if (duckState == DuckState::in)
        {
            duckPhase += duckInInc;
            if (duckPhase >= 1.0f)
            {
                duckPhase = 0.0f;
                duckGain  = 1.0f;
                duckState = DuckState::idle;
            }
            else
                duckGain = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * duckPhase));
        }
        else if (duckState == DuckState::bottom && bottomHoldSamples > 0)
            --bottomHoldSamples;      // the post-latch refill, counted in processed samples

        // Advance the 2.8 crossfade once per base sample.
        const float targetMix = bypassTarget ? 1.0f : 0.0f;
        if (bypassMix < targetMix)      bypassMix = juce::jmin (targetMix, bypassMix + bypassStep);
        else if (bypassMix > targetMix) bypassMix = juce::jmax (targetMix, bypassMix - bypassStep);
        const float deltaTargetMix = deltaTarget ? 1.0f : 0.0f;
        if (deltaMix < deltaTargetMix)      deltaMix = juce::jmin (deltaTargetMix, deltaMix + deltaStep);
        else if (deltaMix > deltaTargetMix) deltaMix = juce::jmax (deltaTargetMix, deltaMix - deltaStep);
        const float monGainNow = monitorGain.getNextValue();
        float monFrameDry[kMaxChannels] = {};
        float monFrameWet[kMaxChannels] = {};
        float renderFrame[kMaxChannels] = {};

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

            // §2.8 duck — PROCESSED path only, downstream of the clamp (a
            // gain ≤ 1 cannot re-exceed the ceiling), upstream of dither so
            // the export grid stays intact. Exact-1 branch keeps the null.
            if (! juce::exactlyEqual (duckGain, 1.0f))
                processed *= duckGain;

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

            // §2.7 meters (always fed — toggling comp must not start from a
            // cold measure) and the delta crossfade (exact endpoints, so the
            // default path is untouched bit-for-bit).
            {
                monFrameDry[ch] = delayedDry;
                monFrameWet[ch] = processed;
            }
            // The delta leg's dry term carries the SAME duck gain the
            // processed term already has (processed = duckGain·p), so the
            // whole difference scales by duckGain and fades to silence with
            // everything else. Subtracting the FULL dry from a ducked
            // processed did the opposite: delta rose to the undicked dry
            // signal exactly when the transition was meant to be silent.
            // The bypass leg below keeps the pure `delayedDry` — invariant 7
            // is a property of that leg alone.
            const float dryForDelta = juce::exactlyEqual (duckGain, 1.0f)
                                        ? delayedDry : delayedDry * duckGain;
            float wetLeg = processed;
            if (deltaMix >= 1.0f)      wetLeg = dryForDelta - processed;
            else if (deltaMix > 0.0f)  wetLeg = processed
                                              + ((dryForDelta - processed) - processed) * deltaMix;

            // §2.9 RENDER tap: the bypass-mixed programme path with NO delta
            // substitution and NO monitor gain — what an offline render emits
            // (both monitor functions are inert offline, invariant 10). The
            // output meters read THIS, so auditioning Delta or Comp cannot
            // bend the LUFS/dBTP readings or poison the session-cumulative
            // integrated figure and dBTP hold.
            //
            // The §2.8 duck IS included (it is already in `processed`), and
            // that is the deliberate choice: the meters report what the plugin
            // EMITTED, and during a transition it really did emit a fade. The
            // alternative — tapping upstream of the duck — would have the
            // meters describe audio nobody heard. Cost, stated: repeated A/B
            // switching feeds ~34–45 ms of fade per transition into the
            // session-cumulative integrated LUFS, a small downward pull. The
            // −70 LUFS absolute gate discards the silent bottom, so only the
            // fade legs contribute, and the P5 meter-hold reset is the escape
            // hatch for a measurement that must not include them.
            float render;
            if (bypassMix <= 0.0f)      render = processed;                     // exact endpoint
            else if (bypassMix >= 1.0f) render = delayedDry;                    // exact endpoint
            else                        render = processed + (delayedDry - processed) * bypassMix;
            renderFrame[ch] = std::isfinite (render) ? render : 0.0f;

            float out;
            if (bypassMix <= 0.0f)      out = wetLeg;                           // exact endpoint
            else if (bypassMix >= 1.0f) out = delayedDry;                       // exact endpoint
            else                        out = wetLeg + (delayedDry - wetLeg) * bypassMix;

            // §2.7 loudness compensation: POST-mix, so the bypass leg carries
            // the same gain (loudness-matched bypass). Exact skip at unity.
            if (! juce::exactlyEqual (monGainNow, 1.0f))
                out *= monGainNow;

            if (! std::isfinite (out))
            {
                sawNonFinite = true;
                out = 0.0f;
            }
            buffer.setSample (ch, start + n, out);
        }
        dryMeter.processFrame (monFrameDry, nCh);
        wetMeter.processFrame (monFrameWet, nCh);
        adaptiveEngine.pushFrame (monFrameDry, nCh);
        outMeter.processFrame (renderFrame, nCh);
        {
            float tp[kMaxChannels] = {};
            outTp.processFrame (renderFrame, nCh, tp);
            for (int ch = 0; ch < nCh; ++ch)
            {
                renderTpMaxCall = juce::jmax (renderTpMaxCall, tp[ch]);
                renderPeakCall  = juce::jmax (renderPeakCall, std::abs (renderFrame[ch]));
            }
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
