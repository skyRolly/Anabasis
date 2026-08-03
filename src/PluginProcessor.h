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
//  OQ-013: RESOLVED by ADR-0014 (2026-08-02, owner-approved) — the per-slot
//  frozen-trim vector is captured from the published trims at save (Freeze
//  on), staged to the engine on ADR-0012's record row at restore, and lands
//  at the §2.8 duck's silent bottom. The Hard Stop this banner carried for
//  five phases is lifted by that ADR.
// ============================================================================

class AnabasisAudioProcessor : public juce::AudioProcessor,
                               private juce::AudioProcessorListener,
                               private juce::AudioProcessorValueTreeState::Listener
{
public:
    AnabasisAudioProcessor();
    // NOT `= default`: the MacroEngine's 30 ms tick calls back into this object
    // (`onDrainTick` → `handleAsyncUpdate` → `liveDetachMask`), and
    // `macroEngine` is declared BEFORE those members, so reverse-order
    // destruction frees everything the tick touches while the timer is still
    // armed. The body stops the drain first. Same premise as the split that
    // moved `startTimer` out of MacroEngine's constructor: the destroying
    // thread is not promised to be the message thread (KI-003).
    ~AnabasisAudioProcessor() override;

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
    // Copy A→B (or B→A): the dirty datum travels with the values, or the
    // copy would land looking edited against whatever the other slot held.
    void copySlotToOther()
    {
        storedSlot = saveSlotFromLive();
        storedPresetBaseline = presetBaseline;
        // …and the destination's history goes with the state it described.
        // A per-slot undo stack records edits made FROM that slot's own values
        // (§7 / ADR-0010); a Copy replaces those values wholesale from outside
        // that history, so every entry now describes a state the slot no
        // longer has. Leaving them, the first undo after switching to B
        // restored a pre-copy state the user never edited from — silently
        // discarding the copy AND B's last edit, because the copy itself is
        // not an undo step. `setStateInformation` already clears both slots'
        // stacks for exactly this reason ("a load starts a fresh history");
        // a copy is that event for one slot, so it takes the same answer.
        const int other = 1 - activeSlot;
        undoStacks[other].clear();
        redoStacks[other].clear();
    }

    // Preset apply goes through here, never through PresetManager directly:
    // the wrapper lands the slot-level fields (name, detach mask) and drops
    // the restore-armed macro mapping (§5.3: a restore is not a gesture).
    bool applyPresetFile (const juce::File& file);
    bool applyFactoryPreset (int index);   // same bracket/duck/restore semantics

    // Host-side bypass routes through our own `bypass` parameter, so it takes
    // the engine's delay-aligned crossfade path instead of the wrapper's
    // zero-latency processBlockBypassed fallback.
    juce::AudioProcessorParameter* getBypassParameter() const override;

    // -- P5 editor accessors (message thread) -------------------------------
    // The preset browser shows the live name; save routes through the wrapper
    // for the same reason applyPresetFile does — the slot-level fields (name,
    // detach mask) belong to the wrapper, not to PresetManager's file I/O.
    const juce::String& currentPresetName() const noexcept { return livePresetName; }

    // Preset dirty marker (Anamorph grammar): the live slot differs from the
    // state the named preset landed. Message thread; the compare is a full
    // slot-tree equivalence, so callers poll it at display rate, not per
    // frame. No preset loaded = never dirty (there is nothing to differ from).
    //
    // The const_cast is JUCE's, not ours: `APVTS::copyState()` is non-const, so
    // the snapshot cannot be taken through a const path. `saveSlotFromLive()`
    // itself mutates NOTHING — it builds the tree and returns it — and that is
    // load-bearing rather than incidental: while it also wrote back
    // `liveFrozenTrims`, this ~3 Hz display poll could overwrite a just-loaded
    // frozen vector with the engine's pre-restore trims (ADR-0014's mirror
    // window). Keep it a pure read.
    bool presetDirty() const
    {
        if (livePresetName.isEmpty() || ! presetBaseline.isValid())
            return false;
        return ! presetBaseline.isEquivalentTo (
                   const_cast<AnabasisAudioProcessor*> (this)->saveSlotFromLive());
    }
    bool savePresetFile (const juce::File& file)
    {
        if (! presetManager->savePreset (file, liveDetachMask))
            return false;
        livePresetName = file.getFileNameWithoutExtension();
        presetBaseline = saveSlotFromLive();   // a just-saved preset is clean
        return true;
    }
    // Read-only view of the §5.3 detach mask, for the Advanced macro row's
    // badges and the Simple view's "edited" indicator (display only — the
    // detach GRAMMAR lives in the wrapper/MacroEngine, never in paint code).
    const juce::StringArray& detachMask() const noexcept { return liveDetachMask; }

    // -- §7 per-slot undo (DESIGN §7; P6) -----------------------------------
    // The undo UNIT is the five-field SLOT tree — the same StateSet A/B uses
    // (saveSlotFromLive / applySlotToLive), which is the §7 widening rationale
    // made mechanical: undoing an edit restores the value AND its detach bit,
    // and undoing a preset apply restores name + mask with the values.
    // Coalescing is gesture-gated: the pre-state is snapshotted when the
    // FIRST gesture opens and pushed when the LAST one closes (if anything
    // changed), so one drag = one step and host AUTOMATION — ungestured —
    // folds silently into the current state, exactly as §7 words it. Preset
    // applies bracket as one step (parse first: a failed parse pushes
    // nothing). Undo/redo restores run inside ScopedRestore (a restore is
    // not a gesture) and DO request the §2.8 duck, like every other bulk swap
    // — DSP_POLICY invariant 8 names the undo step as one of its three routes.
    // This comment used to say they never duck, on the reasoning that discrete
    // rewires are duck-routed by the engine regardless of who wrote them: true
    // for rewires, but an undo that moves no discrete stage produced no duck at
    // all, and after ADR-0014 that left the frozen-trim vector it stages with
    // no silent bottom to land at. Stacks are per slot, capped at
    // 128, and NEVER serialized (a session load clears all four).
    // Message-thread only; off-thread gesture callbacks skip the snapshot,
    // which degrades to the automation path (folded silently) rather than
    // touching ValueTrees from a foreign thread.
    bool canUndo() const noexcept { return ! undoStacks[activeSlot].isEmpty(); }
    bool canRedo() const noexcept { return ! redoStacks[activeSlot].isEmpty(); }
    void undo();
    void redo();

    // §5.3 step 4 — "reset to macro": re-engage every detached parameter
    // WITHOUT moving a macro. Message thread.
    void resetToMacro()
    {
        replaceDetachMask ({});      // …and drops any staged detach with it
        relandMacroCurve();
    }

    // Deterministic drain for the headless tests (mirrors flushPendingMapping):
    // lands any pending detach bits / re-engage into liveDetachMask now.
    void flushPendingDetach() { handleAsyncUpdate(); }

    MacroEngine& getMacroEngine() noexcept { return *macroEngine; }
    const CachedParams& cachedForTest() const noexcept { return cached; }
    PresetManager& getPresetManager() noexcept { return *presetManager; }

private:
    void updateLatency();               // the single setLatencySamples call site
    // `MacroEngine::refreshMapping()` behind the invariant it states but
    // cannot check for itself. Since every drain trigger routes through
    // `drainTick`, refreshing the mapping also runs THIS class's detach drain
    // re-entrantly — harmless only because the caller has already replaced the
    // mask, which zeroes the staged bits the nested drain would otherwise
    // apply in the middle of the caller's own apply. Both callers go through
    // here so a third one inherits the check instead of the trap.
    void relandMacroCurve()
    {
        // The jassert states the intent; the two stores ENFORCE it, in every
        // build. An assert alone is debug-only, so a future third caller that
        // skipped `replaceDetachMask()` would have shipped a release binary
        // applying staged detach bits in the middle of its own apply — the
        // enforcement would have been weaker than the sentence above it.
        // Dropping them is what `replaceDetachMask` already did, so this is a
        // no-op for both existing callers and idempotent by construction.
        jassert (pendingDetachBits.load (std::memory_order_relaxed) == 0
                 && ! pendingReengage.load (std::memory_order_relaxed));
        pendingDetachBits.store (0, std::memory_order_relaxed);
        pendingReengage.store (false, std::memory_order_relaxed);
        macroEngine->refreshMapping();
    }
    void publishSilentMeters() noexcept;   // the six meter atomics, cleared (one list)
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
    juce::Array<juce::ValueTree> undoStacks[anabasis::kNumAbSlots],
                                 redoStacks[anabasis::kNumAbSlots];
    juce::ValueTree gesturePreState;     // armed at first gesture-begin
    // The state the named preset landed, PER SLOT — it is the datum
    // `presetDirty()` compares against, and `livePresetName` is per-slot state,
    // so a single engine-wide copy described the wrong slot the moment the user
    // switched: after applying a preset in B and switching back to A, A's name
    // was marked against B's baseline. Deliberately NOT serialized and NOT part
    // of the ADR-0007 StateSet: it is a display datum, and a session records
    // which preset a slot holds, never whether it had been edited since.
    // Cleared with the other slot fields on a load, for the same reason.
    juce::ValueTree presetBaseline;      // active slot's
    juce::ValueTree storedPresetBaseline;// the inactive slot's, swapped by switchToSlot
    // One bit per parameter index with an OPEN message-thread gesture, so an
    // end can only close its own drag and cannot leak one (see the callbacks
    // for both asymmetries). Atomic because the END clears it on whichever
    // thread the host delivers it on, exactly like `managedGestureBits` beside
    // it — the listener-guard row (OQ-014). The parameter surface is 49 wide
    // and frozen by ADR-0010, so one word covers it with room to spare; a
    // hypothetical index past the word degrades to the untracked (automation)
    // path rather than mis-keying, and the constructor asserts the width so
    // the degradation cannot go unnoticed in a debug run.
    static constexpr int kMaxCountedGestureIndex = 64;
    std::atomic<uint64_t> openGestureBits { 0 };
    void pushUndoStep (juce::ValueTree preState);
    static constexpr int kUndoCap = 128;
    juce::ValueTree defaultSlot;         // pristine defaults, for the missing-AB read rule
    juce::ValueTree storedSlot;          // the inactive slot's SLOT tree
    juce::String    livePresetName;
    juce::ValueTree liveBaseline;        // BASELINE (absent until a macro gesture, §5.3/P4)
    juce::ValueTree liveFrozenTrims;     // FROZEN_TRIMS (captured at save, staged at restore — ADR-0014)
    juce::StringArray liveDetachMask;
    // The ONE writer of the mask above — every replacement path goes through
    // it, because it also drops the staged detach/re-engage inputs. See the
    // definition for why those two things are inseparable.
    void replaceDetachMask (const juce::StringArray& newMask);

    std::atomic<bool> nonRealtimeFlag { false };

    // Mirror of the ADAPTIVE record last staged to the engine. The engine only
    // adopts a staged restore at a block top, so between a load and the next
    // audio block the engine's own learned state is STALE — and a host that
    // loads a project and immediately re-saves it (duplicate track, copy
    // plugin state, save without transport) would otherwise serialize that
    // stale answer and drop the loaded session's Learn.
    //
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
    // MacroEngine's mappingPending (both sit on the listener-guard row the
    // OQ-014 resolution added to THREADING_POLICY, 2026-08-02, reading 1).
    // A gesture that begins on a MACRO re-engages instead: §5.3's "the next
    // macro-knob gesture re-engages ALL detached params".
    void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int parameterIndex) override;
    void audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int parameterIndex) override;
    void audioProcessorChanged (juce::AudioProcessor*, const juce::AudioProcessorListener::ChangeDetails&) override {}
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    // The §5.3 bit drain. NOT an AsyncUpdater callback any more, and the base
    // class is gone with it: nothing here may post to the message queue from a
    // listener callback (drainDetachBitsSoon says why), and an unused
    // AsyncUpdater sitting in the bases is an invitation to re-open that route.
    // Reached from the message thread only — directly when the callback already
    // runs there, otherwise on the MacroEngine's 30 ms tick.
    void handleAsyncUpdate();
    void drainDetachBitsSoon();
    std::atomic<uint32_t> managedGestureBits { 0 };   // bit n = managed_params::ids[n] mid-gesture
    std::atomic<uint32_t> pendingDetachBits  { 0 };
    std::atomic<bool>     pendingReengage    { false };

    // Atomic, though both writer and reader are nominally the message thread:
    // VST3 does not promise which thread delivers setStateInformation
    // (KNOWN_ISSUES KI-003), so a concurrent save could otherwise read a
    // half-written mirror. Relaxed is enough — the three are independent
    // scalars whose fallback (the engine's own atomics) is coherent, and
    // ADR-0012's known-limits section already scopes the record's coherence.
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
    float meterCompGrDb() const noexcept { return engine.lastCompGrDb(); }   // per-stage (P5 panels)
    const anabasis::GrHistoryBuffer& grHistory() const noexcept { return grHistoryRing; }
    const anabasis::ScopeBuffer& spectrumInRing()  const noexcept { return engine.spectrumInRing(); }
    const anabasis::ScopeBuffer& spectrumOutRing() const noexcept { return engine.spectrumOutRing(); }

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
