#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "InternalState.h"
#include "PluginParameters.h"   // isPresetExcludedParam — the shared exclusion predicate
#include <algorithm>
#include <vector>

// ============================================================================
//  PresetManager — user presets as `.anabasis` XML (ADR-0007 §Presets).
//
//  Preset contract: SNAPPED denormalised values only (no `raw` attribute) —
//  snap-equivalence is the preset contract, raw-exactness is the host-session
//  contract — PLUS the macro detach mask (§5.3). Preset-EXCLUDED parameters
//  (view tier ∪ freeze, the single shared predicate) never load from a preset,
//  and a locked ceiling is captured and re-asserted across apply
//  (int_ceilingLock, DESIGN §4.2 "Ceiling lock").
//
//  P1 scope: the mechanism (save/apply/enumerate). Factory presets are
//  compiled-in override tables whose list + wording is OWNER-SUPPLIED at P6
//  (C8) — none are invented here.
// ============================================================================

class PresetManager
{
public:
    PresetManager (juce::AudioProcessorValueTreeState& apvtsIn, InternalState& internalIn)
        : apvts (apvtsIn), internal (internalIn) {}

    static juce::File userPresetDirectory()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("RollyTech").getChildFile ("Anabasis").getChildFile ("Presets");
    }

    // The exact number a preset stores for one parameter: the SNAPPED
    // denormalised value (ADR-0007 — snap-equivalence is the preset contract,
    // raw-exactness is the host-session one). `savePreset` writes it and the
    // wrapper's dirty-marker projection re-derives it, so it lives here rather
    // than twice: two copies of this rule disagreeing would make the "edited"
    // mark describe a file different from the one save would write.
    static double presetValueOf (const juce::RangedAudioParameter& param)
    {
        const auto& r = param.getNormalisableRange();
        return (double) r.snapToLegalValue (r.convertFrom0to1 (param.getValue()));
    }

    // THE preset parameter set — one traversal, one exclusion test — visited in
    // a fixed order and handed to the caller as (id, parameter).
    //
    // THE PARAMETER, not its preset value, because the three callers need
    // different things from it and only two of them want a value:
    // `savePreset` and `presetShapeFromLive` ask `presetValueOf` (the shared
    // ADR-0007 snap rule, still one copy), while `applyFactoryPreset` needs the
    // parameter's DEFAULT and then writes to it. Handing over the value and
    // hiding the parameter would have left the third caller unable to use this
    // function at all — which is exactly why it walked its own collection until
    // round 57.
    //
    // `savePreset` writes the file from it, the wrapper's dirty-marker
    // projection (`presetShapeFromLive`) rebuilds the same content from it, and
    // `applyFactoryPreset`'s defaults pass writes through it.
    // Those three used to walk DIFFERENT collections for one answer: two over
    // `apvts.state`'s PARAM children, one over `getParameters()`. Every value
    // agreed, because APVTS creates one tree child per parameter — but "agree"
    // was a fact about today's JUCE rather than a property of the code, and a
    // parameter registered without a tree node (or a stray node without a
    // parameter) would have put content in the file that the marker could not
    // see, or the reverse. Sharing the walk makes them one set by construction.
    //
    // The FACTORY APPLY is the case where a divergence would have been audible
    // rather than cosmetic. An override table is "defaults + intents", so the
    // pass has to reach EVERY non-excluded parameter: one it skipped would keep
    // the value the previous preset left, which is the blend-two-presets
    // failure the defaults pass exists to prevent — silent, and worse the
    // further apart the two presets are.
    //
    // The PARAMETERS are the collection, not the tree, and that choice carries
    // a second guarantee: the marker runs on the editor's ~3 Hz poll, and this
    // walk touches no `juce::ValueTree` at all — the parameter list is fixed for
    // the processor's lifetime and each value is an atomic load. That is what
    // keeps the poll off the APVTS tree lock (KI-008) and away from the wrapper
    // state an off-message-thread restore writes (KI-003).
    //
    // Restricted to parameters APVTS owns: `getParameters()` is the processor's
    // list, so a future non-APVTS parameter would otherwise silently start
    // appearing in preset files.
    //
    // VISITED IN ID ORDER, and that is a serialisation decision rather than
    // tidiness. `savePreset` used to inherit the order of `apvts.state`'s
    // children, which JUCE keys by id and therefore hands back alphabetically;
    // `getParameters()` is REGISTRATION order, so moving the writer onto it
    // would have re-ordered every `<PARAM>` element in every file on its next
    // save — a diff across the whole preset bank for no semantic gain, since
    // `applyPreset` looks each id up and has never depended on position. Worse,
    // registration order is not stable: it changes whenever the parameter
    // layout is re-arranged, so the file would churn again on any future
    // reshuffle. Sorting by id keeps the bytes exactly as they were AND makes
    // the order independent of the layout. `String::compare` is a plain UTF-8
    // lexicographic compare, which is the collation the tree order already had
    // for this ASCII id set.
    template <typename Fn>
    static void forEachPresetParameter (const juce::AudioProcessorValueTreeState& apvts, Fn&& fn)
    {
        std::vector<juce::RangedAudioParameter*> ordered;
        ordered.reserve ((size_t) apvts.processor.getParameters().size());

        for (auto* p : apvts.processor.getParameters())
        {
            auto* param = dynamic_cast<juce::RangedAudioParameter*> (p);
            if (param == nullptr)
                continue;
            const auto id = param->getParameterID();
            if (isPresetExcludedParam (id) || apvts.getParameter (id) == nullptr)
                continue;
            ordered.push_back (param);
        }

        std::sort (ordered.begin(), ordered.end(),
                   [] (const juce::RangedAudioParameter* a, const juce::RangedAudioParameter* b)
                   { return a->getParameterID().compare (b->getParameterID()) < 0; });

        for (auto* param : ordered)
            fn (param->getParameterID(), *param);
    }

    bool savePreset (const juce::File& file, const juce::StringArray& detachMask) const;
    bool applyPreset (const juce::File& file, juce::StringArray& detachMaskOut);
    // The same apply, against a document the CALLER already parsed. Added so
    // the wrapper's readability gate and the apply operate on ONE document:
    // `applyPresetFile` used to parse for the gate, discard the result, and let
    // this class parse the file again, so a file rewritten between the two
    // passed the gate and then applied different content — and the preset ring
    // walks this path on every ‹/› press. The `File` overload above is kept for
    // callers that have no document yet; it parses and delegates here.
    bool applyPreset (const juce::XmlElement& parsed, juce::StringArray& detachMaskOut);

    // The single answer to "is this a preset file this build can apply?".
    // `applyPreset` reads through it, and so does the wrapper's pre-parse undo
    // gate — two independent readability tests could disagree, and did: the
    // gate accepted any well-formed XML, so a foreign root passed it, opened an
    // undo bracket, and then applied nothing. Returns the parsed document (the
    // caller re-parses; a preset apply is a user action, not a hot path).
    static std::unique_ptr<juce::XmlElement> parsePresetFile (const juce::File& file);

    // -- factory presets (DESIGN §7: compiled-in override tables) ------------
    // The five names come VERBATIM from the brief (§9 names them — they are
    // owner wording, not invented); the VALUES are ⊕ drafts with the same
    // status as the §5.5 macro curves: tuned by ear at the P6 listening pass,
    // frozen before v0.1.0. The ≥12-preset bank and any further wording stay
    // owner-supplied (C8, OQ). A factory preset expresses itself through the
    // MACROS plus non-managed parameters wherever possible, so nothing loads
    // pre-detached; the mask it carries is empty.
    struct FactoryPreset
    {
        // `id` is the preset's INTERNAL identity (ADR-0022) and is never
        // shown: the menu, the top bar and the Save Preset field all display
        // `name`. It exists so a factory preset is selected by something a
        // user preset can never collide with — a user preset is identified by
        // its FILE, so the two namespaces are disjoint and a user preset
        // saved as e.g. "EDM Club" no longer steals the factory row's tick.
        // Treat the ids as immutable: renaming a preset is a display change,
        // renaming an id would silently re-point A/B slots, undo entries and
        // saved sessions that still hold the old one.
        const char* id;
        const char* name;
        struct Override { const char* id; float value; };
        const Override* overrides;
        int numOverrides;
    };
    static const FactoryPreset* factoryPresets (int& countOut);
    bool applyFactoryPreset (int index, juce::StringArray& detachMaskOut);

    // -- preset identity (ADR-0022) ------------------------------------------
    // What PRODUCED the current sound, as opposed to what it is CALLED. The
    // list used to be resolved by NAME everywhere (the menu ticked every row
    // whose name matched — BOTH rows on a clash — and ‹ › stepped from
    // whichever row the scan hit first). Identity no longer travels as a
    // name: a factory preset is its immutable `FactoryPreset::id`, a user
    // preset is its file on disk, and the two namespaces cannot collide, so a
    // duplicate name is merely a duplicate label. The wrapper holds the live
    // value (`liveSelection`) beside `livePresetName` and carries it on the
    // SLOT tree, so undo, A/B, Copy and the session all inherit it from the
    // slot plumbing that already carries the name.
    struct Selection
    {
        enum class Kind { unknown, factory, userFile };
        Kind         kind = Kind::unknown;   // unknown = "no identity" → name fallback
        juce::String factoryId;              // kind == factory
        juce::File   file;                   // kind == userFile
    };

    // The wire form of a Selection: three plain strings, so the encoding
    // lives with the type rather than being spelled out at each serialization
    // site (the SLOT properties `presetSource` / `presetFactoryId` /
    // `presetUserFile` — SERIALIZATION_REGISTRY §1.2).
    //
    // A user preset sitting DIRECTLY in the preset folder is stored as its
    // FILE NAME, not its full path: there the name is already a complete
    // identity (every list this build shows is a non-recursive scan of that
    // folder), it keeps the user's home directory out of the saved project,
    // and a project moved to another machine still resolves. Anything else
    // stores its absolute path, which keeps `decode(encode(s)) == s` true:
    // a file loaded from outside the folder, one nested in a SUB-folder of it
    // (the test is `getParentDirectory() == userPresetDirectory()` — the
    // recursive `juce::File::isAChildOf` would store a nested file by bare
    // name and decode it to a DIFFERENT same-named file sitting directly in
    // the folder), and one whose name `juce::File::isAbsolutePath` would
    // accept — a leading `~` on POSIX — which would decode to a literal
    // relative path instead. Both conditions are ports of defects the sibling
    // product's reviews found (Anamorph ADR-0024; its worklog §§7, 9).
    struct SelectionFields { juce::String kind, factoryId, userFile; };
    static SelectionFields encodeSelection (const Selection&);
    static Selection       decodeSelection (const juce::String& kind,
                                            const juce::String& factoryId,
                                            const juce::String& userFile);

    // Which row of the flat preset list (the FACTORY block first, then
    // `userFiles` in the order given) is the CURRENT one — the single answer
    // the menu tick and ‹ › stepping share, so exactly one row is ever
    // marked. Identity first; when the identity is KNOWN but absent from the
    // list (a file loaded from outside the preset folder, a user preset
    // deleted or renamed on disk, a factory id a later version removed) the
    // answer is -1 — NO row. Falling through to the name scan there is
    // precisely the mis-tick ADR-0022 removes. The name fallback below it
    // covers only state that carries no identity at all (a pre-ADR-0022
    // session), where it keeps the old answer: first matching row, factory
    // block first — a documented tie-break, not an accident.
    static int selectedPresetRow (const Selection& sel,
                                  const juce::String& currentName,
                                  const FactoryPreset* factory, int factoryCount,
                                  const juce::Array<juce::File>& userFiles);

private:
    juce::AudioProcessorValueTreeState& apvts;
    InternalState& internal;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
