#include "AnabasisEngine.h"
#include "StageTrace.h"

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
    // +1, not +2 like the dry ring below, and the difference is deliberate
    // rather than an oversight: this ring is read at a FIXED offset behind the
    // write (`delayOs`, and `delayOs ≤ ringSizeOs − maxBlock·osN − 1` by the
    // way `ringSizeOs` is computed), so the read slot cannot coincide with the
    // slot being written. The dry ring's offset is a SUM of three quantities
    // including a table lookup, which is what the spare slot there guards. A
    // change to the region's tap offset has no cushion here — size it again
    // rather than assuming one.
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
    specInL.resize ((size_t) maxBlock);
    specInR.resize ((size_t) maxBlock);
    specOutL.resize ((size_t) maxBlock);
    specOutR.resize ((size_t) maxBlock);
    // …and rewind the two spectrum rings with them. The scratch was resized
    // here from the start; the RINGS were not touched, so a host sample-rate
    // change left up to 4096 frames captured at the old rate readable, and
    // `SpectrumView` maps bins through the CURRENT rate — the trace was drawn
    // at the wrong frequencies until the ring refilled (~85 ms of audio). The
    // wrapper clears `grHistoryRing` at `prepareToPlay` for exactly this
    // reason — since 0.1.2 only when the rate or block size actually changed,
    // so a transport-start re-prepare keeps the scrolling timeline; these two
    // were the analyser state that survived.
    specInRing.reset();
    specOutRing.reset();

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
            // such a bump fails a test rather than shipping quietly. The tripwire
            // holds because there is ONE `FetchContent_MakeAvailable(JUCE)` in
            // the whole build: the plugin, AnabasisTests and AnabasisStateTests
            // all link the `juce::` targets from that single tree, so the
            // revision the suite verifies is by construction the revision the
            // release artefacts ship (CMakeLists.txt).
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
    outRms.prepare (sampleRate);
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
    // The PENDING request goes too, and the reason is completeness of the
    // state class rather than a behaviour change: the first block after a
    // reset takes the `! smoothersPrimed` branch, which already discards it.
    // Leaving it set would make that discard load-bearing for a fact stated in
    // another branch — the shape of argument this file has had to correct
    // repeatedly. What this does NOT fix, because the engine has no clock: a
    // request raised while the host has stopped calling processBlock WITHOUT
    // re-preparing (a transport stop in a host that suspends the plugin)
    // survives, and its ~6 ms out / ~28 ms in leg plays over the head of the
    // next take instead of over the swap it was guarding. Bounded to ~34 ms
    // and audible only as a fade-in; ageing it needs a time base the audio
    // thread does not have, so it is a P5 wrapper question (releaseResources
    // and suspendProcessing both see the transition the engine cannot).
    duckRequested.store (false, std::memory_order_relaxed);
    // Edge detector and meter taps are reset state too, so the invariant is
    // local to this function instead of leaning on `smoothersPrimed` (the
    // flip detector) or on the previous session's last block (the GR tap,
    // which the §2.7 predict floor reads at the NEXT block top).
    lastNonRealtime = false;
    grMinLinear.store (1.0f, std::memory_order_relaxed);
    compGrDb.store (0.0f, std::memory_order_relaxed);
    for (int ch = 0; ch < 2; ++ch)
    {
        limGrDbCh[ch].store (0.0f, std::memory_order_relaxed);
        compGrDbCh[ch].store (0.0f, std::memory_order_relaxed);
    }
    dryMeter.reset();
    wetMeter.reset();
    outMeter.reset();
    outRms.reset();
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
    bool duckAsked = duckRequested.exchange (false, std::memory_order_relaxed);

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
    // ADR-0014: consume the staged frozen-trim record at the block top (the
    // ADR-0012 contract); the duck bottom below APPLIES it. Last-writer-wins:
    // a second restore before the bottom overwrites the pending copy.
    if (frozenPending.exchange (false, std::memory_order_acquire))
    {
        // Generation FIRST, payload after — deliberately this order. The
        // writer's settled test is stageSeq == appliedSeq, and only the
        // application below may advance the applied side, so the number
        // stamped here must never be NEWER than the vector it labels. Read
        // last, a stage landing between the payload loads and this one would
        // stamp its generation onto the previous vector and the writer would
        // read "settled" while the published trims were a restore behind.
        // Read first, that same interleaving stamps the OLDER generation, the
        // record stays pending, and the next block top re-consumes it — the
        // self-correcting direction of ADR-0012's Known-limit 3.
        pendingFrozenSeq = frozenStageSeq.load (std::memory_order_relaxed);
        pendingFrozenTrims.releaseOctaves = stagedFrozen[0].load (std::memory_order_relaxed);
        pendingFrozenTrims.stereoLink     = stagedFrozen[1].load (std::memory_order_relaxed);
        pendingFrozenTrims.scHpfHz        = stagedFrozen[2].load (std::memory_order_relaxed);
        pendingFrozenTrims.dynTiltDb      = stagedFrozen[3].load (std::memory_order_relaxed);
        havePendingFrozen = true;
        // The record IS a duck request, asserted HERE rather than by a second
        // store in restoreFrozenTrims. Two stores are two observations: this
        // consume and `duckRequested`'s are a dozen lines apart, so a block
        // could see the record while the request was still invisible and the
        // vector would wait a further block (or, before the request existed at
        // all, for an unrelated duck). Deriving it from the record removes the
        // ordering question instead of tightening it — the bottom is the only
        // landing site, so wanting one is a property OF the record.
        //
        // UNCONDITIONAL, including on a block that is ALREADY at the bottom.
        // There the vector is injected in the bottom branch immediately and
        // `holdForRequest` then keeps the bottom for one extra block (~11 ms)
        // that nothing needs — a real, inaudible side effect of deriving the
        // request from the record, stated so it is not read as an oversight.
        // Clearing `duckAsked` after applying would remove it and reintroduce
        // exactly the ordering question with `duckAskedWhileOut` that deriving
        // it was meant to close: one extra silent block is the cheaper half.
        duckAsked = true;
    }
    // Learn: ONE atomic word, so the command the writer composed arrives whole
    // and exactly once — a code plus a separate flag could be consumed between
    // the writer's two stores and then re-delivered (see requestLearnStart).
    // Commit BEFORE start within a composed command — the reverse order is the
    // defect this replaced (startLearn zeroes the accumulator commitLearn needs).
    if (const int cmd = learnCmd.exchange (kLearnNone, std::memory_order_acquire);
        cmd != kLearnNone)
    {
        if (cmd != kLearnStart)
            adaptiveEngine.commitLearn();
        if (cmd != kLearnCommit)
            adaptiveEngine.startLearn();
    }
    const bool rewireWanted = wantIdx != latchedFactorIdx
                           || (wantIdx >= 0 && wantPh != latchedPhaseIdx)
                           || wantEq != appliedEqPos
                           || wantModel != appliedModel;

    // ENTERING offline is a RESET-class event, not an audible transition: the
    // render starts there, the host re-reads PDC across it (setNonRealtime is
    // an ADR-0004 recompute trigger) and, at Force Max, effectiveFactor
    // changes with it. Ducking would fade the HEAD OF A BOUNCE — ~45 ms of
    // envelope written into the rendered file — for a transition no one is
    // listening to. Adopting directly makes the no-re-prepare path behave
    // exactly like the re-prepare path most hosts take.
    //
    // The RETURN edge is the opposite case and must NOT share this branch:
    // offline→realtime lands in live playback, where the same direct adopt
    // clears the lookahead ring at FULL gain — ~11 ms of silence followed by
    // an abrupt resumption, which is the click invariant 8 names for exactly
    // this switch. It goes through the duck like any other factor rewire.
    const bool enteringOffline = p.nonRealtime && ! lastNonRealtime;
    lastNonRealtime = p.nonRealtime;

    if (! smoothersPrimed || enteringOffline)
    {
        if (wantIdx != latchedFactorIdx || (wantIdx >= 0 && wantPh != latchedPhaseIdx))
            latchOsConfig (wantIdx, wantPh);
        if (wantEq != appliedEqPos)
        {
            // Paired with the position change on THIS branch too, exactly as
            // at the silent bottom: the biquad history belongs to the stream
            // the EQ was processing (MasteringEQ::resetState). Harmless while
            // this branch was only reachable straight after reset() — the
            // offline-entry edge reaches it mid-stream, with charged state.
            appliedEqPos = wantEq;
            eq.resetState();
        }
        appliedModel = wantModel;
        if (havePendingFrozen)
        {
            adaptiveEngine.injectTrims (pendingFrozenTrims);   // ADR-0014
            frozenAppliedSeq.store (pendingFrozenSeq, std::memory_order_release);
            havePendingFrozen = false;
        }
        duckState = DuckState::idle;
        duckGain  = 1.0f;
        // A render that STARTS with an empty pipeline is not a transition:
        // the reported latency is the promise that those samples are absent,
        // so no hold, and nothing is owed from before the reset. `duckAsked`
        // is consumed above and DELIBERATELY discarded here, in BOTH cases
        // this branch now covers:
        //   • first block after prepare/reset — a request that arrived before
        //     any audio (a setStateInformation ahead of prepareToPlay, which
        //     updateLatency's 48 kHz fallback shows is a real ordering) has
        //     nothing to fade;
        //   • entering offline — a wrapper bulk swap (A/B, preset, session
        //     load) landing on the exact block the host flips to non-realtime
        //     loses its cover and steps at full gain. Bounded to the first
        //     sample of a render and recorded in KNOWN_ISSUES KI-004, because
        //     the alternative — carrying a monitor fade into the head of a
        //     bounce — is worse.
        // The three requests that must NOT be dropped are handled below.
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
        if (havePendingFrozen)
        {
            // ADR-0014: the restored vector lands at the silent bottom, like
            // every other restore-driven discontinuity (DESIGN §7). Freeze is
            // ON in the slot that staged it, so finishBlock holds the vector
            // from here on — this is the per-slot Freeze memory restoring.
            adaptiveEngine.injectTrims (pendingFrozenTrims);
            // Only NOW is the published vector the restored one — the writer's
            // capture guard reads this, not the block-top flag.
            frozenAppliedSeq.store (pendingFrozenSeq, std::memory_order_release);
            havePendingFrozen = false;
        }

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

    // ---- Invariant 9, the unconditional per-block repairs -----------------
    // POSITION IS LOAD-BEARING: everything below reads state these calls
    // repair — `currentTrims()` on the next line most of all — so they run
    // FIRST. The alternative, repairing at the end of the block, was how a
    // ruined Learn pass reached commitLearn(): a consumer that runs at the
    // block top sees the previous block's state, and "the sweep happens later
    // in the same function" is a fact about call order that nothing enforces.
    //
    // These stages need an unconditional check rather than the recovery flag
    // because their corruption produces NO non-finite audio to detect: the
    // §2.7/§2.9 meters and the §5.4 feature extractor emit no audio at all —
    // NaN readings compare false against every gate, so the loudness
    // compensation freezes, the integrated histogram stops accumulating, and
    // the trim vector holds its last value for the session while looking
    // entirely plausible. A few comparisons per block each. `outTp` is
    // deliberately absent: its history is FIR, so a poisoned entry flushes
    // itself in 12 samples. The limiter left this list with its detector HPF
    // (0.1.2, ADR-0023) — its detector is now the tapped magnitude itself,
    // which the ring writes keep finite, so it has no recursive detector
    // state whose corruption could hide.
    dryMeter.sanitiseState();
    wetMeter.sanitiseState();
    outMeter.sanitiseState();
    outRms.sanitiseState();
    adaptiveEngine.sanitiseState();

    // §5.4 adaptive trims: bounded deltas around the CURRENT values, applied
    // to the per-block settings only — never parameter writes, never
    // lookahead or the OS factor (policy inv 4). All four are inert when
    // their host stages are inert, which is why the bit-exact null test runs
    // with adaptation LIVE. A ceiling lock is irrelevant here (ceiling is not
    // a trim member); the §9 lockable set is {ceiling} in v1.
    {
        const auto& t = adaptiveEngine.currentTrims();
        // ADR-0013 (OQ-016 resolved 2026-08-02, owner-approved): the release
        // trim reaches BOTH release paths — the manual time below, and the
        // auto poles through setAutoReleaseScale — so the §5.4 behaviour is
        // audible at factory defaults (auto ON). One factor, computed once —
        // it said that while calling `pow` twice, which is both the comment
        // drifting from the code and the only measurable part of this block.
        // Still UNCONDITIONAL (no manual/auto test, no changed-since-last-block
        // gate): the call is idempotent and factor 1.0 reproduces the prepared
        // alphas exactly, so the invariant-7 bit-exact null holds with
        // adaptation live — and a gate would be a second piece of state that
        // has to agree with the first, for one `pow` and two `exp` per block.
        const float releaseScale = std::pow (2.0f, t.releaseOctaves);
        pApplied.limReleaseMs = juce::jlimit (1.0f, 1000.0f, p.limReleaseMs * releaseScale);
        limiter.setAutoReleaseScale (releaseScale);
        pApplied.stereoLink   = juce::jlimit (0.0f, 1.0f, p.stereoLink + t.stereoLink);
        // The scHpf trim changes a DETECTOR rather than a gain — the COMP's
        // detector only, since 0.1.2 (ADR-0023). Until then the shared
        // sidechain HPF also fed the LIMITER's detector, and this trim
        // engaging it at factory defaults was one of the round's field bugs
        // twice over: LF peaks were under-reported (bass transients reached
        // the unconditional CeilingClamp as hard clips instead of being
        // limited) AND the filter's transient overshoot over-reported them by
        // up to ~6 dB, drawing gain reduction on material whose samples never
        // crossed the ceiling. The comp's copy of the filter is magnitude-
        // clamped to a raw-magnitude ceiling for the same reason (MasteringComp's
        // detector comment).
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
        // The GATE reads momentary (400 ms) while the VALUE is a short-term
        // (3 s) difference — deliberate, not a mismatch: the gate answers "is
        // there programme here at all", which momentary decides four times
        // sooner. Between 0.4 s and 3 s after a prepare both short-term
        // readings are still kSilentLufs, so their difference is exactly 0 and
        // only the predict floor acts — which is the documented split (predict
        // engages in one block, measure needs seconds). The two meters advance
        // in lockstep on the same frame count, so one can never be valid while
        // the other returns the −100 sentinel.
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
    grMinThisCallCh[0] = grMinThisCallCh[1] = 1.0f;
    renderTpMaxCall = 0.0f;
    renderPeakCall  = 0.0f;
    for (int start = 0; start < totalSamples; start += maxBlock)
        processChunk (buffer, start, juce::jmin (maxBlock, totalSamples - start),
                      p, eqPre, eqPost);
    grMinLinear.store (grMinThisCall, std::memory_order_relaxed);
    compGrDb.store (comp.currentGainReductionDb(), std::memory_order_relaxed);
    // Per-channel per-stage copies (0.1.2 item 12) — the same meter row, one
    // store per channel per block.
    for (int ch = 0; ch < 2; ++ch)
    {
        limGrDbCh[ch].store (juce::Decibels::gainToDecibels (grMinThisCallCh[ch], -60.0f),
                             std::memory_order_relaxed);
        compGrDbCh[ch].store (comp.currentGainReductionDb (ch), std::memory_order_relaxed);
    }
    adaptiveEngine.finishBlock (p.freeze);
    return true;
}

void AnabasisEngine::processChunk (juce::AudioBuffer<float>& buffer, const int start,
                                   const int num, const EngineParameters& p,
                                   const bool eqPre, const bool eqPost) noexcept
{
    const int nCh = juce::jmin (buffer.getNumChannels(), wetRing.getNumChannels());
    bool sawNonFinite = false;

    // Set only where a value that ENTERED a stage finite comes out non-finite,
    // i.e. where the chain generated the contamination itself rather than
    // receiving it. Non-finite INPUT is zeroed before any state sees it, so it
    // never sets this; overflow inside a recursive stage does, and that stage
    // then holds a NaN no boundary can wash out. See the invariant 9 block.
    bool stageGeneratedNonFinite = false;

    // The same, narrowed to the ONE stage whose state cannot be inspected or
    // repaired value by value: the oversampler is JUCE's, so the only repair
    // available is a full reset, and a full reset is a discontinuity that must
    // not fire for a fault another stage caused.
    bool regionInputNonFinite = false;

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
            ANABASIS_TRACE (anabasis::StageTrace::rawIn, ch, in);
            if (! std::isfinite (in))
                sawNonFinite = true;

            float s = juce::exactlyEqual (gIn, 1.0f) ? in : in * gIn;
            if (! std::isfinite (s))
                s = 0.0f;                      // filter state must never eat a NaN
            // §2.9 spectrum tap 1: post-InputGain, pre-everything-else. The
            // L/R selection is exhaustive only while the engine is at most
            // stereo; the static_assert that says so lives beside the scratch
            // declarations in the header, where a widening would start.
            (ch == 0 ? specInL : specInR)[(size_t) n] = s;
            if (eqPre)
                s = eq.processSample (ch, s);
            if (! std::isfinite (s))
            {
                // The EQ overflowed on a finite-but-astronomical sample (its
                // biquads carry gain). Without this the post-EQ value reached
                // the compressor unchecked: |x| = inf makes `levelDb` inf and
                // the GR envelope -inf, and the NEXT sample's -inf + inf is a
                // NaN the envelope keeps for ever — permanent silence.
                s = 0.0f;
                stageGeneratedNonFinite = true;
            }
            staged[ch] = s;
            ANABASIS_TRACE (anabasis::StageTrace::postEqPre, ch, s);

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
        {
            if (! std::isfinite (staged[ch]))
            {
                staged[ch] = 0.0f;             // the compressor's own arithmetic
                stageGeneratedNonFinite = true;
            }
            ANABASIS_TRACE (anabasis::StageTrace::compOut, ch, staged[ch]);
            staging.setSample (ch, n, staged[ch]);
        }
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
        {
            frame[ch] = region.getSample (ch, i);
            ANABASIS_TRACE (anabasis::StageTrace::regionIn, ch, frame[ch]);
            if (! std::isfinite (frame[ch]))
            {
                frame[ch] = 0.0f;              // the oversampler's own filters
                stageGeneratedNonFinite = true;
                regionInputNonFinite    = true;
            }
        }

        clip.processSample (frame, nCh);       // Clipper/Sat, inside the region
        for (int ch = 0; ch < nCh; ++ch)
            ANABASIS_TRACE (anabasis::StageTrace::clipOut, ch, frame[ch]);

        // Limiter push, at its documented place in the chain: after Clip/Sat,
        // before the lookahead line, so the detector and the delayed signal
        // both carry it. Exact-1 skip keeps the null path untouched.
        //
        // The value is per BASE sample, held across all osN region samples —
        // a zero-order hold, i.e. a piecewise-constant modulator rather than
        // the band-limited per-sample gain it was at base rate. While the
        // 20 ms glide runs that puts images around multiples of the base rate,
        // all above the decimation filter's cutoff, so they are removed on the
        // way down and the steady state is unaffected. Recorded because the
        // ceilArr/wArr/pushArr idiom is the obvious one to reuse for any
        // future level-affecting control inside the region, where a slower
        // glide or a higher factor could put an image under the cutoff.
        //
        // For `ceilArr` the same hold is LOAD-BEARING, not a cost: the region's
        // gain computer and stage E's CeilingClamp must use the SAME
        // instantaneous ceiling, which is what CeilingClamp's header promises
        // ("a backstop, never a second differently-timed threshold"). The
        // clamp runs at base rate on the decimated signal, so interpolating
        // `ceilArr` across the region would give the two a different threshold
        // per sample and break that contract. Do not "improve" it.
        if (const float gPushNow = pushArr[(size_t) b]; ! juce::exactlyEqual (gPushNow, 1.0f))
            for (int ch = 0; ch < nCh; ++ch)
                frame[ch] *= gPushNow;

        for (int ch = 0; ch < nCh; ++ch)
        {
            if (! std::isfinite (frame[ch]))
            {
                frame[ch] = 0.0f;              // ClipSat's own polynomial/ADAA
                stageGeneratedNonFinite = true;
            }
            ANABASIS_TRACE (anabasis::StageTrace::wetRingWrite, ch, frame[ch]);
            wetRing.setSample (ch, writePosOs, frame[ch]);
        }

        // Detector tap (the LookaheadLimiter CONTRACT): feed the sample that
        // plays W steps from now - written (delayOs - wOs) steps ago. W is
        // the SMOOTHED window scaled to the region rate, so a lookahead move
        // slides the tap instead of jumping it (invariant 8).
        //
        // While that glide runs the tap advances by more or less than one
        // sample per step, so the sequence handed to the detector has a
        // duplicated or skipped sample in it. For the peak wedge that is the
        // documented coverage cost of a moving window. In TRUE-PEAK mode it
        // also reaches the estimator's 12-tap history, which interpolates
        // across a discontinuous sequence rather than a resampled one — but
        // that history is FIR, so the error flushes itself within kTaps frames
        // instead of persisting. (This paragraph argued about a RECURSIVE
        // detector high-pass until 0.1.2: ADR-0023 removed the limiter's
        // biquad outright, so there is no filter state on this path left to
        // see the discontinuity at all, and the bounded-spectral-error
        // reasoning went with it.) Invariant 4 is untouched (the clamp is
        // downstream and unconditional). Recorded rather than fixed: reading
        // the ring at a fractional position would put an interpolator in the
        // detector path to remove an error smaller than the window change that
        // caused it.
        int detPos = writePosOs - (delayOs - wOs);
        if (detPos < 0)
            detPos += ringSizeOs;
        float tapped[kMaxChannels] = {};
        for (int ch = 0; ch < nCh; ++ch)
        {
            tapped[ch] = wetRing.getSample (ch, detPos);
            ANABASIS_TRACE (anabasis::StageTrace::detectorTap, ch, tapped[ch]);
        }

        float gains[kMaxChannels] = { 1.0f, 1.0f };
        limiter.processSample (tapped, nCh, wOs, ceilingNow, gains);
        // GR TAP SCOPE, since the name does not say it: this is the LIMITER's
        // reduction, not the chain's. `MasteringComp::currentGainReductionDb()`
        // exists and is read only by the tests, so the published `pubGrDb`, the
        // GR history ring and the §2.7 predict floor
        // (`inputGainDb + limGainDb + grDbNow`) all describe the limiter alone
        // — the floor therefore UNDER-estimates the lift whenever the
        // compressor is doing the work, and `min(measure, predict)` hides that
        // once the measure converges. A P5 item (the meter legend has to say
        // which reduction it is showing) rather than a defect today.
        grMinThisCall = juce::jmin (grMinThisCall, gains[0], gains[nCh - 1]);
        for (int ch = 0; ch < nCh; ++ch)
            grMinThisCallCh[ch] = juce::jmin (grMinThisCallCh[ch], gains[ch]);

        int readPos = writePosOs - delayOs;
        if (readPos < 0)
            readPos += ringSizeOs;
        for (int ch = 0; ch < nCh; ++ch)
        {
            const float delayedWet = wetRing.getSample (ch, readPos);
            ANABASIS_TRACE (anabasis::StageTrace::delayedWet, ch, delayedWet);
            ANABASIS_TRACE (anabasis::StageTrace::limiterGain, ch, gains[ch]);
            const float limited = juce::exactlyEqual (gains[ch], 1.0f) ? delayedWet
                                                                       : delayedWet * gains[ch];
            ANABASIS_TRACE (anabasis::StageTrace::postLimiter, ch, limited);
            region.setSample (ch, i, limited);
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
            ANABASIS_TRACE (anabasis::StageTrace::stageEIn, ch, processed);
            if (! std::isfinite (processed))
            {
                // Only reachable with oversampling ON: stage A's write to
                // `staging` is sanitised, so at OS Off this value is finite by
                // construction. With the region active this is what came out
                // of processSamplesDown — the DECIMATION half of the same
                // polyphase filters the region read guards, and the same
                // recursive IIR on the default path. Attributed to the
                // oversampler for that reason, so the reset below repairs it.
                processed               = 0.0f;
                stageGeneratedNonFinite = true;
                regionInputNonFinite    = true;
            }

            // Post-position EQ sits AFTER the limiter and BEFORE the clamp -
            // the placement ADR-0002 exists for: a +12 dB post shelf can push
            // the limited signal back over the ceiling, and the clamp being
            // downstream is what keeps invariant 4 unconditional.
            if (eqPost)
            {
                processed = eq.processSample (ch, processed);
                if (! std::isfinite (processed))
                {
                    // The Post EQ CAN overflow, and the previous claim that it
                    // could not was wrong in a way worth keeping written down:
                    // the argument was "its input is the limited signal,
                    // bounded by the ceiling". The limiter's ATTACK is what
                    // bounds it, and at a short lookahead the envelope only
                    // reaches ~0.29 by the time the peak plays (0.4 ms attack
                    // at the default transientPreserve, ~5 samples of window
                    // at 0.1 ms). A fully boosted EQ multiplies by ~3.4, and
                    // 0.29 × 3.4 > 1 — measured, permanently silent, and now a
                    // case in testExtremeLevelDoesNotSilencePermanently.
                    // CeilingClamp cannot stand in for this boundary either:
                    // it maps +inf to the ceiling (so the sample looks fine)
                    // and passes NaN straight through, and it is downstream of
                    // the EQ's own poisoned history in any case.
                    processed               = 0.0f;
                    stageGeneratedNonFinite = true;
                }
            }
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
            // processed did the opposite: delta rose to the unducked dry
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
            // §2.9 spectrum tap 2: post-chain — the same render the meters read.
            (ch == 0 ? specOutL : specOutR)[(size_t) n] = renderFrame[ch];

            // Bypass crossfade. The SECOND leg downstream of dither (the §2.7
            // monitor gain below is the other), and unlike that one it is not
            // monitor-only — it runs in a render too. Both endpoints are exact
            // branches, so a steady state is on the quantisation grid either
            // way; the ~10 ms ramp between them is a convex combination of a
            // dithered wet leg and the UNDITHERED dry, so those samples are
            // not. Scoped in DSP_POLICY invariant 12 rather than fixed: moving
            // the crossfade upstream of dither would put the dry leg THROUGH
            // the quantiser, and invariant 7 requires bypass to be a bit-exact
            // null. A ~10 ms off-grid ramp on an audition toggle is the
            // cheaper of the two.
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
            ANABASIS_TRACE (anabasis::StageTrace::finalOut, ch, out);
            buffer.setSample (ch, start + n, out);
        }
        dryMeter.processFrame (monFrameDry, nCh);
        wetMeter.processFrame (monFrameWet, nCh);
        adaptiveEngine.pushFrame (monFrameDry, nCh);
        outMeter.processFrame (renderFrame, nCh);
        outRms.processFrame (renderFrame, nCh);      // §2.9 stats row (ADR-0020)
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

    // §2.9 spectrum publication: one release-store per ring per CHUNK, which is
    // more than once per host block whenever the host delivers more than the
    // prepared size — the caller runs this ⌈blockSize / preparedMax⌉ times.
    // THREADING_POLICY's SPSC row is worded around the committed unit for
    // exactly this reason: the guarantee is "every frame below the acquired
    // index is complete", which holds per chunk as it does per block, since the
    // payload is written before the index is released. What differs is only the
    // reader's "has anything new arrived?" cadence. Mono sources duplicate L into R via
    // the stage loops above writing only ch 0 — harmless for a stereo-only
    // plugin (isBusesLayoutSupported pins 2×2).
    //
    // What this publication ASSUMES, stated because it is invisible from here:
    // both stage loops run the full `num` samples with no early `continue`, so
    // every element of the scratch below `num` was written THIS chunk. An early
    // exit added to stage A or E would publish the previous chunk's tail as if
    // it were current — a display artefact, not a signal one, but a silent one.
    // If such an exit is ever added, publish only the samples actually written.
    // A SECOND thing this publication assumes, beside the no-early-exit one
    // above: that the chunk it publishes is one the engine will keep. It runs
    // BEFORE the invariant-9 self-heal below, so a chunk the heal then decides
    // was contaminated has already reached the GUI. Nothing non-finite gets
    // through — the taps are individually scrubbed (stage A's `s = 0.0f`, stage
    // E's `isFinite(render) ? render : 0.0f`), so the FFT cannot be poisoned —
    // what reaches it is a chunk of substituted zeros for audio that is about to
    // be reset. Display-only and self-correcting within one analysis window;
    // recorded here so the assumption list is complete rather than half-stated.
    specInRing.pushBlock (specInL.data(), nCh > 1 ? specInR.data() : specInL.data(), num);
    specOutRing.pushBlock (specOutL.data(), nCh > 1 ? specOutR.data() : specOutL.data(), num);

    // ---- Invariant 9 self-heal -------------------------------------------
    // TWO failure modes, repaired separately, because they are not the same
    // failure: contamination that ARRIVES (a non-finite input sample) and
    // contamination the chain GENERATES (a finite sample a stage overflows on).
    //
    // THE SANITISATION BOUNDARIES — each replaces a non-finite value with
    // 0.0f, so no recursive state DOWNSTREAM of one absorbs it:
    //   • stage A, before the EQ:      `if (! isfinite (s)) s = 0.0f`
    //   • stage A, after the EQ:       protects the compressor
    //   • the dry ring write:          `isfinite (in) ? in : 0.0f`
    //   • the staging write:           the compressor's own output
    //   • the region read:             protects ClipSat
    //   • the wet ring write:          ClipSat's own output
    //   • the staging read in stage E: the decimation filters' own output
    //   • stage E, after the EQ:       the Post position's own output
    //   • the render tap:              `isfinite (render) ? … : 0.0f`
    //   • the output write:            sets `sawNonFinite` and emits 0.0f
    // A boundary bounds PROPAGATION and nothing else. It does not protect the
    // stage that produced the value, and every stage here can produce one from
    // a perfectly legal float: the EQ biquads carry gain, the compressor's RMS
    // detector squares, ClipSat's colour model raises to the fifth power, the
    // oversampler's polyphase filters carry gain. Each then holds a NaN for
    // ever — the boundary keeps emitting 0.0f, which reads to a user as the
    // plugin having gone silent permanently. So the boundaries that substitute
    // also RECORD (`stageGeneratedNonFinite`), and the stages that can poison
    // themselves are repaired. Non-finite INPUT is zeroed before the EQ, so it
    // never triggers this: a hostile host buffer still costs no state.
    //
    // REPAIR IS VALUE-LEVEL, not a reset, everywhere it can be: `sanitiseState`
    // clears the members that are actually non-finite and carries the rest, so
    // a poisoned detector filter does not cost the compressor its gain-reduction
    // envelope. The oversampler is the one stage that cannot be repaired that
    // way (JUCE's state, and its default path is recursive), so it gets a full
    // reset — and only on the flag that means the oversampler itself produced
    // the value, never on another stage's fault.
    //
    // WHY THE LIMITER IS SEPARATE: its wedge is index-keyed state, not a
    // filtered value. A sample that arrives finite but pathological (a
    // legitimately huge peak, or one written just before a boundary sanitised
    // its successor) stays the window maximum until its index expires, so the
    // structure needs explicit repair rather than time — on EITHER flag. That
    // is what resetWindow() is for, and why carrying the envelope across it is
    // right (a snap to unity is the louder bug — see the test of that name).
    //
    // RULE FOR FUTURE STAGES, since the chain keeps growing recursive state:
    // a new stateful stage must sit downstream of a boundary (add one if it
    // does not) AND, if any finite input can make it produce a non-finite
    // value, be added to the reset below. The first alone was the bug this
    // pair of blocks fixes; "the input is probably finite" is neither.
    if (sawNonFinite || stageGeneratedNonFinite)
        limiter.resetWindow();
    if (stageGeneratedNonFinite)
    {
        // SANITISE, do not reset: each of these clears the state that is
        // actually non-finite and carries the rest, which is the same rule
        // resetWindow() follows for the limiter (discard the wedge, carry the
        // envelope). A blanket reset would snap the compressor's gain-reduction
        // envelope to unity on a fault its detector filter caused, which is the
        // exact effect the limiter path exists to avoid. The smoothed CONTROLS
        // are untouched throughout — nothing is un-primed, so nothing steps.
        eq.sanitiseState();
        comp.sanitiseState();
        clip.sanitiseState();
    }
    // The dither error-feedback pair, swept on EITHER flag (0.1.2): while
    // dither + shaping are ON this is per-channel recursive state — the one
    // such state the lists above never covered — and a non-finite value in it
    // makes every later `y − x` non-finite too, i.e. permanent one-channel
    // silence behind the output boundary's zeros. Unreachable today (`x` is
    // clamp-bounded and the quantiser cannot overflow it), but "the input is
    // probably finite" is exactly the argument the rule above rejects. Two
    // isfinite checks per contaminated chunk.
    if (sawNonFinite || stageGeneratedNonFinite)
        for (auto& e : ditherErr)
            if (! std::isfinite (e))
                e = 0.0f;
    if (regionInputNonFinite && osActive != nullptr)
    {
        // The oversampler is the exception to the rule above, twice over: its
        // state is JUCE's and cannot be repaired value by value, and the
        // DEFAULT min-phase path is polyphase IIR — allpass sections that feed
        // themselves, so one infinite state makes every later region sample
        // non-finite and the boundary above turns that into permanent silence.
        // (The linear-phase path is FIR and flushes itself in a few samples;
        // the reset is harmless there and the branch is not worth splitting.)
        // reset() clears the stage buffers only — no allocation, no latency
        // change — and is the same call latchOsConfig makes on this thread.
        osActive->reset();
    }
}

} // namespace anabasis
