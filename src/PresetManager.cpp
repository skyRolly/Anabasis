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
        auto* p = root.createNewChildElement ("PARAM");
        p->setAttribute ("id", id);
        // Snapped denormalised value only — the preset contract (ADR-0007).
        p->setAttribute ("value", (double) (float) node.getProperty ("value"));
    }

    auto* mask = root.createNewChildElement ("DETACH_MASK");
    for (const auto& id : detachMask)
        mask->createNewChildElement ("PARAM")->setAttribute ("id", id);

    file.getParentDirectory().createDirectory();
    return root.writeTo (file);
}

bool PresetManager::applyPreset (const juce::File& file, juce::StringArray& detachMaskOut)
{
    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("AnabasisPreset"))
        return false;   // foreign/corrupt input is a no-op, never a crash (schema read rules)

    // Ceiling lock: capture before, re-assert after — browsing presets never
    // moves a locked ceiling (DESIGN §4.2; the mechanism is generic, the
    // lockable set is {ceiling} in v1).
    const bool locked = internal.ceilingLocked();
    float lockedCeiling = 0.0f;
    if (auto* ceilingParam = apvts.getParameter (pid::ceiling); locked && ceilingParam != nullptr)
        lockedCeiling = ceilingParam->getValue();

    for (auto* p : xml->getChildWithTagNameIterator ("PARAM"))
    {
        const auto id = p->getStringAttribute ("id");
        if (id.isEmpty() || isPresetExcludedParam (id))
            continue;                                   // the shared predicate, applied on read too
        if (auto* param = apvts.getParameter (id))      // unknown ids ignored
        {
            const auto value = (float) p->getDoubleAttribute ("value");
            param->setValueNotifyingHost (
                param->getNormalisableRange().convertTo0to1 (
                    param->getNormalisableRange().snapToLegalValue (value)));
        }
    }

    detachMaskOut.clear();
    if (auto* mask = xml->getChildByName ("DETACH_MASK"))
        for (auto* p : mask->getChildWithTagNameIterator ("PARAM"))
            detachMaskOut.add (p->getStringAttribute ("id"));

    if (auto* ceilingParam = apvts.getParameter (pid::ceiling); locked && ceilingParam != nullptr)
        ceilingParam->setValueNotifyingHost (lockedCeiling);

    return true;
}
