#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginParameters.h"
#include "InternalState.h"
#include "MacroEngine.h"
#include "PresetManager.h"
#include "AbSlotIndex.h"
#include "dsp/AnabasisEngine.h"

// ============================================================================
//  AnabasisAudioProcessor — wrapper layer (ADR-0001 boundary; ADR-0011
//  threading; ADR-0004 latency; ADR-0007 state schema v1).
//
//  Latency: ONE setLatencySamples call site (updateLatency), fed by the const
//  race-free predictor in dsp/Latency.h, invoked from prepareToPlay, from the
//  InternalState onChanged of the three latency inputs, and from
//  setNonRealtime() — ADR-0004 decision item 5's full trigger set.
//
//  State: schema v1 (ADR-0007) — root AnabasisRoot { schemaVersion=1,
//  ANABASIS (+ additive exact `raw` attribute per PARAM), ANABASIS_INTERNAL,
//  AB { activeIndex + per-slot params/presetName/BASELINE/FROZEN_TRIMS/
//  DETACH_MASK }, ADAPTIVE }. Read rules: unknown ignored, missing default,
//  indices clamped.
//
//  OQ-013 HARD STOP: the frozenTrims message→audio inject transport is NOT
//  decided. The per-slot fields are serialized here (that is ADR-0007's
//  settled half); no code may wire the audio-side restore until the OQ-013
//  ADR lands.
// ============================================================================

class AnabasisAudioProcessor : public juce::AudioProcessor
{
public:
    AnabasisAudioProcessor();
    ~AnabasisAudioProcessor() override = default;

    // -- AudioProcessor -----------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;   // keep the double overload visible
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void setNonRealtime (bool isNonRealtime) noexcept override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override               { return true; }
    const juce::String getName() const override   { return "Anabasis"; }
    bool acceptsMidi() const override             { return false; }
    bool producesMidi() const override            { return false; }
    bool isMidiEffect() const override            { return false; }
    double getTailLengthSeconds() const override  { return 0.0; }
    int getNumPrograms() override                 { return 1; }
    int getCurrentProgram() override              { return 0; }
    void setCurrentProgram (int) override         {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // -- Anabasis -----------------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;
    InternalState internalState;

    int  activeSlotIndex() const noexcept { return activeSlot; }
    void switchToSlot (int newIndex);   // message thread; P1 form (TODO(P2): §2.8 duck)

private:
    void updateLatency();               // the single setLatencySamples call site
    juce::ValueTree saveSlotFromLive();
    void applySlotToLive (const juce::ValueTree& slot);
    void reassertFromRaw (const juce::ValueTree& apvtsTree);

    CachedParams cached;
    anabasis::AnabasisEngine engine;
    anabasis::EngineParameters snapshot;

    std::unique_ptr<MacroEngine> macroEngine;
    std::unique_ptr<PresetManager> presetManager;

    // A/B: the live APVTS is the active slot; the inactive one is stored here.
    // Per-slot StateSet = {params, presetName, baseline, frozenTrims,
    // detachMask} (ADR-0007) — all five fields or none on every copy path.
    int activeSlot = 0;
    juce::ValueTree storedSlot;          // the inactive slot's SLOT tree
    juce::String    livePresetName;
    juce::ValueTree liveBaseline;        // BASELINE (absent until a macro gesture, §5.3/P4)
    juce::ValueTree liveFrozenTrims;     // FROZEN_TRIMS (serialized only — OQ-013 blocks the inject)
    juce::StringArray liveDetachMask;

    std::atomic<bool> nonRealtimeFlag { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnabasisAudioProcessor)
};
