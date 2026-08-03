#include "PluginProcessor.h"
#include "gui/PluginEditor.h"
#include "dsp/Latency.h"

namespace
{
    constexpr int kSchemaVersion = 1;   // ADR-0007: explicit from day one
}

AnabasisAudioProcessor::AnabasisAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ANABASIS", createAnabasisLayout())
{
    cached.resolve (apvts);
    macroEngine   = std::make_unique<MacroEngine> (apvts);
    presetManager = std::make_unique<PresetManager> (apvts, internalState);

    // Two of ADR-0004 item 5's recompute triggers wire here: the three
    // latency-input onChanged callbacks. prepareToPlay and setNonRealtime()
    // are the others. The callback runs on whichever thread mutated the
    // ValueTree (message thread in practice); updateLatency never touches
    // audio-thread state, so this is safe from any non-audio thread.
    internalState.onLatencyInputChanged = [this] { updateLatency(); };

    defaultSlot = saveSlotFromLive();   // pristine defaults (missing-AB read rule)
    storedSlot  = defaultSlot.createCopy();   // slot B starts as a copy of defaults

    // §5.3 detach discriminator (ADR-0005's P5 half — see the header block).
    // The mapper asks the wrapper, never the reverse: the mask is per-slot
    // serialized state and lives here.
    macroEngine->isDetached = [this] (const char* id)
    { return liveDetachMask.contains (juce::String (id)); };
    // Off-message-thread detach/re-engage bits land on the MacroEngine's
    // existing 30 ms tick rather than through a message post of our own
    // (drainDetachBitsSoon explains why that route is closed).
    macroEngine->onDrainTick = [this] { handleAsyncUpdate(); };
    // Both callbacks are wired; only now may the tick that reads them run.
    macroEngine->startDraining();
    // The undo gesture bookkeeping keys one bit per parameter index; ADR-0010
    // freezes the surface at 49, well inside the word.
    jassert (getParameters().size() <= kMaxCountedGestureIndex);
    addListener (this);                       // gesture begin/end
    // The MANAGED nine only. The three macro ids were registered here too and
    // `parameterChanged` discarded every one of those callbacks on its first
    // line (`managedIndexOf` fails for a macro) — three registrations that read
    // as load-bearing and were not, which is how a future edit comes to assume
    // macro writes reach this handler. `MacroEngine` is the macros' listener;
    // the wrapper hears them through the GESTURE callbacks instead, which is
    // where §5.3's re-engage rule actually lives.
    for (const char* id : managed_params::ids)
        apvts.addParameterListener (id, this);
}

AnabasisAudioProcessor::~AnabasisAudioProcessor()
{
    // FIRST, before any member is destroyed — see the declaration.
    macroEngine->stopDraining();

    // Symmetric with the constructor, and deliberately explicit rather than
    // order-dependent. It IS safe implicitly today — `apvts` is declared above
    // every member these callbacks touch, so it outlives them, and nothing
    // changes a parameter during teardown — but "safe because of declaration
    // order" is the argument the startDraining/stopDraining split was written
    // to stop relying on. Deregistering says what the destructor guarantees
    // instead of leaving a reader to re-derive it.
    removeListener (this);
    for (const char* id : managed_params::ids)
        apvts.removeParameterListener (id, this);
}

// The three §5.3 conditions meet here. Gesture callbacks arrive with a raw
// parameter INDEX; the managed set is matched by ID so a layout reorder can
// never silently re-key the discriminator.
static int managedIndexOf (const juce::String& id)
{
    for (int i = 0; i < managed_params::kCount; ++i)
        if (id == managed_params::ids[i])
            return i;
    return -1;
}

void AnabasisAudioProcessor::audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*,
                                                                        int parameterIndex)
{
    // §7 undo: the FIRST open gesture snapshots the pre-state the eventual
    // step will restore. Message thread only — an off-thread gesture (host
    // UI) skips this and its edit folds silently, the automation rule.
    //
    // Open drags are tracked as a BITMASK keyed by parameter index, not as a
    // bare count, because VST3 gesture threading is host-defined (the same
    // premise this class's off-thread degradation path rests on) and the two
    // asymmetries a count cannot survive are opposites:
    //   • begin off the message thread, end on it — a bare `--` closed a
    //     DIFFERENT, still-open drag, pushing its step mid-gesture and clearing
    //     the snapshot, so its real end pushed nothing. Only a bit this handler
    //     set can be cleared, so a foreign end matches nothing.
    //   • begin on the message thread, end off it — the end below clears the
    //     bit on ANY thread (see there), so the mask cannot leak. A count
    //     guarded by the thread test leaked one open drag for ever, and undo
    //     then never fired again for the rest of the session.
    // Only message-thread begins set a bit: an off-thread gesture is invisible
    // to undo by design (it folds into the automation path), and letting it
    // occupy the mask would suppress a concurrent real drag's step.
    //
    // NOTE the deliberate asymmetry with `managedGestureBits` below, which is
    // set and cleared on ANY thread: an off-thread drag DOES detach but does
    // NOT produce an undo step. The two masks answer different questions and
    // §5.3/§7 word them differently on purpose — detachment keys on "the change
    // was gesture-bracketed", which is true whoever delivered it, while an undo
    // step keys on a message-thread drag, because pushing one means copying
    // ValueTrees and that may not happen off the message thread. Stated here
    // because the two lines sit three apart and look like an oversight.
    if (juce::MessageManager::existsAndIsCurrentThread()
        && parameterIndex >= 0 && parameterIndex < kMaxCountedGestureIndex)
    {
        const uint64_t bit = 1ull << parameterIndex;
        // Arming on 0 → non-zero is what makes one drag (or several
        // overlapping ones) exactly one step.
        if (openGestureBits.fetch_or (bit, std::memory_order_relaxed) == 0)
            gesturePreState = saveSlotFromLive();
    }

    if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*> (
            getParameters()[parameterIndex]))
    {
        const auto id = p->getParameterID();
        if (const int m = managedIndexOf (id); m >= 0)
            managedGestureBits.fetch_or (1u << m, std::memory_order_relaxed);
        else if (id == pid::loudness || id == pid::character || id == pid::tone)
        {
            // §5.3: the NEXT macro-knob gesture re-engages everything — the
            // clear must land before the gesture's mapping writes, and both
            // run on the message thread, so the drain below is ordered right.
            pendingReengage.store (true, std::memory_order_relaxed);
            drainDetachBitsSoon();             // never posts off-thread — see there
        }
    }
}

