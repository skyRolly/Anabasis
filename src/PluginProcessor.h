#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginParameters.h"
#include "InternalState.h"
#include "MacroEngine.h"
#include "PresetManager.h"
#include "AbSlotIndex.h"
#include "dsp/AnabasisEngine.h"
#include "dsp/LoudnessMeter.h"
#include "dsp/TruePeak.h"
#include "dsp/GrHistoryBuffer.h"

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
//  AB { active + per-slot params/presetName/BASELINE/FROZEN_TRIMS/
//  DETACH_MASK }, ADAPTIVE }. Read rules: unknown ignored, missing default,
//  indices clamped.
//
//  OQ-013 HARD STOP: the frozenTrims message→audio inject transport is NOT
//  decided. The per-slot fields are serialized here (that is ADR-0007's
//  settled half); no code may wire the audio-side restore until the OQ-013
//  ADR lands.
// ============================================================================

class AnabasisAudioProcessor : public juce::AudioProcessor,
                               private juce::AudioProcessorListener,
                               private juce::AudioProcessorValueTreeState::Listener,
                               private juce::AsyncUpdater
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
    void switchToSlot (int newIndex);   // message thread; §2.8 duck requested before the swap

    // Top-bar Copy (§6.1): the INACTIVE slot becomes a snapshot of the live
    // state. No duck and no engine involvement — nothing audible changes, the
    // copy lands where the next A/B switch will read it.
    void copySlotToOther() { storedSlot = saveSlotFromLive(); }

    // Preset apply goes through here, never through PresetManager directly:
    // the wrapper lands the slot-level fields (name, detach mask) and drops
    // the restore-armed macro mapping (§5.3: a restore is not a gesture).
    bool applyPresetFile (const juce::File& file);

    // Host-side bypass routes through our own `bypass` parameter, so it takes
    // the engine's delay-aligned crossfade path instead of the wrapper's
    // zero-latency processBlockBypassed fallback.
    juce::AudioProcessorParameter* getBypassParameter() const override;

    // -- P5 editor accessors (message thread) -------------------------------
    // The preset browser shows the live name; save routes through the wrapper
    // for the same reason applyPresetFile does — the slot-level fields (name,
    // detach mask) belong to the wrapper, not to PresetManager's file I/O.
    const juce::String& currentPresetName() const noexcept { return livePresetName; }
    bool savePresetFile (const juce::File& file)
    {
        if (! presetManager->savePreset (file, liveDetachMask))
            return false;
        livePresetName = file.getFileNameWithoutExtension();
        return true;
    }
    // Read-only view of the §5.3 detach mask, for the Advanced macro row's
    // badges and the Simple view's "edited" indicator (display only — the
    // detach GRAMMAR lives in the wrapper/MacroEngine, never in paint code).
    const juce::StringArray& detachMask() const noexcept { return liveDetachMask; }

    // §5.3 step 4 — "reset to macro": re-engage every detached parameter
    // WITHOUT moving a macro. Message thread.
    void resetToMacro()
    {
        liveDetachMask.clear();
        macroEngine->refreshMapping();
    }

    // Deterministic drain for the headless tests (mirrors flushPendingMapping):
    // lands any pending detach bits / re-engage into liveDetachMask now.
    void flushPendingDetach() { handleAsyncUpdate(); }

    MacroEngine& getMacroEngine() noexcept { return *macroEngine; }
    const CachedParams& cachedForTest() const noexcept { return cached; }
    PresetManager& getPresetManager() noexcept { return *presetManager; }

