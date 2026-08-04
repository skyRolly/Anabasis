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

    // THE preset parameter set — one traversal, one exclusion test, one value
    // rule — visited in a fixed order and handed to the caller as (id, value).
    //
    // `savePreset` writes the file from it and the wrapper's dirty-marker
    // projection (`presetShapeFromLive`) rebuilds the same content from it.
    // Those two used to walk DIFFERENT collections for the same answer: the
    // writer iterated `apvts.state`'s PARAM children, the projection iterated
    // `getParameters()`. Every value agreed, because APVTS creates one tree
    // child per parameter — but "agree" was a fact about today's JUCE rather
    // than a property of the code, and a parameter registered without a tree
    // node (or a stray node without a parameter) would have put content in the
    // file that the marker could not see, or the reverse. Sharing the walk
    // makes the two the same set by construction.
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
            fn (param->getParameterID(), presetValueOf (*param));
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
        const char* name;
        struct Override { const char* id; float value; };
        const Override* overrides;
        int numOverrides;
    };
    static const FactoryPreset* factoryPresets (int& countOut);
    bool applyFactoryPreset (int index, juce::StringArray& detachMaskOut);

private:
    juce::AudioProcessorValueTreeState& apvts;
    InternalState& internal;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
