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

bool PresetManager::applyPreset (const juce::File& file, juce::StringArray& detachMaskOut)
{
    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("AnabasisPreset"))
        return false;   // foreign/corrupt input is a no-op, never a crash (schema read rules)

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
    const O kTransparentMaster[] = {
        { pid::loudness, 25.0f }, { pid::character, 0.05f }, { pid::tone, 0.0f },
        { pid::lookahead, 3.0f }, { pid::limStyle, 0.0f },
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

    const PresetManager::FactoryPreset kFactory[] = {
        { "Transparent Master", kTransparentMaster, (int) std::size (kTransparentMaster) },
        { "Loud Pop",           kLoudPop,           (int) std::size (kLoudPop) },
        { "EDM Club",           kEdmClub,           (int) std::size (kEdmClub) },
        { "Vocal Forward",      kVocalForward,      (int) std::size (kVocalForward) },
        { "Tape Glue",          kTapeGlue,          (int) std::size (kTapeGlue) },
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
