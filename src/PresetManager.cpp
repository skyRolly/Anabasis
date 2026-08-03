#include "PresetManager.h"
#include "PluginParameters.h"

bool PresetManager::savePreset (const juce::File& file, const juce::StringArray& detachMask) const
{
    juce::XmlElement root ("AnabasisPreset");
    root.setAttribute ("schemaVersion", 1);

    for (const auto node : apvts.state)
    {
        if (! node.hasType ("PARAM"))
            continue;
        const auto id = node.getProperty ("id").toString();
        if (isPresetExcludedParam (id))
            continue;
        auto* param = apvts.getParameter (id);
        if (param == nullptr)
            continue;
        // SNAPPED denormalised value only — the preset contract (ADR-0007).
        // The tree's `value` is convertFrom0to1(getValue()) UNSNAPPED for the
        // Raw* discrete classes (they deliberately hold mid-step raw values),
        // so snap through the range here rather than copying the property.
        const auto& r = param->getNormalisableRange();
        auto* p = root.createNewChildElement ("PARAM");
        p->setAttribute ("id", id);
        p->setAttribute ("value", (double) r.snapToLegalValue (r.convertFrom0to1 (param->getValue())));
    }

    auto* mask = root.createNewChildElement ("DETACH_MASK");
    for (const auto& id : detachMask)
        mask->createNewChildElement ("PARAM")->setAttribute ("id", id);

    file.getParentDirectory().createDirectory();
    return root.writeTo (file);
}

// The shared apply core: both file and factory presets go through the same
// lock/exclusion/snap semantics, so the two cannot drift.
static void applyOnePresetValue (juce::AudioProcessorValueTreeState& apvts,
                                 bool ceilingLocked,
                                 const juce::String& id, float value)
{
    if (id.isEmpty() || isPresetExcludedParam (id))
        return;                                     // the shared predicate, applied on read too
    if (ceilingLocked && id == pid::ceiling)
        return;                                     // §4.2: a locked ceiling is never written
    if (auto* param = apvts.getParameter (id))      // unknown ids ignored
        param->setValueNotifyingHost (
            param->getNormalisableRange().convertTo0to1 (
                param->getNormalisableRange().snapToLegalValue (value)));
}

std::unique_ptr<juce::XmlElement> PresetManager::parsePresetFile (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("AnabasisPreset"))
        return nullptr;   // foreign/corrupt input is a no-op, never a crash (schema read rules)
    return xml;
}

bool PresetManager::applyPreset (const juce::File& file, juce::StringArray& detachMaskOut)
{
    const auto xml = parsePresetFile (file);
    if (xml == nullptr)
        return false;

    // Ceiling lock is a SKIP, not a write-then-revert: a revert leaves a
    // window in which an audio block snapshots the preset's ceiling and the
    // smoother starts gliding toward it — "browsing presets never moves a
    // locked ceiling" (DESIGN §4.2) means the locked parameter is never
    // written at all. The mechanism is generic; the lockable set is {ceiling}
    // in v1.
    const bool locked = internal.ceilingLocked();

    for (auto* p : xml->getChildWithTagNameIterator ("PARAM"))
        applyOnePresetValue (apvts, locked, p->getStringAttribute ("id"),
                             (float) p->getDoubleAttribute ("value"));

    detachMaskOut.clear();
    if (auto* mask = xml->getChildByName ("DETACH_MASK"))
        for (auto* p : mask->getChildWithTagNameIterator ("PARAM"))
            detachMaskOut.add (p->getStringAttribute ("id"));

    return true;
}

// ============================================================================
//  Factory presets — DESIGN §7's compiled-in override tables. Values are ⊕
//  drafts (see the header). Everything not listed stays at its declared
//  default, which is what "override table" means: a factory preset is the
//  default patch plus a handful of intents, mostly macro positions.
// ============================================================================
namespace
{
    using O = PresetManager::FactoryPreset::Override;