void AnabasisAudioProcessor::audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*,
                                                                      int parameterIndex)
{
    if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*> (
            getParameters()[parameterIndex]))
        if (const int m = managedIndexOf (p->getParameterID()); m >= 0)
            managedGestureBits.fetch_and (~(1u << m), std::memory_order_relaxed);

    // The bit is cleared on WHICHEVER thread the end arrives on — that is the
    // half a thread-guarded count could not do, and its absence stranded an
    // open drag for ever (undo then never fired again). Clearing a bit is a
    // lock-free store; only the ValueTree work below is message-thread-gated,
    // which is the same split the detach discriminator uses.
    const uint64_t bit = parameterIndex >= 0 && parameterIndex < kMaxCountedGestureIndex
                             ? 1ull << parameterIndex : 0ull;
    if (bit == 0)
        return;
    const uint64_t prevOpen = openGestureBits.fetch_and (~bit, std::memory_order_relaxed);

    // Push only when THIS end closed the last open drag (`prevOpen == bit`):
    // an end for a bit we never set — a begin delivered off-thread — matches
    // nothing, and overlapping drags collapse to one step at the last close.
    // An end that arrives off-thread still clears above but pushes nothing:
    // that step is lost, which is the documented automation-path degradation,
    // and the next message-thread begin re-arms from 0 rather than inheriting
    // a stale snapshot.
    if (prevOpen == bit
        && juce::MessageManager::existsAndIsCurrentThread()
        && gesturePreState.isValid())
    {
        // One step per completed drag, and only if something CHANGED —
        // an aborted gesture (press, no move) pushes nothing.
        if (! gesturePreState.isEquivalentTo (saveSlotFromLive()))
            pushUndoStep (gesturePreState);
        gesturePreState = {};
    }
}

void AnabasisAudioProcessor::pushUndoStep (juce::ValueTree preState)
{
    auto& stack = undoStacks[activeSlot];
    stack.add (preState.createCopy());
    while (stack.size() > kUndoCap)
        stack.remove (0);
    redoStacks[activeSlot].clear();      // a new edit invalidates the redo line
}

// §2.8: an undo step is a bulk swap exactly like an A/B switch or a preset
// apply — DSP_POLICY invariant 8's click-free enumeration names it explicitly
// (ADR-0004). It restores a whole StateSet, so it can rewire the discrete
// stages (EQ position, colour model, OS factor), and since ADR-0014 it also
// STAGES the frozen-trim vector, which is only ever applied at the silent
// bottom. Without the request an undo that happens to move no rewire never
// reaches a bottom at all: the staged vector then sat pending indefinitely and
// was injected at the next unrelated duck, into whatever slot was live by then.
void AnabasisAudioProcessor::undo()
{
    auto& stack = undoStacks[activeSlot];
    if (stack.isEmpty())
        return;
    redoStacks[activeSlot].add (saveSlotFromLive());
    const auto prev = stack.removeAndReturn (stack.size() - 1);
    const MacroEngine::ScopedRestore guard (*macroEngine);   // §5.3: not a gesture
    engine.requestForcedDuck();
    applySlotToLive (prev);
}

void AnabasisAudioProcessor::redo()
{
    auto& stack = redoStacks[activeSlot];
    if (stack.isEmpty())
        return;
    undoStacks[activeSlot].add (saveSlotFromLive());
    const auto next = stack.removeAndReturn (stack.size() - 1);
    const MacroEngine::ScopedRestore guard (*macroEngine);
    engine.requestForcedDuck();                              // §2.8, as undo()
    applySlotToLive (next);
}

void AnabasisAudioProcessor::parameterChanged (const juce::String& parameterID, float)
{
    const int m = managedIndexOf (parameterID);
    if (m < 0)
        return;                                   // macros route through MacroEngine —
                                                  // and are no longer REGISTERED here,
                                                  // so this line now guards only an id
                                                  // a future registration might add
    if ((managedGestureBits.load (std::memory_order_relaxed) & (1u << m)) == 0)
        return;                                   // ungestured: automation/restore — never detaches
    if (macroEngine->isApplyingMacro() || macroEngine->isRestoring())
        return;
    pendingDetachBits.fetch_or (1u << m, std::memory_order_relaxed);
    drainDetachBitsSoon();
}

// The bits are set from whichever thread APVTS/the host delivers the callback
// on, and only the message thread may turn them into `liveDetachMask`. On the
// message thread that is immediate; OFF it, the drain WAITS for the MacroEngine
// tick — deliberately, and this is the one line that must not become
// `triggerAsyncUpdate()`. Posting to the platform message queue takes a lock
// and on some platforms allocates, so a callback delivered on the audio thread
// would put both inside `processBlock`: a REALTIME_AUDIO_POLICY hard red line.
// `MacroEngine::parameterChanged` refused to post for exactly this reason and
// leans on its 30 ms timer instead; the detach bits now ride the same tick
// rather than opening a second, riskier route to the same place. Cost of the
// wait: the badge and the serialized mask lag by up to 30 ms on a host that
// delivers gestures off-thread — display latency, never a wrong value.
void AnabasisAudioProcessor::drainDetachBitsSoon()
{
    if (juce::MessageManager::existsAndIsCurrentThread())
        handleAsyncUpdate();
}

