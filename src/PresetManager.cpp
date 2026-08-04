#include "PresetManager.h"
#include "PluginParameters.h"

bool PresetManager::savePreset (const juce::File& file, const juce::StringArray& detachMask) const
{
    juce::XmlElement root ("AnabasisPreset");
    root.setAttribute ("schemaVersion", 1);

    // SNAPPED denormalised values only — the preset contract (ADR-0007). The
    // tree's `value` is convertFrom0to1(getValue()) UNSNAPPED for the Raw*
    // discrete classes (they deliberately hold mid-step raw values), so the
    // shared walk snaps through the range (`presetValueOf`) rather than copying
    // the property. It also owns the exclusion test and the traversal, so this
    // writer and the wrapper's dirty-marker projection cannot come to describe
    // different content — see `forEachPresetParameter`.
    forEachPresetParameter (apvts, [&root] (const juce::String& id, double value)
    {
        auto* p = root.createNewChildElement ("PARAM");
        p->setAttribute ("id", id);
        p->setAttribute ("value", value);
    });

    auto* mask = root.createNewChildElement ("DETACH_MASK");
    for (const auto& id : detachMask)
        mask->createNewChildElement ("PARAM")->setAttribute ("id", id);

    file.getParentDirectory().createDirectory();
    return root.writeTo (file);
}

// The shared apply core: both file and factory presets go through the same
// lock/exclusion/snap semantics, so the two cannot drift.
// One write, one rule: notify the host only when the value actually moves.
//
// `setValueNotifyingHost` is REQUIRED for a preset apply — it is what keeps the
// host's cached value, the APVTS `value` property and the editor attachments in
// agreement, and every value-landing path in this build uses it. What it is not
// required for is a parameter that is already where the preset wants it: that
// notification carries no information, and a host is free to record it as an
// automation point or to mark the project dirty. A factory apply writes the
// whole default patch before laying the overrides on top, so browsing presets
// with ‹/› emitted up to ~46 notifications per step, most of them for values
// that did not change — and the two-pass shape meant every overridden parameter
// was notified twice, once to its default and once to the preset's value.
//
// EXACT comparison on the NORMALISED value, after the snap, because that is the
// number the host is told: if the bits are identical the write is a no-op by
// construction, so skipping it cannot lose an update. Automation correctness is
// unchanged (a host that never hears about a value that did not move still
// holds the right value) and preset semantics are unchanged (the parameter ends
// at the same place either way).
static void setParamIfMoved (juce::RangedAudioParameter& param, float normalised)
{
    if (! juce::exactlyEqual (param.getValue(), normalised))
        param.setValueNotifyingHost (normalised);
}

static void applyOnePresetValue (juce::AudioProcessorValueTreeState& apvts,
                                 bool ceilingLocked,
                                 const juce::String& id, float value)
{
    if (id.isEmpty() || isPresetExcludedParam (id))
        return;                                     // the shared predicate, applied on read too
    if (ceilingLocked && id == pid::ceiling)
        return;                                     // §4.2: a locked ceiling is never written
    if (auto* param = apvts.getParameter (id))      // unknown ids ignored
        setParamIfMoved (*param,
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
    return applyPreset (*xml, detachMaskOut);
}

bool PresetManager::applyPreset (const juce::XmlElement& xmlDoc, juce::StringArray& detachMaskOut)
{
    // Re-checked HERE and not only at the `File` overload: this is public, and
    // "the caller already validated it" is exactly the kind of precondition a
    // second caller does not know about. `parsePresetFile` is the one
    // readability answer (root tag included), so asking it again costs a tag
    // comparison and keeps the two entry points impossible to disagree.
    if (! xmlDoc.hasTagName ("AnabasisPreset"))
        return false;
    const auto* xml = &xmlDoc;

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
    // WHY `setValueNotifyingHost` AND NOT A SILENT WRITE, investigated at round
    // 46 and deliberately left as it is. Notifying is not a stylistic choice
    // here — it is the only write that keeps the host, the APVTS tree and the
    // editor attachments agreeing: the host caches parameter values and reads
    // them back for automation and its own displays, and a non-notifying write
    // updates neither that cache nor the `value` property the attachments and
    // the serialiser read. Every other value-landing path in this build uses the
    // same call for the same reason — `MacroEngine::applyMapping`,
    // `reassertFromRaw` (ADR-0007's raw-exact session restore) and
    // `applyOnePresetValue` below — so making this one path silent would leave
    // exactly one restore whose values the host never learns about.
    //
    // THE COST, and how much of it survived. This pass writes every
    // non-excluded parameter, so a factory apply used to emit up to ~46 host
    // notifications per ‹/› step — most of them for values already at the
    // default, plus a second one for every parameter the overrides then moved.
    // Round 50 removed the ones that carry no information: `setParamIfMoved`
    // notifies only when the normalised value actually changes (see there for
    // why that cannot lose an update). What remains is one notification per
    // parameter the preset genuinely moves, which is the contract — a host
    // learns about a change because there was one. Whether even that burst
    // reads as automation in a given DAW stays a matrix check
    // (`RELEASE_COMPATIBILITY_CHECKLIST.md`).
    // Each notification re-enters `AnabasisAudioProcessor::parameterChanged`,
    // where the nine managed ids are discarded because `isRestoring()` is true.
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
        auto* param = apvts.getParameter (id);
        if (param == nullptr)
            continue;

        // ONE write per parameter, at its FINAL value. This used to be two
        // passes — every non-excluded parameter to its default, then the
        // table's intents over the top — which meant a parameter the preset
        // overrides was written twice and announced to the host twice, the
        // first time to a value the preset never wanted. It also put the whole
        // surface through a default state that no preset describes, for the
        // window between the passes.
        //
        // The override lookup is a linear scan of a table whose length is the
        // preset's own intent count (a handful), inside a loop over 49
        // parameters, on the message thread — cheaper than the second pass of
        // `setValueNotifyingHost` calls it replaces, and it needs no container.
        // The exclusion and ceiling-lock rules are applied ONCE here rather
        // than once per pass, which is also why they cannot now disagree
        // between the two.
        float target = param->getDefaultValue();
        for (int i = 0; i < table[index].numOverrides; ++i)
            if (id == table[index].overrides[i].id)
                target = param->getNormalisableRange().convertTo0to1 (
                             param->getNormalisableRange().snapToLegalValue (
                                 table[index].overrides[i].value));
        setParamIfMoved (*param, target);
    }

    detachMaskOut.clear();     // factory presets load nothing pre-detached
    return true;
}
