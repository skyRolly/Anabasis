#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "InternalState.h"

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

    bool savePreset (const juce::File& file, const juce::StringArray& detachMask) const;
    bool applyPreset (const juce::File& file, juce::StringArray& detachMaskOut);

private:
    juce::AudioProcessorValueTreeState& apvts;
    InternalState& internal;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