    // brief §9 name 1: clean level lift, everything else stays honest.
    // `colourModel` is NAMED here even though a table is "defaults + intents":
    // the registered default is 1 = Tape (`PluginParameters.cpp`), so leaving it
    // out made the two presets whose whole intent is "untouched" land on the
    // Tape model. Inaudible today only because the managed `colourDepth` the
    // macro mapping writes from Character is ~0 at these settings — i.e. the
    // intent was resting on a §5.5 curve constant that is ⊕ for the listening
    // pass. Naming Clean (index 0) decouples them. ⊕ 2026-08-03 with the rest
    // of the table's values.
    const O kTransparentMaster[] = {
        { pid::loudness, 25.0f }, { pid::character, 0.05f }, { pid::tone, 0.0f },
        { pid::lookahead, 3.0f }, { pid::limStyle, 0.0f }, { pid::colourModel, 0.0f },
    };
    // brief §9 name 2: modern pop level with a little forwardness.
    const O kLoudPop[] = {
        { pid::loudness, 60.0f }, { pid::character, 0.25f }, { pid::tone, 0.15f },
        { pid::limStyle, 1.0f },
    };
    // brief §9 name 3: club level; the Loud style carries the density.
    const O kEdmClub[] = {
        { pid::loudness, 80.0f }, { pid::character, 0.45f }, { pid::tone, 0.25f },
        { pid::limStyle, 2.0f }, { pid::ceiling, -0.5f },
    };
    // brief §9 name 4: presence via the (non-managed) bells, macro modest.
    const O kVocalForward[] = {
        { pid::loudness, 45.0f }, { pid::character, 0.15f }, { pid::tone, 0.3f },
        { pid::eqBell2Freq, 3000.0f }, { pid::eqBell2Gain, 1.5f }, { pid::eqBell2Q, 1.2f },
    };
    // brief §9 name 5: colour-led glue; Tape model, gentle dark tilt.
    const O kTapeGlue[] = {
        { pid::loudness, 40.0f }, { pid::character, 0.6f }, { pid::tone, -0.2f },
        { pid::colourModel, 1.0f }, { pid::colourBalance, -0.2f },
    };
    // ⊕ names 6-12 (2026-08-02, owner-approved under the v0.1.0 blanket
    // approval — C8 wording owed the fine review, same status as the values):
    // genre/purpose per brief §9, each the default patch plus a few intents.
    // Punchy style + high transient preserve: drums stay drums at level.
    const O kRockPunch[] = {
        { pid::loudness, 55.0f }, { pid::character, 0.35f }, { pid::tone, 0.1f },
        { pid::limStyle, 1.0f }, { pid::transientPreserve, 75.0f },
    };
    // Low end carried, detector kept off it so the kick does not pump the bus.
    const O kHipHopLowEnd[] = {
        { pid::loudness, 65.0f }, { pid::character, 0.3f }, { pid::tone, -0.15f },
        { pid::eqLowShelfGain, 1.5f }, { pid::scHpfFreq, 60.0f }, { pid::limStyle, 1.0f },
    };
    // Tube warmth at moderate level; the macro stays low so dynamics survive.
    const O kAcousticWarmth[] = {
        { pid::loudness, 30.0f }, { pid::character, 0.4f }, { pid::tone, -0.1f },
        { pid::colourModel, 2.0f }, { pid::colourBalance, -0.1f },
    };
    // Barely-touched: long lookahead, Transparent style, level lift only.
    const O kClassicalDynamics[] = {
        { pid::loudness, 15.0f }, { pid::character, 0.0f }, { pid::tone, 0.0f },
        { pid::lookahead, 6.0f }, { pid::limStyle, 0.0f }, { pid::colourModel, 0.0f },
    };
    // Spoken word: HPF under the voice, presence tilt, steady level.
    const O kPodcastVoice[] = {
        { pid::loudness, 50.0f }, { pid::character, 0.1f }, { pid::tone, 0.2f },
        { pid::scHpfFreq, 80.0f }, { pid::eqLowShelfGain, -1.0f },
    };
    // Wide programme kept wide: link relaxed, gentle level, soft top.
    const O kCinematicWide[] = {
        { pid::loudness, 35.0f }, { pid::character, 0.2f }, { pid::tone, -0.05f },
        { pid::stereoLink, 60.0f }, { pid::lookahead, 5.0f },
    };
    // Deliberate colour-forward crush; Transistor model carries the edge.
    // `colourDepth` is deliberately NOT listed even though the intent wants it
    // high: it is one of the nine §5.5 MANAGED parameters, so the macro mapping
    // that runs after a factory apply owns it, and naming it here would only
    // write a value the mapping then overwrites from Character (0.8 already
    // drives depth). The same rule applies to any table: express the intent
    // through the macros and the UNMANAGED parameters.
    const O kLoFiCrush[] = {
        { pid::loudness, 70.0f }, { pid::character, 0.8f }, { pid::tone, -0.3f },
        { pid::colourModel, 3.0f }, { pid::limStyle, 2.0f },
    };

