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
    {
        const auto id = p->getStringAttribute ("id");
        if (id.isEmpty() || isPresetExcludedParam (id))
            continue;                                   // the shared predicate, applied on read too
        if (locked && id == pid::ceiling)
            continue;
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

    return true;
}
