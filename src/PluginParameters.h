#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/EngineParameters.h"

// ============================================================================
//  PluginParameters — the 49-parameter surface of DESIGN §4.2 (ADR-0010).
//
//  IDs are a PERMANENT CONTRACT from v0.1.0 (PARAMETER_COMPATIBILITY_POLICY
//  rules 1/3): ID, range, default and choice ordering freeze; display names
//  stay revisable (rule 2). The registry snapshot test fails on any drift.
//
//  Renaming or removing any pid:: constant is an AI-agent HARD STOP.
// ============================================================================

namespace pid
{
    // view / macro / shared / adaptive / monitor
    inline constexpr const char* bypass            = "bypass";
    inline constexpr const char* advancedMode      = "advancedMode";
    inline constexpr const char* loudness          = "loudness";
    inline constexpr const char* character         = "character";
    inline constexpr const char* tone              = "tone";
    inline constexpr const char* ceiling           = "ceiling";
    inline constexpr const char* freeze            = "freeze";
    inline constexpr const char* loudnessComp      = "loudnessComp";
    inline constexpr const char* deltaMonitor      = "deltaMonitor";
    // input / detectors
    inline constexpr const char* inputGain         = "inputGain";
    inline constexpr const char* scHpfFreq         = "scHpfFreq";
    // comp
    inline constexpr const char* compRatio         = "compRatio";
    inline constexpr const char* compThreshold     = "compThreshold";
    inline constexpr const char* compAttack        = "compAttack";
    inline constexpr const char* compRelease       = "compRelease";
    inline constexpr const char* compAutoRelease   = "compAutoRelease";
    inline constexpr const char* compKnee          = "compKnee";
    inline constexpr const char* compDetector      = "compDetector";
    inline constexpr const char* compMix           = "compMix";
    // clip / colour
    inline constexpr const char* clipShape         = "clipShape";
    inline constexpr const char* clipDrive         = "clipDrive";
    inline constexpr const char* clipMix           = "clipMix";
    inline constexpr const char* colourModel       = "colourModel";
    inline constexpr const char* colourBalance     = "colourBalance";
    inline constexpr const char* colourTone        = "colourTone";
    inline constexpr const char* dynTilt           = "dynTilt";
    inline constexpr const char* colourDepth       = "colourDepth";
    // limiter
    inline constexpr const char* limGain           = "limGain";
    inline constexpr const char* lookahead         = "lookahead";
    inline constexpr const char* limRelease        = "limRelease";
    inline constexpr const char* limAutoRelease    = "limAutoRelease";
    inline constexpr const char* limStyle          = "limStyle";
    inline constexpr const char* stereoLink        = "stereoLink";
    inline constexpr const char* transientPreserve = "transientPreserve";
    inline constexpr const char* truePeakMode      = "truePeakMode";
    // eq
    inline constexpr const char* eqTilt            = "eqTilt";
    inline constexpr const char* eqLowShelfFreq    = "eqLowShelfFreq";
    inline constexpr const char* eqLowShelfGain    = "eqLowShelfGain";
    inline constexpr const char* eqHighShelfFreq   = "eqHighShelfFreq";
    inline constexpr const char* eqHighShelfGain   = "eqHighShelfGain";
    inline constexpr const char* eqBell1Freq       = "eqBell1Freq";
    inline constexpr const char* eqBell1Gain       = "eqBell1Gain";
    inline constexpr const char* eqBell1Q          = "eqBell1Q";
    inline constexpr const char* eqBell2Freq       = "eqBell2Freq";
    inline constexpr const char* eqBell2Gain       = "eqBell2Gain";
    inline constexpr const char* eqBell2Q          = "eqBell2Q";
    inline constexpr const char* eqPosition        = "eqPosition";
    // output
    inline constexpr const char* dither            = "dither";
    inline constexpr const char* ditherShaping     = "ditherShaping";
}

juce::AudioProcessorValueTreeState::ParameterLayout createAnabasisLayout();

// Exclusion tiers (DESIGN §4.2, ADR-0010) — ONE shared predicate, consulted by
// A/B, undo and preset apply alike so the sets cannot drift apart.
//   view tier          {bypass, loudnessComp, deltaMonitor, advancedMode}
//                        -> excluded from A/B, undo AND presets
//   preset-excluded    view ∪ {freeze}   (freeze travels in A/B and undo)
bool isViewTierParam (const juce::String& paramID);
bool isPresetExcludedParam (const juce::String& paramID);

// Cached raw-value pointers: resolved once after APVTS construction, then read
// ONCE PER BLOCK into the POD snapshot on the audio thread (ADR-0001/0011 —
// never piecemeal mid-block).
//
// 44, not 49: the five macro/view-only parameters (advancedMode, loudness,
// character, tone, freeze) never reach the engine. The cache order and
// toEngine's assignment sequence are coupled POSITIONALLY — inserting one
// without the other silently shifts every later field — so a static_assert
// pins the count and `testCachedParamsMapping` pins the field-by-field
// mapping end to end.
inline constexpr int kCachedParamCount = 44;

struct CachedParams
{
    void resolve (juce::AudioProcessorValueTreeState& apvts);

    // Fills every APVTS-sourced field of the snapshot. The InternalState
    // mirrors (oversample/phase/offline) and the nonRealtime flag are the
    // caller's to add — they are not APVTS parameters (§4.3).
    void toEngine (anabasis::EngineParameters& out) const noexcept;

    std::atomic<float>* raw[kCachedParamCount] = {};
};