    const PresetManager::FactoryPreset kFactory[] = {
        { "Transparent Master", kTransparentMaster, (int) std::size (kTransparentMaster) },
        { "Loud Pop",           kLoudPop,           (int) std::size (kLoudPop) },
        { "EDM Club",           kEdmClub,           (int) std::size (kEdmClub) },
        { "Vocal Forward",      kVocalForward,      (int) std::size (kVocalForward) },
        { "Tape Glue",          kTapeGlue,          (int) std::size (kTapeGlue) },
        { "Rock Punch",         kRockPunch,         (int) std::size (kRockPunch) },
        { "Hip-Hop Low End",    kHipHopLowEnd,      (int) std::size (kHipHopLowEnd) },
        { "Acoustic Warmth",    kAcousticWarmth,    (int) std::size (kAcousticWarmth) },
        { "Classical Dynamics", kClassicalDynamics, (int) std::size (kClassicalDynamics) },
        { "Podcast Voice",      kPodcastVoice,      (int) std::size (kPodcastVoice) },
        { "Cinematic Wide",     kCinematicWide,     (int) std::size (kCinematicWide) },
        { "Lo-Fi Crush",        kLoFiCrush,         (int) std::size (kLoFiCrush) },
    };
} // namespace

const PresetManager::FactoryPreset* PresetManager::factoryPresets (int& countOut)
{
    countOut = (int) std::size (kFactory);
    return kFactory;
}

bool PresetManager::applyFactoryPreset (int index, juce::StringArray& detachMaskOut)
{
    int count = 0;
    const auto* table = factoryPresets (count);
    if (index < 0 || index >= count)
        return false;

    // The default patch first — an override TABLE is defaults + intents, and
    // skipping the reset would blend two presets. Locked/excluded parameters
    // go through the same shared core as every file preset.
    //
    // This ITERATES the APVTS tree while WRITING it: `setValueNotifyingHost`
    // has APVTS write the `value` property of the very node being visited.
    // Safe with juce::ValueTree's iterator because a property write neither
    // adds nor removes children, so the child array is never reallocated — and
    // stated because that is the whole reason it is safe. A listener that ever
    // added or removed a PARAM node from here would invalidate the iteration,
    // so collect the ids first if that day comes.
    const bool locked = internal.ceilingLocked();
    for (const auto node : apvts.state)
    {
        if (! node.hasType ("PARAM"))
            continue;
        const auto id = node.getProperty ("id").toString();
        if (isPresetExcludedParam (id) || (locked && id == pid::ceiling))
            continue;
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->getDefaultValue());
    }
    for (int i = 0; i < table[index].numOverrides; ++i)
        applyOnePresetValue (apvts, locked, table[index].overrides[i].id,
                             table[index].overrides[i].value);

    detachMaskOut.clear();     // factory presets load nothing pre-detached
    return true;
}
