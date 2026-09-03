#include "PresetManager.h"
#include "PluginParameters.h"

#include <cmath>   // std::isfinite — the preset read rule in applyOnePresetValue

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
    forEachPresetParameter (apvts, [&root] (const juce::String& id,
                                            juce::RangedAudioParameter& param)
    {
        auto* p = root.createNewChildElement ("PARAM");
        p->setAttribute ("id", id);
        p->setAttribute ("value", presetValueOf (param));
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
    // The session path's read rule, applied to the preset one, and for the same
    // reason: `snapToLegalValue` below is comparison-based, so a NaN passes
    // through it and through `convertTo0to1` into the parameter and out to the
    // host. A number that cannot be USED is treated exactly like the unknown id
    // on the next line — skipped, leaving the parameter where it was.
    // NaN ONLY: `snapToLegalValue` does clamp ±inf to the range ends, and a
    // preset asking for an endpoint gets one. 0.2.9 wrote `! std::isfinite`
    // here, which declined the endpoint too — the comment beside it said
    // "infinities already clamp correctly" while the code stopped them doing so.
    if (std::isnan (value))
        return;
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
        { pid::limStyle, 2.0f },
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

    // The `id` column is the ADR-0022 identity: immutable internal tokens,
    // never displayed, never renamed (saved sessions, A/B slots and undo
    // entries may hold them). Reordering or renaming the PRESETS is safe —
    // nothing resolves by position or by name once an identity exists.
    const PresetManager::FactoryPreset kFactory[] = {
        // Index 0, an EMPTY override table: "defaults + intents" with zero
        // intents IS the default patch. The sibling's pattern (its bank also
        // opens on { "default", "Default", {} }); the fresh-constructed state
        // is named by it, so the plugin never shows a nameless "Preset"
        // placeholder.
        { "default",            "Default",            nullptr,            0 },
        { "transparentMaster",  "Transparent Master", kTransparentMaster, (int) std::size (kTransparentMaster) },
        { "loudPop",            "Loud Pop",           kLoudPop,           (int) std::size (kLoudPop) },
        { "edmClub",            "EDM Club",           kEdmClub,           (int) std::size (kEdmClub) },
        { "vocalForward",       "Vocal Forward",      kVocalForward,      (int) std::size (kVocalForward) },
        { "tapeGlue",           "Tape Glue",          kTapeGlue,          (int) std::size (kTapeGlue) },
        { "rockPunch",          "Rock Punch",         kRockPunch,         (int) std::size (kRockPunch) },
        { "hipHopLowEnd",       "Hip-Hop Low End",    kHipHopLowEnd,      (int) std::size (kHipHopLowEnd) },
        { "acousticWarmth",     "Acoustic Warmth",    kAcousticWarmth,    (int) std::size (kAcousticWarmth) },
        { "classicalDynamics",  "Classical Dynamics", kClassicalDynamics, (int) std::size (kClassicalDynamics) },
        { "podcastVoice",       "Podcast Voice",      kPodcastVoice,      (int) std::size (kPodcastVoice) },
        { "cinematicWide",      "Cinematic Wide",     kCinematicWide,     (int) std::size (kCinematicWide) },
        { "loFiCrush",          "Lo-Fi Crush",        kLoFiCrush,         (int) std::size (kLoFiCrush) },
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
    // THROUGH `forEachPresetParameter`, the same walk `savePreset` and the
    // wrapper's dirty-marker projection use, which is what keeps every preset
    // operation over ONE parameter set. This loop used to iterate
    // `apvts.state`'s PARAM children instead, and the round-52 argument for
    // unifying the other two applies here with more force: "one tree child per
    // parameter" is a fact about JUCE, not an invariant of this code, and a
    // parameter the tree walk missed would keep the value the PREVIOUS preset
    // left it at — the blend-two-presets failure this defaults pass exists to
    // prevent, and audible rather than cosmetic.
    //
    // It also retires a caveat rather than restating it: the old loop wrote the
    // APVTS tree (via `setValueNotifyingHost`) while iterating it, safe only
    // because a property write neither adds nor removes children. The shared
    // walk reads the processor's parameter list and touches no `ValueTree` at
    // all, so the iterator has nothing left to invalidate.
    //
    // The ceiling lock stays HERE, not in the shared walk: a locked ceiling is
    // never WRITTEN by a preset (DESIGN §4.2), but it is still saved and still
    // compared by the dirty marker, so it is an apply-side rule and not a
    // member of the preset parameter set.
    const bool locked = internal.ceilingLocked();
    forEachPresetParameter (apvts, [&] (const juce::String& id,
                                        juce::RangedAudioParameter& param)
    {
        if (locked && id == pid::ceiling)
            return;                                 // §4.2, as `applyOnePresetValue`

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
        // THE DEFAULT IS NOT SNAPPED, and that is a stated invariant rather than
        // an omission. A registered default must already BE a legal value: it is
        // the value the parameter reports before anything writes it, so an
        // off-step default would mean the plugin starts out at a position the
        // preset system, the snapping ranges and ADR-0007's round trip all agree
        // is unreachable. Every default in `PluginParameters.cpp` satisfies that
        // today, which is why routing it through `snapToLegalValue` — as the old
        // two-pass walk did, via `applyOnePresetValue` — was a no-op and its
        // removal changed nothing.
        //
        // Overrides ARE snapped, below, and the asymmetry is the point: an
        // override is TABLE DATA, hand-written per preset and free to be an
        // approximate intent ("ceiling −0.5"), so it is normalised on the way
        // in. A default is registry data that the registry itself must keep
        // legal. If a discrete parameter is ever registered with an off-step
        // default, snapping here would paper over it — the fix belongs in the
        // registration, and `testRegistrySnapshot` (which pins every default against a
        // frozen fixture) is where it would surface.
        float target = param.getDefaultValue();
        for (int i = 0; i < table[index].numOverrides; ++i)
            if (id == table[index].overrides[i].id)
                target = param.getNormalisableRange().convertTo0to1 (
                             param.getNormalisableRange().snapToLegalValue (
                                 table[index].overrides[i].value));
        setParamIfMoved (param, target);
    });

    detachMaskOut.clear();     // factory presets load nothing pre-detached
    return true;
}

// ----------------------------------------------------------------------------
//  Preset identity (ADR-0022). Metadata only: nothing here reads or writes a
//  parameter, and nothing here touches a user preset FILE — the `.anabasis`
//  format is untouched, the trio lives in the session blob's SLOT unit.
// ----------------------------------------------------------------------------
PresetManager::SelectionFields PresetManager::encodeSelection (const Selection& s)
{
    switch (s.kind)
    {
        case Selection::Kind::factory:
            return { "factory", s.factoryId, {} };

        case Selection::Kind::userFile:
        {
            // DIRECT child, not descendant — `juce::File::isAChildOf` recurses,
            // so it is also true for a file nested in a SUB-folder of the
            // preset folder, and that file would then be stored as its bare
            // name and decode back to a DIFFERENT same-named file sitting
            // directly in the folder. Every list this build shows is a
            // non-recursive scan, so a direct child is the only thing that can
            // ever be a menu row anyway; everything else takes the
            // absolute-path branch and round-trips exactly.
            //
            // ...and the bare name has to be one the decoder cannot mistake
            // for a path. Nothing stops a user dropping `~foo.anabasis` into
            // the preset folder by hand, and `juce::File::isAbsolutePath`
            // accepts a leading `~` on POSIX — so a bare `~foo.anabasis` would
            // come back as the literal relative string rather than the file in
            // the folder, and the row would lose its tick. Such a name takes
            // the absolute-path branch instead: less portable for that one
            // preset, but `decode(encode(s)) == s` holds, which is the
            // invariant the whole design rests on (Anamorph worklog §§7, 9).
            const auto name = s.file.getFileName();
            const bool nameIsUnambiguous = ! juce::File::isAbsolutePath (name);
            return { "user", {}, (s.file.getParentDirectory() == userPresetDirectory()
                                      && nameIsUnambiguous)
                                     ? name
                                     : s.file.getFullPathName() };
        }

        case Selection::Kind::unknown:
        default:
            return {};
    }
}

PresetManager::Selection PresetManager::decodeSelection (const juce::String& kind,
                                                         const juce::String& factoryId,
                                                         const juce::String& userFile)
{
    // Anything unrecognised, empty or half-written decodes to `unknown`,
    // which is the pre-ADR-0022 behaviour (resolve by name). A
    // wrong-but-well-formed value cannot select the wrong row either:
    // `selectedPresetRow` answers -1 for an identity it cannot find, rather
    // than falling back to a same-named preset.
    if (kind == "factory" && factoryId.isNotEmpty())
        return { Selection::Kind::factory, factoryId, {} };

    if (kind == "user" && userFile.isNotEmpty())
        return { Selection::Kind::userFile, {},
                 juce::File::isAbsolutePath (userFile)
                     ? juce::File (userFile)
                     : userPresetDirectory().getChildFile (userFile) };

    return {};
}

int PresetManager::selectedPresetRow (const Selection& sel,
                                      const juce::String& currentName,
                                      const FactoryPreset* factory, int factoryCount,
                                      const juce::Array<juce::File>& userFiles)
{
    if (sel.kind == Selection::Kind::factory)
    {
        for (int i = 0; i < factoryCount; ++i)
            if (sel.factoryId == factory[i].id)
                return i;
        return -1;   // a known id that resolves to no row ticks NOTHING —
                     // the name scan below must not answer for it
    }

    if (sel.kind == Selection::Kind::userFile)
    {
        // A PATH-STRING compare, deliberately: `juce::File::operator==` goes
        // through `compareFilenames`, which does NO canonicalisation — no
        // symlink resolution, no `/private/var`↔`/var` folding, no UNC↔mapped
        // drive folding, no relative-path normalisation — but IS
        // case-insensitive on Windows and macOS, and case-sensitive on Linux.
        // So the property is narrower than "any different spelling misses": a
        // differently-CASED spelling matches on the two case-insensitive
        // platforms (the same file, which is the answer wanted anyway), while
        // every other re-spelling misses on all three and shows no tick. That
        // is the safe direction — a miss, never a WRONG row — and it is the
        // documented contract rather than an oversight: `getLinkedTarget()`
        // would change what "the same preset" means and brings its own
        // per-platform failure modes (ADR-0022 §Decision 8).
        for (int i = 0; i < userFiles.size(); ++i)
            if (userFiles.getReference (i) == sel.file)
                return factoryCount + i;
        return -1;   // outside the folder, or deleted/renamed on disk — no row
    }

    // No identity at all (a pre-ADR-0022 session): the name fallback, first
    // matching row with the factory block first — the documented tie-break
    // this build gave before identities existed.
    for (int i = 0; i < factoryCount; ++i)
        if (currentName == factory[i].name)
            return i;
    for (int i = 0; i < userFiles.size(); ++i)
        if (userFiles.getReference (i).getFileNameWithoutExtension() == currentName)
            return factoryCount + i;
    return -1;
}