void AnabasisAudioProcessor::handleAsyncUpdate()
{
    // Message thread. §5.3: "the next macro-knob gesture re-engages ALL
    // detached params", so a macro gesture that raced a detach WINS — which
    // means the detaches are applied FIRST and the re-engage clears over them,
    // not the other way round. The two orders differ only when both are
    // pending in the same drain, and on a host that delivers callbacks on the
    // message thread that never happens (drainDetachBitsSoon runs one per
    // callback). Off-thread they coexist inside one 30 ms tick — and the
    // previous order let the DETACH win there, leaving a parameter detached
    // through the very gesture the rule says re-engages it. The comment here
    // claimed the correct precedence while the code did the opposite.
    if (auto bits = pendingDetachBits.exchange (0, std::memory_order_relaxed))
        for (int i = 0; i < managed_params::kCount; ++i)
            if ((bits & (1u << i)) != 0)
                liveDetachMask.addIfNotAlreadyThere (managed_params::ids[i]);
    if (pendingReengage.exchange (false, std::memory_order_relaxed))
        liveDetachMask.clear();
}

juce::AudioProcessorParameter* AnabasisAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter (pid::bypass);
}

bool AnabasisAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo();
}

void AnabasisAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    grHistoryRing.reset();
    dbTpMaxHold = -144.0f;
    // Publish the cleared values too, not just the state behind them: without
    // this the six meter atomics keep the previous session's readings until a
    // block completes — and indefinitely if the host prepares without ever
    // processing (a rate change while stopped, a plugin rescan).
    publishSilentMeters();
    updateLatency();
}

// The six published meter atomics, cleared — ONE list, because three sites
// need exactly this and two of them had grown their own copy. Relaxed stores,
// so it is callable from any thread: `prepareToPlay` (host), the block-top
// meter-reset consume (audio) and `setStateInformation` (whichever thread the
// host restores on) all use it. It deliberately does NOT touch `dbTpMaxHold`,
// which is plain audio-thread state and stays with the two callers that own
// that thread.
void AnabasisAudioProcessor::publishSilentMeters() noexcept
{
    pubLufsM.store (anabasis::LoudnessMeter::kSilentLufs, std::memory_order_relaxed);
    pubLufsS.store (anabasis::LoudnessMeter::kSilentLufs, std::memory_order_relaxed);
    pubLufsI.store (anabasis::LoudnessMeter::kSilentLufs, std::memory_order_relaxed);
    pubDbTpMax.store (-144.0f, std::memory_order_relaxed);
    pubPlr.store (0.0f, std::memory_order_relaxed);
    pubGrDb.store (0.0f, std::memory_order_relaxed);
}

void AnabasisAudioProcessor::setNonRealtime (bool isNonRealtime) noexcept
{
    AudioProcessor::setNonRealtime (isNonRealtime);
    nonRealtimeFlag.store (isNonRealtime, std::memory_order_relaxed);
    // The realtime→offline transition is a PDC recompute trigger in its own
    // right (ADR-0004 item 5): at Force Max the reported figure uses the
    // forced 16x factor, and this is the only callback guaranteed to fire.
    updateLatency();
}

void AnabasisAudioProcessor::updateLatency()
{
    // Snapshot the inputs the predictor needs; const and race-free, never
    // mutating audio-thread state (THREADING_POLICY forbidden-access rule).
    anabasis::EngineParameters p;
    p.oversample      = internalState.oversampleFactor();
    p.osPhase         = internalState.osPhaseMode();
    p.forceMaxOffline = internalState.forceMaxOffline();
    p.nonRealtime     = nonRealtimeFlag.load (std::memory_order_relaxed);

    // The 48 kHz fallback covers calls before the first prepareToPlay (a
    // setStateInformation or an int_ change can arrive first), so the figure
    // reported from an unprepared state is a placeholder — 480 samples
    // regardless of the host's eventual rate. prepareToPlay re-reports with
    // the real rate before any audio runs, and wrappers set the rate before
    // calling it, so no host compensates with the placeholder. A future PDC
    // test must therefore assert the reported value only AFTER a prepare.
    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    setLatencySamples (anabasis::predictLatencySamples (p, sr));
}

void AnabasisAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;   // the single FTZ/DAZ mechanism (§1.4)

    // §2.9 meter-hold reset, consumed BEFORE the engine runs so this block's
    // measurement starts clean, and the cleared values are published HERE
    // rather than left to the post-process publish: a short-circuited block
    // (zero-length) returns before that publish, and the reset must not sit
    // invisible until real audio arrives. Same publish set as prepareToPlay.
    if (meterResetPending.exchange (false, std::memory_order_relaxed))
    {
        engine.resetMeterHolds();
        dbTpMaxHold = -144.0f;
        publishSilentMeters();   // superset of the three this needed; see there
    }

    // Build the POD snapshot ONCE per block (ADR-0001/ADR-0011).
    cached.toEngine (snapshot);
    snapshot.oversample      = internalState.oversampleFactor();
    snapshot.osPhase         = internalState.osPhaseMode();
    snapshot.forceMaxOffline = internalState.forceMaxOffline();
    snapshot.nonRealtime     = nonRealtimeFlag.load (std::memory_order_relaxed);

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // A block the engine short-circuited produced no render-tap values:
    // publishing anyway would re-report the previous block's peaks and push a
    // duplicate GR-history entry, breaking the one-entry-per-processed-block
    // property the ring's readers rely on. The engine reports the fact rather
    // than the wrapper re-deriving its early-return condition.
    if (! engine.process (buffer, snapshot))
        return;

    // -- §2.9 metering: publish once per block from the engine's RENDER tap
    // (relaxed atomics — monotonic display data, THREAD_MODEL meter row).
    // NOT from `buffer`: the buffer carries the LISTENING path, which the
    // §2.7 monitor functions alter — metering it made Delta show the
    // difference signal's loudness, Comp the attenuated level, and both
    // permanently biased the session-cumulative integrated LUFS and dBTP
    // hold. The render tap is the programme output (identical to the buffer
    // whenever the monitor functions are off). Same audio thread, right
    // after process(): plain reads, no atomics needed on this side.
    const auto& om = engine.outputLoudness();
    const float blockTpDb = juce::Decibels::gainToDecibels (engine.lastRenderTpMax(), -144.0f);
    dbTpMaxHold = juce::jmax (dbTpMaxHold, blockTpDb);

    // integratedLufs() walks the 751-bin histogram twice (~1500 iterations,
    // bounded and allocation-free) although the figure only moves when a
    // gating block commits, every 100 ms. Caching it in finishSubBlock would
    // remove ~99 % of that at 512-sample blocks — a candidate if the P6 CPU
    // measurement puts metering near DESIGN §9's ≤ 0.5 % allocation.
    const float lufsI = om.integratedLufs();
    pubLufsM.store (om.momentaryLufs(),  std::memory_order_relaxed);
    pubLufsS.store (om.shortTermLufs(),  std::memory_order_relaxed);
    pubLufsI.store (lufsI,               std::memory_order_relaxed);
    pubDbTpMax.store (dbTpMaxHold,       std::memory_order_relaxed);
    // PLR = session true-peak max − integrated loudness (meaningful only once
    // both exist; 0 until then).
    pubPlr.store (lufsI > anabasis::LoudnessMeter::kSilentLufs + 1.0f
                      ? dbTpMaxHold - lufsI : 0.0f,
                  std::memory_order_relaxed);

    const float grDb = juce::Decibels::gainToDecibels (engine.lastBlockMinGain(), -60.0f);
    pubGrDb.store (grDb, std::memory_order_relaxed);
    // ONE entry per processBlock CALL, which is the ring's documented contract
    // — and the span it covers is the HOST's block, not the prepared one. The
    // engine chunks an oversize block internally while `grMinThisCall` and the
    // render peak accumulate across every chunk, so a host running 4096 with
    // 512 prepared publishes one entry describing 85 ms. Correct for a
    // worst-case display and wrong for a time axis drawn as if entries were
    // evenly spaced: the P5 GR-history renderer needs a time base, not an
    // index count. Recorded here rather than in the ring, which cannot know.
    grHistoryRing.push (grDb, engine.lastRenderPeak());
}

juce::AudioProcessorEditor* AnabasisAudioProcessor::createEditor()
{
    return new AnabasisAudioProcessorEditor (*this);
}

// ---------------------------------------------------------------------------
//  Schema v1 (ADR-0007)
// ---------------------------------------------------------------------------
juce::ValueTree AnabasisAudioProcessor::copyStateWithRaw()
{
    // Anamorph ADR-0013's additive exact-`raw` attribute, stamped on EVERY
    // serialised copy of the parameter tree — the session's ANABASIS child
    // AND the AB slots — so every restore path can be raw-exact. Stamping
    // only the top-level copy left A/B switching value-only: log-taper
    // params drift ulps through the pow/log round trip and discrete Raw*
    // params lose their mid-step values.
    auto params = apvts.copyState();
    for (int i = 0; i < params.getNumChildren(); ++i)
    {
        auto node = params.getChild (i);
        if (node.hasType ("PARAM"))
            if (auto* p = apvts.getParameter (node.getProperty ("id").toString()))
                node.setProperty ("raw", (double) p->getValue(), nullptr);
    }
    return params;
}

void AnabasisAudioProcessor::adoptParamsTree (const juce::ValueTree& paramsWithRaw)
{
    // Strip the `raw` overlay BEFORE replaceState — the live tree never
    // carries it (save→load→save must stay byte-identical) — then re-assert
    // from the unstripped copy, which is what makes the restore raw-exact.
    auto stripped = paramsWithRaw.createCopy();
    for (int i = 0; i < stripped.getNumChildren(); ++i)
        stripped.getChild (i).removeProperty ("raw", nullptr);
    apvts.replaceState (stripped);
    reassertFromRaw (paramsWithRaw);
}

