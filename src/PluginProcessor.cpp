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
    updateLatency();
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

    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    setLatencySamples (anabasis::predictLatencySamples (p, sr));
}

void AnabasisAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;   // the single FTZ/DAZ mechanism (§1.4)

    // Build the POD snapshot ONCE per block (ADR-0001/ADR-0011).
    cached.toEngine (snapshot);
    snapshot.oversample      = internalState.oversampleFactor();
    snapshot.osPhase         = internalState.osPhaseMode();
    snapshot.forceMaxOffline = internalState.forceMaxOffline();
    snapshot.nonRealtime     = nonRealtimeFlag.load (std::memory_order_relaxed);

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    engine.process (buffer, snapshot);
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
    juce::ValueTree slot ("SLOT");
    slot.setProperty ("presetName", livePresetName, nullptr);
    slot.appendChild (copyStateWithRaw(), nullptr);
    if (liveBaseline.isValid())
        slot.appendChild (liveBaseline.createCopy(), nullptr);
    if (liveFrozenTrims.isValid())
        slot.appendChild (liveFrozenTrims.createCopy(), nullptr);
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
    // OQ-013 HARD STOP: liveFrozenTrims is restored as SESSION DATA only.
    // The message→audio inject transport is undecided; nothing here may push
    // these values toward the engine until the OQ-013 ADR lands.

    liveDetachMask.clear();
    if (const auto mask = slot.getChildWithName ("DETACH_MASK"); mask.isValid())
        for (int i = 0; i < mask.getNumChildren(); ++i)
            liveDetachMask.add (mask.getChild (i).getProperty ("id").toString());
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
    // P1 form: plain swap on the message thread. TODO(P2): route through the
    // §2.8 forced duck (requestDuck() BEFORE the swap) once the transition
    // layer exists — a bulk swap without it is a click-free-invariant hole
    // that pluginval will not catch but invariant 8's per-path test will.
    auto newlyStored = saveSlotFromLive();
    applySlotToLive (storedSlot);
    storedSlot = std::move (newlyStored);
    activeSlot = newIndex;
}

bool AnabasisAudioProcessor::applyPresetFile (const juce::File& file)
{
    const MacroEngine::ScopedRestore guard (*macroEngine);   // §5.3, as above

    juce::StringArray mask;
    if (! presetManager->applyPreset (file, mask))
        return false;                     // the guard still runs: a partial
                                          // apply arms the listeners too
    liveDetachMask = mask;
    livePresetName = file.getFileNameWithoutExtension();
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
    liveDetachMask.clear();
    storedSlot = defaultSlot.createCopy();
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

    // ADAPTIVE is deliberately NOT written while nothing has been learned:
    // "absent = never learned" is the §4.4 discriminator, and writing an
    // empty child from the first shipped session would destroy it (Learn
    // lands at P4 and starts writing the child when it has targets).

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
            liveDetachMask.clear();
            if (const auto mask = live.getChildWithName ("DETACH_MASK"); mask.isValid())
                for (int i = 0; i < mask.getNumChildren(); ++i)
                    liveDetachMask.add (mask.getChild (i).getProperty ("id").toString());
        }
    }

    updateLatency();   // int_ latency inputs may have changed with the session
}

// ---------------------------------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AnabasisAudioProcessor();
}