private:
    void updateLatency();               // the single setLatencySamples call site
    juce::ValueTree copyStateWithRaw();  // APVTS copy + additive exact-`raw` per PARAM
    void adoptParamsTree (const juce::ValueTree& paramsWithRaw);   // strip → replaceState → reassert
    juce::ValueTree saveSlotFromLive();
    void applySlotToLive (const juce::ValueTree& slot);
    void reassertFromRaw (const juce::ValueTree& apvtsTree);
    void resetSlotFieldsToDefaults();

    CachedParams cached;
    anabasis::AnabasisEngine engine;
    anabasis::EngineParameters snapshot;

    std::unique_ptr<MacroEngine> macroEngine;
    std::unique_ptr<PresetManager> presetManager;

    // A/B: the live APVTS is the active slot; the inactive one is stored here.
    // Per-slot StateSet = {params, presetName, baseline, frozenTrims,
    // detachMask} (ADR-0007) — all five fields or none on every copy path.
    int activeSlot = 0;
    juce::ValueTree defaultSlot;         // pristine defaults, for the missing-AB read rule
    juce::ValueTree storedSlot;          // the inactive slot's SLOT tree
    juce::String    livePresetName;
    juce::ValueTree liveBaseline;        // BASELINE (absent until a macro gesture, §5.3/P4)
    juce::ValueTree liveFrozenTrims;     // FROZEN_TRIMS (serialized only — OQ-013 blocks the inject)
    juce::StringArray liveDetachMask;

    std::atomic<bool> nonRealtimeFlag { false };

    // Mirror of the ADAPTIVE record last staged to the engine. The engine only
    // adopts a staged restore at a block top, so between a load and the next
    // audio block the engine's own learned state is STALE — and a host that
    // loads a project and immediately re-saves it (duplicate track, copy
    // plugin state, save without transport) would otherwise serialize that
    // stale answer and drop the loaded session's Learn.
    //
    // Atomic, though both writer and reader are nominally the message thread:
    // VST3 does not promise which thread delivers setStateInformation
    // (KNOWN_ISSUES KI-003), so a concurrent save could otherwise read a
    // half-written mirror. Relaxed is enough — the three are independent
    // scalars whose fallback (the engine's own atomics) is coherent, and
    // ADR-0012's known-limits section already scopes the record's coherence.
    // -- §5.3 detach discriminator (ADR-0005's P5 half) ---------------------
    // A managed parameter detaches when a change arrives that is
    //   (1) gesture-bracketed — a real user drag, begin/endChangeGesture —
    //   (2) not macro-originated (isApplyingMacro), and
    //   (3) not part of a restore (isRestoring).
    // Ungestured writes (automation playback, preset/A-B/session restores)
    // never detach. The callbacks can arrive off the message thread (APVTS
    // rule; VST3 gesture threading is host-defined), so the listener only
    // sets lock-free bits keyed by managed_params index; the message thread
    // drains them into `liveDetachMask` — the same marshalling shape as
    // MacroEngine's mappingPending (both are the OQ-014 pattern, and OQ-014's
    // owner call covers this edge under whichever reading it takes).
    // A gesture that begins on a MACRO re-engages instead: §5.3's "the next
    // macro-knob gesture re-engages ALL detached params".
    void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int parameterIndex) override;
    void audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int parameterIndex) override;
    void audioProcessorChanged (juce::AudioProcessor*, const juce::AudioProcessorListener::ChangeDetails&) override {}
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    std::atomic<uint32_t> managedGestureBits { 0 };   // bit n = managed_params::ids[n] mid-gesture
    std::atomic<uint32_t> pendingDetachBits  { 0 };
    std::atomic<bool>     pendingReengage    { false };

    std::atomic<bool>  stagedAdaptiveLearned { false };
    std::atomic<float> stagedRefOnset { anabasis::AdaptiveEngine::kDefaultRefOnset };
    std::atomic<float> stagedRefTilt  { anabasis::AdaptiveEngine::kDefaultRefTilt };

public:
    // -- §2.9 meter publication (THREAD_MODEL: Audio→GUI relaxed atomics,
    //    one publish per block; readers are the editor's paint sites) -------
    float meterLufsM()    const noexcept { return pubLufsM.load (std::memory_order_relaxed); }
    float meterLufsS()    const noexcept { return pubLufsS.load (std::memory_order_relaxed); }
    float meterLufsI()    const noexcept { return pubLufsI.load (std::memory_order_relaxed); }
    float meterDbTpMax()  const noexcept { return pubDbTpMax.load (std::memory_order_relaxed); }
    float meterPlr()      const noexcept { return pubPlr.load (std::memory_order_relaxed); }
    float meterGrDb()     const noexcept { return pubGrDb.load (std::memory_order_relaxed); }
    const anabasis::GrHistoryBuffer& grHistory() const noexcept { return grHistoryRing; }

    // §2.9 meter-hold reset — the THREADING_POLICY momentary-request row
    // (single atomic, payload-free, consumed with `exchange` at the top of
    // processBlock), the shape THREAD_MODEL reserved for it at P3. Clears the
    // session-cumulative display state ONLY: the integrated-LUFS histogram
    // (engine render meter) and the wrapper's dBTP max-hold; PLR follows by
    // derivation. Two callers: the P5 meter panel's reset affordance, and
    // setStateInformation — the P5 decision THREAD_MODEL left open is taken
    // there: loading a session clears the previous programme's holds.
    void requestMeterReset() noexcept
    { meterResetPending.store (true, std::memory_order_relaxed); }

    // §5.4 Learn (message thread → the engine's command atomics; the P5 UI
    // adds the duck-routed engage + undo bracketing around these).
    void startLearn() noexcept { engine.requestLearnStart(); }
    // NOTE: the commit runs at the next block top, so a stop with no further
    // audio leaves the pass uncommitted and getStateInformation writes no
    // ADAPTIVE child — the mirror image of the load-then-save case the staged
    // mirror below fixes, and NOT fixable the same way: the sums live on the
    // audio thread, so there is nothing for the message thread to mirror. The
    // P5 Learn grammar owes an acknowledged commit (the UI cannot offer "save"
    // until the engine has confirmed), which is where this closes.
    void stopLearn() noexcept  { engine.requestLearnStop(); }
    const anabasis::AdaptiveEngine& adaptiveReadout() const noexcept
    { return const_cast<AnabasisAudioProcessor*> (this)->engine.adaptiveForWrapper(); }

private:
    // The output LUFS/TP meters live in the ENGINE (its §2.9 render tap) —
    // only the engine sees the sample before the monitor-only stages touch
    // it. The wrapper keeps the session max-hold and the publish atomics.
    anabasis::GrHistoryBuffer   grHistoryRing;    // SPSC, audio writes
    float dbTpMaxHold = -144.0f;                  // audio-thread session max
    std::atomic<bool> meterResetPending { false };
    std::atomic<float> pubLufsM { anabasis::LoudnessMeter::kSilentLufs },
                       pubLufsS { anabasis::LoudnessMeter::kSilentLufs },
                       pubLufsI { anabasis::LoudnessMeter::kSilentLufs },
                       pubDbTpMax { -144.0f }, pubPlr { 0.0f }, pubGrDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnabasisAudioProcessor)
};