juce::ValueTree AnabasisAudioProcessor::saveSlotFromLive()
{
    // The slot serialises the FULL parameter tree, view-tier entries included;
    // the "view state never travels with a slot" rule lives entirely on the
    // apply side (applySlotToLive overwrites those entries from LIVE before
    // adopting). Consequence for later phases: any path that ever adopts a
    // slot tree WITHOUT going through applySlotToLive — an undo stack, the
    // P2 duck-routed swap — silently re-introduces view-tier travel. Route
    // every slot adoption through applySlotToLive, or move the exclusion here
    // first.
    juce::ValueTree slot ("SLOT");
    slot.setProperty ("presetName", livePresetName, nullptr);
    slot.appendChild (copyStateWithRaw(), nullptr);
    if (liveBaseline.isValid())
        slot.appendChild (liveBaseline.createCopy(), nullptr);
    // ADR-0014 capture: with Freeze ON the latched vector IS the published
    // one, so the save reads it live — unless a restore staged for this slot
    // has not been APPLIED yet (a load-then-save with no audio between, or the
    // ~34 ms between the block-top consume and the duck bottom), where the
    // engine's published trims are STALE and the restored copy is the truth:
    // the same mirror rule the ADAPTIVE child follows.
    //
    // The result goes into a LOCAL, never back into `liveFrozenTrims`: this
    // function is also the dirty-marker compare (`presetDirty()`, polled at
    // ~3 Hz by the editor), and a display query that rewrites the mirror would
    // destroy the loaded vector the moment it ran inside that window — the
    // mirror is written by the restore paths only.
    juce::ValueTree frozen = liveFrozenTrims;
    if (apvts.getRawParameterValue (pid::freeze)->load() >= 0.5f
        && ! engine.frozenRestorePending())
    {
        const auto& a = engine.adaptiveForWrapper();
        juce::ValueTree ft ("FROZEN_TRIMS");
        ft.setProperty ("releaseOctaves", (double) a.publishedTrimRelease(), nullptr);
        ft.setProperty ("stereoLink",     (double) a.publishedTrimLink(), nullptr);
        ft.setProperty ("scHpfHz",        (double) a.publishedTrimHpf(), nullptr);
        ft.setProperty ("dynTiltDb",      (double) a.publishedTrimTilt(), nullptr);
        frozen = ft;
    }
    if (frozen.isValid())
        slot.appendChild (frozen.createCopy(), nullptr);
    juce::ValueTree mask ("DETACH_MASK");
    for (const auto& id : liveDetachMask)
    {
        juce::ValueTree m ("PARAM");
        m.setProperty ("id", id, nullptr);
        mask.appendChild (m, nullptr);
    }
    slot.appendChild (mask, nullptr);
    return slot;
}

void AnabasisAudioProcessor::reassertFromRaw (const juce::ValueTree& apvtsTree)
{
    // Anamorph ADR-0013's additive exact-`raw` attribute: the host-session
    // contract is RAW-exact restoration, and discrete Raw* parameters carry
    // mid-step values only through this path.
    for (int i = 0; i < apvtsTree.getNumChildren(); ++i)
    {
        const auto node = apvtsTree.getChild (i);
        if (! node.hasType ("PARAM") || ! node.hasProperty ("raw"))
            continue;
        if (auto* p = apvts.getParameter (node.getProperty ("id").toString()))
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (float) (double) node.getProperty ("raw")));
    }
}

void AnabasisAudioProcessor::applySlotToLive (const juce::ValueTree& slot)
{
    const auto params = slot.getChildWithName ("ANABASIS");
    if (params.isValid())
    {
        // View-tier parameters never travel with a slot (the shared
        // predicate): overwrite the incoming copy with the LIVE values —
        // value from the tree, raw from the parameter itself — so both the
        // replaceState and the raw re-assert leave them untouched.
        auto incoming = params.createCopy();
        for (int i = 0; i < incoming.getNumChildren(); ++i)
        {
            auto node = incoming.getChild (i);
            if (node.hasType ("PARAM") && isViewTierParam (node.getProperty ("id").toString()))
                if (auto live = apvts.state.getChildWithProperty ("id", node.getProperty ("id")); live.isValid())
                {
                    node.setProperty ("value", live.getProperty ("value"), nullptr);
                    if (auto* lp = apvts.getParameter (node.getProperty ("id").toString()))
                        node.setProperty ("raw", (double) lp->getValue(), nullptr);
                }
        }
        adoptParamsTree (incoming);
    }

    livePresetName  = slot.getProperty ("presetName").toString();
    liveBaseline    = slot.getChildWithName ("BASELINE").createCopy();
    liveFrozenTrims = slot.getChildWithName ("FROZEN_TRIMS").createCopy();
    // ADR-0014 (OQ-013 resolved 2026-08-02, owner-approved — the Hard Stop
    // this banner used to carry is LIFTED): a freeze-ON slot's vector is
    // staged to the engine on ADR-0012's row and lands at the duck's silent
    // bottom — the per-slot Freeze memory restoring, MODE inv 3's last gap.
    // The freeze value is read from the freshly adopted surface, not from the
    // incoming tree: adoptParamsTree already applied the read rules.
    if (liveFrozenTrims.isValid()
        && apvts.getRawParameterValue (pid::freeze)->load() >= 0.5f)
        engine.restoreFrozenTrims (
            (float) (double) liveFrozenTrims.getProperty ("releaseOctaves", 0.0),
            (float) (double) liveFrozenTrims.getProperty ("stereoLink", 0.0),
            (float) (double) liveFrozenTrims.getProperty ("scHpfHz", 0.0),
            (float) (double) liveFrozenTrims.getProperty ("dynTiltDb", 0.0));

    juce::StringArray restored;
    if (const auto mask = slot.getChildWithName ("DETACH_MASK"); mask.isValid())
        for (int i = 0; i < mask.getNumChildren(); ++i)
            restored.add (mask.getChild (i).getProperty ("id").toString());
    replaceDetachMask (restored);
}

// The ONLY way the detach mask is replaced, and the reason it is a function
// rather than two lines repeated: dropping the STAGED inputs is half of
// "replace the mask", not a courtesy beside it.
//
// A gestured managed edit delivered off the message thread sits in
// `pendingDetachBits` for up to one MacroEngine tick (nothing may post from
// that callback — see drainDetachBitsSoon), so the tick AFTER a restore, a
// preset apply, a session load or a reset-to-macro would OR that id into the
// mask this call just installed: a parameter silently detached in a state it
// was never edited in, skipped by the mapper from then on, and serialized with
// the slot. Same for a re-engage that raced the same boundary. A restore is not
// a gesture (§5.3), so a gesture racing it belongs to the state being replaced
// — which is exactly what `MacroEngine::ScopedRestore` does to a pending
// mapping on the way out.
//
// It became a function because the rule was first written at ONE of its five
// sites (`applySlotToLive`) and missed the other four; the mask has no other
// writer now, so a sixth site cannot forget it.
void AnabasisAudioProcessor::replaceDetachMask (const juce::StringArray& newMask)
{
    pendingDetachBits.store (0, std::memory_order_relaxed);
    pendingReengage.store (false, std::memory_order_relaxed);
    liveDetachMask = newMask;
}

