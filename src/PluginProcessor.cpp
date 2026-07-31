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

    storedSlot = saveSlotFromLive();    // slot B starts as a copy of defaults
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
juce::ValueTree AnabasisAudioProcessor::saveSlotFromLive()
{
    juce::ValueTree slot ("SLOT");
    slot.setProperty ("presetName", livePresetName, nullptr);
    slot.appendChild (apvts.copyState(), nullptr);
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
        // predicate): keep the live values across the swap.
        auto incoming = params.createCopy();
        for (int i = 0; i < incoming.getNumChildren(); ++i)
        {
            auto node = incoming.getChild (i);
            if (node.hasType ("PARAM") && isViewTierParam (node.getProperty ("id").toString()))
                if (auto live = apvts.state.getChildWithProperty ("id", node.getProperty ("id")); live.isValid())
                {
                    node.setProperty ("value", live.getProperty ("value"), nullptr);
                    if (live.hasProperty ("raw")) node.setProperty ("raw", live.getProperty ("raw"), nullptr);
                    else                          node.removeProperty ("raw", nullptr);
                }
        }
        apvts.replaceState (incoming);
        reassertFromRaw (incoming);
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
    // P1 form: plain swap on the message thread. TODO(P2): route through the
    // §2.8 forced duck (requestDuck() BEFORE the swap) once the transition
    // layer exists — a bulk swap without it is a click-free-invariant hole
    // that pluginval will not catch but invariant 8's per-path test will.
    auto newlyStored = saveSlotFromLive();
    applySlotToLive (storedSlot);
    storedSlot = std::move (newlyStored);
    activeSlot = newIndex;
}

void AnabasisAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("AnabasisRoot");
    root.setProperty ("schemaVersion", kSchemaVersion, nullptr);

    // ANABASIS with the additive exact-`raw` attribute per PARAM.
    auto params = apvts.copyState();
    for (int i = 0; i < params.getNumChildren(); ++i)
    {
        auto node = params.getChild (i);
        if (node.hasType ("PARAM"))
            if (auto* p = apvts.getParameter (node.getProperty ("id").toString()))
                node.setProperty ("raw", (double) p->getValue(), nullptr);
    }
    root.appendChild (params, nullptr);
    root.appendChild (internalState.state().createCopy(), nullptr);

    juce::ValueTree ab ("AB");
    ab.setProperty ("activeIndex", activeSlot, nullptr);
    ab.appendChild (activeSlot == 0 ? saveSlotFromLive() : storedSlot.createCopy(), nullptr);
    ab.appendChild (activeSlot == 0 ? storedSlot.createCopy() : saveSlotFromLive(), nullptr);
    root.appendChild (ab, nullptr);

    // ADAPTIVE — global learned reference targets only (§4.4); none exist
    // until Learn lands (P4), and "absent = never learned" is the contract.
    root.appendChild (juce::ValueTree ("ADAPTIVE"), nullptr);

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

    if (const auto params = root.getChildWithName ("ANABASIS"); params.isValid())
    {
        // Strip the additive `raw` annotations BEFORE replaceState: they are a
        // serialisation-time overlay, and letting them into the live tree
        // would make save→load→save non-byte-identical (the original never
        // carries them). reassertFromRaw still reads them from `params`.
        auto stripped = params.createCopy();
        for (int i = 0; i < stripped.getNumChildren(); ++i)
            stripped.getChild (i).removeProperty ("raw", nullptr);
        apvts.replaceState (stripped);
        reassertFromRaw (params);
    }
    internalState.replaceFrom (root.getChildWithName ("ANABASIS_INTERNAL"));

    if (const auto ab = root.getChildWithName ("AB"); ab.isValid())
    {
        activeSlot = anabasis::clampAbSlotIndex ((int) ab.getProperty ("activeIndex", 0));
        const auto live   = ab.getChild (activeSlot);
        const auto stored = ab.getChild (1 - activeSlot);
        if (stored.isValid() && stored.hasType ("SLOT"))
            storedSlot = stored.createCopy();
        if (live.isValid() && live.hasType ("SLOT"))
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