void AnabasisAudioProcessor::switchToSlot (int newIndex)
{
    newIndex = anabasis::clampAbSlotIndex (newIndex);
    if (newIndex == activeSlot)
        return;
    // An A/B restore is not a macro gesture (§5.3) — held across the whole
    // swap, not dropped after it, so a drain cannot land between the macro
    // values arriving and the abort.
    const MacroEngine::ScopedRestore guard (*macroEngine);
    // §2.8: BEFORE the swap, so the duck's envelope covers the glide the swap
    // starts — all but its first few ms. The request and the parameter writes
    // are separate stores, and the audio thread reads the parameters (snapshot
    // build) before the flag (block top), so a block can adopt the new values
    // and start the out-leg together: the first ~6 ms of the glide then plays
    // at decreasing but non-zero gain. Still band-limited, never a step.
    // Note the two halves of a swap therefore land at different times: the
    // smoothed parameters glide from that first block, while the discrete
    // rewires the same swap carries (eqPosition, colourModel, OS factor) wait
    // for the silent bottom, which is the whole point of the duck. Only the
    // smoothed half is exposed, and only for the out-leg's first samples.
    engine.requestForcedDuck();
    auto newlyStored = saveSlotFromLive();
    applySlotToLive (storedSlot);
    storedSlot = std::move (newlyStored);
    // The dirty datum swaps WITH the slot: `livePresetName` is per-slot, so a
    // single engine-wide baseline described the wrong slot from here on — after
    // applying a preset in B, switching back to A marked A's name against B's
    // baseline.
    std::swap (presetBaseline, storedPresetBaseline);
    activeSlot = newIndex;
}

bool AnabasisAudioProcessor::applyFactoryPreset (int index)
{
    int count = 0;
    const auto* table = PresetManager::factoryPresets (count);
    if (index < 0 || index >= count)
        return false;                          // validated BEFORE the undo bracket
    pushUndoStep (saveSlotFromLive());

    juce::StringArray mask;
    {
        const MacroEngine::ScopedRestore guard (*macroEngine);
        engine.requestForcedDuck();            // §2.8: a preset is a bulk swap

        // Unreachable given the validation above — and deliberately still
        // checked, because the two guards would only diverge silently if one
        // moved. The early return leaves the pushed undo step and the requested
        // duck standing, exactly as `applyPresetFile`'s does and for the same
        // reason: a failed apply may already have written part of the defaults
        // pass, so those values must land under the duck, and the pre-state is
        // precisely what the undo step should restore. Spending a step and a
        // dip on a partial apply is the correct trade; spending them on a
        // rejected INDEX is not, which is why the index is validated before
        // the bracket opens rather than here.
        if (! presetManager->applyFactoryPreset (index, mask))
            return false;
        replaceDetachMask (mask);
        liveBaseline   = {};                   // defaults-based: no macro baseline survives
        livePresetName = table[index].name;
    }   // the guard drops here, DELIBERATELY before the mapping below

    // A FACTORY preset is not a file preset, and this is where they part.
    // A file carries every parameter, managed ones included, so the restore
    // guard is exactly right there: the mapping must not overwrite what the
    // file landed. A factory table is defaults + a handful of intents, and
    // PresetManager's contract is that it "expresses itself through the MACROS
    // plus non-managed parameters wherever possible" — so after the defaults
    // pass the nine §5.5 managed parameters sit at their DEFAULTS and only the
    // macro positions describe the preset. The guard swallowed the mapping
    // those positions armed (its destructor aborts whatever is pending) and
    // nothing re-ran it, so "EDM Club" moved `loudness` to 80 and left the
    // compressor, clipper, limiter and EQ at M(0,0,0): the preset was
    // inaudible. Run the mapping once, here, exactly as `resetToMacro()` does
    // — after the values have landed, outside the guard, and BEFORE the
    // baseline below, or the preset would read as dirty the moment it loaded.
    macroEngine->refreshMapping();

    presetBaseline = saveSlotFromLive();       // dirty marker datum
    return true;
}

bool AnabasisAudioProcessor::applyPresetFile (const juce::File& file)
{
    // §7 preset bracketing: parse BEFORE the bracket opens — an unreadable
    // file must not cost the user an undo step. A partial apply after a
    // successful parse still pushes: the pre-state is exactly what undo
    // should restore in that case. The readability test is PresetManager's own
    // (root tag included), so this gate and the apply cannot disagree — an
    // is-it-XML gate let a foreign root through and charged an undo step for a
    // guaranteed no-op.
    if (! file.existsAsFile() || PresetManager::parsePresetFile (file) == nullptr)
        return false;
    pushUndoStep (saveSlotFromLive());

    const MacroEngine::ScopedRestore guard (*macroEngine);   // §5.3, as above
    // DELIBERATELY before the apply, and NOT undone on failure: a failed
    // applyPreset may still have written some parameters (a partial apply),
    // and those must land under the duck. Moving this after the success check
    // to save a ~34 ms dip on the failure path would reopen the unducked
    // bulk-swap hole INC-001 records.
    engine.requestForcedDuck();                               // §2.8, as above

    juce::StringArray mask;
    if (! presetManager->applyPreset (file, mask))
        return false;                     // the guard still runs: a partial
                                          // apply arms the listeners too
    replaceDetachMask (mask);
    livePresetName = file.getFileNameWithoutExtension();
    presetBaseline = saveSlotFromLive();       // dirty marker datum
    return true;
}

void AnabasisAudioProcessor::resetSlotFieldsToDefaults()
{
    // The §4.4 read rule "missing fields are taken at their defaults" applies
    // to the slot fields too: a valid root WITHOUT an AB child must not leave
    // the previous session's mask/trims/name/slot behind, or the next save
    // serialises a chimera of two sessions.
    activeSlot = 0;
    livePresetName.clear();
    liveBaseline    = juce::ValueTree();
    liveFrozenTrims = juce::ValueTree();
    replaceDetachMask ({});
    storedSlot = defaultSlot.createCopy();
    // The dirty datum is the one slot field that is NOT serialized, so a load
    // cannot restore it — it must be dropped with the rest. Kept, it described
    // a preset applied in the PREVIOUS session and the freshly loaded name
    // rendered as edited (or, as wrongly, as clean). Absent, `presetDirty()`
    // returns false until the next apply or save, which is the honest answer:
    // a session records which preset a slot holds, never whether it had been
    // edited since.
    presetBaseline       = juce::ValueTree();
    storedPresetBaseline = juce::ValueTree();
}

void AnabasisAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("AnabasisRoot");
    root.setProperty ("schemaVersion", kSchemaVersion, nullptr);

    // ANABASIS with the additive exact-`raw` attribute per PARAM.
    root.appendChild (copyStateWithRaw(), nullptr);
    root.appendChild (internalState.state().createCopy(), nullptr);

    juce::ValueTree ab ("AB");
    ab.setProperty ("active", activeSlot, nullptr);   // ADR-0007's field name
    ab.appendChild (activeSlot == 0 ? saveSlotFromLive() : storedSlot.createCopy(), nullptr);
    ab.appendChild (activeSlot == 0 ? storedSlot.createCopy() : saveSlotFromLive(), nullptr);
    root.appendChild (ab, nullptr);

    // ADAPTIVE: "absent = never learned" is the §4.4 discriminator, so the
    // child is written ONLY once Learn has committed targets. The values are
    // audio-thread-written atomics read here on the message thread — stable
    // after the commit, the same capture pattern as the frozen-trim latch.
    //
    // While a restore is still STAGED (loaded, not yet consumed by a block
    // top) the engine's answer is one session out of date, so the staged
    // record is authoritative instead: a host that loads a project and
    // re-saves it without running audio must not lose — or resurrect — a
    // learned reference. Once consumed the two agree.
    //
    // Residual window, stated exactly rather than claimed away: the consumer
    // clears the flag with `exchange` and adopts a few instructions LATER, so
    // a save landing between the two reads `false` here and falls back to the
    // engine's pre-adoption values. Cost is one save's worth of learned
    // references; closing it would need the flag cleared after adoption, which
    // trades this window for a lost-update one (a stage arriving between adopt
    // and clear would be erased). ADR-0012 §Known limits records the choice.
    const bool  restoreStaged = engine.adaptiveRestorePending();
    const auto& ad            = engine.adaptiveForWrapper();
    const bool  learnedNow    = restoreStaged
                                  ? stagedAdaptiveLearned.load (std::memory_order_relaxed)
                                  : ad.hasLearned();
    if (learnedNow)
    {
        juce::ValueTree adaptive ("ADAPTIVE");
        adaptive.setProperty ("refOnsetRate",
                              (double) (restoreStaged
                                            ? stagedRefOnset.load (std::memory_order_relaxed)
                                            : ad.publishedRefOnset()),
                              nullptr);
        adaptive.setProperty ("refTiltDb",
                              (double) (restoreStaged
                                            ? stagedRefTilt.load (std::memory_order_relaxed)
                                            : ad.publishedRefTilt()),
                              nullptr);
        root.appendChild (adaptive, nullptr);
    }

    if (const auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void AnabasisAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;                                        // corrupt input: keep current state
    const auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("AnabasisRoot"))
        return;                                        // foreign input: keep current state
    // schemaVersion 1 is the only generation; missing → treated as 1
    // (structural-tolerance read rules, §4.4).

    // §5.3 again, and this is the path that needs the SCOPE rather than a
    // trailing abort: VST3 does not promise `setStateInformation` arrives on
    // the message thread, so the 30 ms drain timer can fire in the middle of
    // the restore below. See KNOWN_ISSUES KI-003 for what this does and does
    // not cover.
    const MacroEngine::ScopedRestore guard (*macroEngine);
    engine.requestForcedDuck();   // §2.8: a session load is the biggest bulk swap of all
    // §7: undo stacks are session-local and never serialized — a load starts
    // a fresh history for both slots.
    for (int slot = 0; slot < anabasis::kNumAbSlots; ++slot)
    {
        undoStacks[slot].clear();
        redoStacks[slot].clear();
    }
    openGestureBits.store (0, std::memory_order_relaxed);
    gesturePreState = {};
    // The P5 decision THREAD_MODEL left open, taken: a state load CLEARS the
    // session-cumulative meter holds. The integrated LUFS and the dBTP hold
    // describe the programme measured so far, and after a load the programme
    // is a different session's — keeping the old maximum "will read as a bug
    // the first time a meter is visible" (the reviewer's words, and correct).
    // Staged through the same momentary-request row as the GUI button, so the
    // clear lands at a block top like every other cross-thread command — AND
    // published here as well, because a project is ordinarily opened with the
    // transport stopped and no block then runs at all: the request would sit
    // pending while the open editor still showed the previous session's
    // integrated LUFS and dBTP maximum. The engine-side clear still has to
    // wait for its block top (it is engine state); the DISPLAY does not, and
    // the constants are the same ones `prepareToPlay` writes for the same
    // reason. If audio is in fact running, the next block's own publish
    // overwrites this within one block.
    requestMeterReset();
    publishSilentMeters();

    // Same read rule for the parameter tree: a valid root that omits ANABASIS
    // means "defaults", not "keep whatever is live".
    if (const auto params = root.getChildWithName ("ANABASIS"); params.isValid())
        adoptParamsTree (params);
    else
        adoptParamsTree (defaultSlot.getChildWithName ("ANABASIS"));
    internalState.replaceFrom (root.getChildWithName ("ANABASIS_INTERNAL"));

    // Slot fields FIRST go to defaults (the missing-fields read rule), then
    // whatever the AB child actually carries overlays them.
    resetSlotFieldsToDefaults();
    if (const auto ab = root.getChildWithName ("AB"); ab.isValid())
    {
        // Collect SLOT children BY TYPE, never by raw position: the
        // tolerance rules admit unknown children, and indexing ab.getChild(i)
        // directly would let a tolerated foreign child shift both slots.
        juce::Array<juce::ValueTree> slots;
        for (int i = 0; i < ab.getNumChildren(); ++i)
            if (ab.getChild (i).hasType ("SLOT"))
                slots.add (ab.getChild (i));

        activeSlot = anabasis::clampAbSlotIndex ((int) ab.getProperty ("active", 0));
        const auto live   = activeSlot     < slots.size() ? slots[activeSlot]     : juce::ValueTree();
        const auto stored = 1 - activeSlot < slots.size() ? slots[1 - activeSlot] : juce::ValueTree();
        if (stored.isValid())
            storedSlot = stored.createCopy();
        if (live.isValid())
        {
            // The ANABASIS child above is already the live surface; take the
            // slot's non-parameter fields (name/baseline/trims/mask) from AB.
            livePresetName  = live.getProperty ("presetName").toString();
            liveBaseline    = live.getChildWithName ("BASELINE").createCopy();
            liveFrozenTrims = live.getChildWithName ("FROZEN_TRIMS").createCopy();
            juce::StringArray loadedMask;
            if (const auto mask = live.getChildWithName ("DETACH_MASK"); mask.isValid())
                for (int i = 0; i < mask.getNumChildren(); ++i)
                    loadedMask.add (mask.getChild (i).getProperty ("id").toString());
            replaceDetachMask (loadedMask);
        }
    }

    // ADR-0014: a freeze-ON session restores its frozen vector too — the same
    // stage applySlotToLive performs, against the same duck this load already
    // requested. The freeze value is read from the adopted surface (the
    // ANABASIS child above), never the raw tree. liveFrozenTrims doubles as
    // the mirror: while the stage is unconsumed, saveSlotFromLive serialises
    // it instead of the engine's stale published trims.
    if (liveFrozenTrims.isValid()
        && apvts.getRawParameterValue (pid::freeze)->load() >= 0.5f)
        engine.restoreFrozenTrims (
            (float) (double) liveFrozenTrims.getProperty ("releaseOctaves", 0.0),
            (float) (double) liveFrozenTrims.getProperty ("stereoLink", 0.0),
            (float) (double) liveFrozenTrims.getProperty ("scHpfHz", 0.0),
            (float) (double) liveFrozenTrims.getProperty ("dynTiltDb", 0.0));

    // ADAPTIVE read rules: present → restore the learned targets through the
    // mirror pattern (consumed at the next block top); absent → never
    // learned, defaults (§4.4's discriminator).
    // A missing FIELD inside a present child takes its default (§4.4), which
    // here is the factory neutral reference — not var()'s 0.0, which would
    // have the trims chase a reference no programme material can match.
    // The staged record is mirrored here (message thread) so getStateInformation
    // can answer correctly before the next block top consumes it.
    //
    // INVARIANT: the mirror store and the engine stage must stay PAIRED. This
    // is the only site that stages an adaptive record today; a future one (a
    // preset carrying adaptive data, say)
    // that calls restoreLearnedTargets/restoreNeverLearned without updating
    // the mirror would raise `adaptivePending` while the mirror still held the
    // previous record — and getStateInformation, which prefers the mirror
    // exactly while that flag is up, would serialize the stale one. Route any
    // new stager through here, or pair the two stores in a helper first.
    if (const auto adaptive = root.getChildWithName ("ADAPTIVE"); adaptive.isValid())
    {
        const auto onset = (float) (double) adaptive.getProperty (
                               "refOnsetRate", anabasis::AdaptiveEngine::kDefaultRefOnset);
        const auto tilt  = (float) (double) adaptive.getProperty (
                               "refTiltDb", anabasis::AdaptiveEngine::kDefaultRefTilt);
        stagedRefOnset.store (onset, std::memory_order_relaxed);
        stagedRefTilt.store (tilt, std::memory_order_relaxed);
        stagedAdaptiveLearned.store (true, std::memory_order_relaxed);
        engine.restoreLearnedTargets (onset, tilt);
    }
    else
    {
        stagedRefOnset.store (anabasis::AdaptiveEngine::kDefaultRefOnset,
                              std::memory_order_relaxed);
        stagedRefTilt.store (anabasis::AdaptiveEngine::kDefaultRefTilt,
                             std::memory_order_relaxed);
        stagedAdaptiveLearned.store (false, std::memory_order_relaxed);
        engine.restoreNeverLearned();
    }

    // Deliberately the SECOND recompute of this load: replaceFrom's batch
    // already fired one (that is the level testLatencyNotifyIsBatchedAcrossARead
    // pins). This one is belt-and-braces for the rest of the restore body, and
    // costs nothing — setLatencySamples no-ops when the figure is unchanged,
    // which it is, so the host sees at most one PDC change per load either way.
    updateLatency();
}

// ---------------------------------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AnabasisAudioProcessor();
}
