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
    const juce::String getProgramName (int) override { return "Default"; }   // the sibling's constant
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // -- Anabasis -----------------------------------------------------------
    // DECLARED BEFORE `apvts`, and that buys exactly ONE of the two halves.
    //
    // CONSTRUCTION — a real guarantee. `createAnabasisLayout (&ceilingUnit)`
    // runs inside `apvts`'s member initialiser, so the holder has to be
    // constructed already. Declaration order is what makes that true; moving
    // this line below `apvts` would hand the layout the address of a member
    // that does not exist yet.
    //
    // DESTRUCTION — NOT a guarantee, and this comment used to claim it was.
    // The value-text lambda lives inside the `ceiling` parameter, and the
    // parameter is NOT owned by `apvts`: the APVTS constructor hands every
    // layout parameter to `AudioProcessor::addParameter`, so the base class
    // owns them and `~AudioProcessor` — which runs AFTER every derived member
    // — is what destroys them. So the lambda outlives this holder no matter
    // where it is declared. `apvts` is destroyed before it too, which leaves
    // `truePeakRaw` (a pointer into APVTS storage) dangling for the rest of
    // the derived teardown.
    //
    // WHAT ACTUALLY MAKES IT SAFE is a runtime fact, not a structural one:
    // `getText` is called by hosts and by the editor while the processor is
    // live, and nothing in JUCE queries parameter text from a destructor. The
    // hazard is latent, not live. It is written down rather than papered over
    // because the fallback would NOT rescue a teardown-time read — reading a
    // destroyed `std::atomic` is UB before any value it held matters — so a
    // future change that queries parameter text during destruction needs a
    // handle the parameters can own (a shared holder), not a re-ordering of
    // these two lines. ADR-0015 §5 carries the same correction.
    CeilingUnitSource ceilingUnit;
    juce::AudioProcessorValueTreeState apvts;
    InternalState internalState;

    int  activeSlotIndex() const noexcept { return activeSlot; }
    void switchToSlot (int newIndex);   // message thread; §2.8 duck requested before the swap

    // Top-bar Copy (§6.1): the INACTIVE slot becomes a snapshot of the live
    // state. No duck and no engine involvement — nothing audible changes, the
    // copy lands where the next A/B switch will read it. Since ADR-0018 the
    // Copy is an UNDO STEP on the destination slot — its pre-copy state is
    // pushed onto that slot's stack, whose older entries are KEPT (entries are
    // absolute snapshots, so the pre-copy history stays reachable beneath the
    // new entry) — the sibling's semantics, replacing the clear-both-stacks
    // answer 0.1.0 shipped. Body in the .cpp: it pushes through `pushCapped`.
    void copySlotToOther();

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

    // Preset dirty marker (Anamorph grammar): the live state differs from the
    // state the named preset landed, MEASURED IN WHAT A PRESET CAN CARRY —
    // `presetShapeFromLive()` owns that projection and explains why it is not
    // the slot tree. Message thread; the compare walks the projection, so
    // callers poll it at display rate, not per frame. No preset loaded = never
    // dirty (there is nothing to differ from).
    //
    // Genuinely const now, and not by a cast: the projection reads the fixed
    // parameter list and their atomic values, so no `APVTS::copyState()` (whose
    // non-constness forced the old const_cast) and no ValueTree member is
    // touched. Both baseline and comparand come from the SAME function, so the
    // two sides cannot describe different shapes.
    bool presetDirty() const
    {
        if (livePresetName.isEmpty() || ! presetBaseline.isValid())
            return false;
        return ! presetBaseline.isEquivalentTo (presetShapeFromLive());
    }
    bool savePresetFile (const juce::File& file)
    {
        if (! presetManager->savePreset (file, liveDetachMask))
            return false;
        livePresetName = file.getFileNameWithoutExtension();
        presetBaseline = presetShapeFromLive();   // a just-saved preset is clean
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
    //
    // NOT const, and that is the point rather than an oversight: every reader
    // and every writer of the four stacks goes through `syncHistory()` first,
    // which is the ONE place the message thread reconciles a session load. See
    // `historyEpoch`.
    bool canUndo() noexcept { syncHistory(); return ! undoStacks[activeSlot].isEmpty(); }
    bool canRedo() noexcept { syncHistory(); return ! redoStacks[activeSlot].isEmpty(); }
    void undo();
    void redo();

    // §5.3 step 4 — "reset to macro": re-engage every detached parameter
    // WITHOUT moving a macro. Message thread.
    void resetToMacro()
    {
        // §7: this is a user-visible multi-parameter change — the whole detach
        // mask plus the nine §5.5 managed values — so it takes an undo step
        // like every other one. It had none: the writes are ungestured (so the
        // drag path never sees them) and the mask replacement is not a
        // parameter write at all, which left the one affordance in the Simple
        // view that changes nine parameters at once as the only one the user
        // could not take back. Pushed BEFORE the change, exactly as the preset
        // applies do, so the entry is the pre-state. No duck request: unlike a
        // preset apply or an undo this rewires no discrete stage — it moves
        // nine continuous, smoothed values — and DSP invariant 8's click-free
        // enumeration is about the bulk swaps that do.
        pushUndoStep (saveSlotFromLive());
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
        // The two stores ARE the mechanism, in every build. There is
        // deliberately no `jassert` that they were already clear: a bit can be
        // legitimately set at this point. `applyFactoryPreset` calls
        // `replaceDetachMask` INSIDE its `ScopedRestore` and reaches here
        // several statements after the guard drops, and
        // `AnabasisAudioProcessor::parameterChanged` stages a bit from
        // whichever thread the host delivers the callback on — the whole
        // atomic-bit machinery exists because VST3 gesture threading is
        // host-defined (KI-003). Asserting "no edit is waiting" in the one
        // place written to cope with an edit waiting made a debug build abort
        // on ordinary preset browsing. Dropping the bits is the correct
        // outcome either way: a gesture racing a bulk swap belongs to the
        // state being replaced, which is exactly what `replaceDetachMask`
        // already decides for every other path.
        pendingDetachBits.store (0, std::memory_order_relaxed);
        pendingReengage.store (false, std::memory_order_relaxed);
        // The result is deliberately not checked, and that is a statement
        // rather than an omission: `refreshMapping()` returns false only while
        // a restore is in flight or after teardown, and every caller of this
        // function is outside both by construction — `applyFactoryPreset`
        // drops its `ScopedRestore` first (its own paragraph says why),
        // `resetToMacro` and the §5.3 re-engage never open one. If a future
        // caller does need this inside a restore, the answer is not to check
        // the flag here but to move the call after the guard, because the
        // deferral is not a retry: the arm is dropped, not queued.
        macroEngine->refreshMapping();
    }
    void publishSilentMeters() noexcept;   // the six meter atomics, cleared (one list)
    void adoptFrozenMirror (juce::ValueTree frozen);   // the ONLY writer of liveFrozenTrims
    juce::ValueTree engineFrozenTrimsIfLive();   // ADR-0014 ownership test (one rule, one reader)
    // The retained-trim generation at the moment the live surface's frozen
    // ownership last changed — see `engineFrozenTrimsIfLive`. Atomic because the
    // writers are the restore paths (message OR host thread) and the reader is
    // `saveSlotFromLive`, which the editor poll and `getStateInformation` both
    // reach; relaxed because it is a comparand, not a publication.
    std::atomic<juce::uint32> slotFrozenBase { 0 };
    juce::ValueTree copyStateWithRaw();  // APVTS copy + additive exact-`raw` per PARAM
    void adoptParamsTree (const juce::ValueTree& paramsWithRaw);   // strip → replaceState → reassert
    juce::ValueTree saveSlotFromLive();
    // The live state reduced to preset content — the dirty marker's datum on
    // BOTH sides of its compare. See the definition for why the slot tree is
    // not that datum.
    juce::ValueTree presetShapeFromLive() const;
    // adoptAdvanced (ADR-0018): the UNDO/REDO restore adopts `advancedMode`
    // from the slot tree (an ADV toggle is an undoable step); every other
    // adoption path — A/B switch, Copy — pins it to the live value, because
    // an A/B compare is a sound compare and must not resize the editor.
    void applySlotToLive (const juce::ValueTree& slot, bool adoptAdvanced = false);
    // The same ADR-0018 pin, applied at PUSH time instead of adopt time — see
    // the definition. Only `copySlotToOther` needs it, and only because its
    // undo entry is the one entry whose slot tree was not captured at the
    // moment of the step it records.
    juce::ValueTree slotWithLiveAdvancedMode (const juce::ValueTree& slot);
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
    // A §7 history entry is the StateSet PLUS the dirty datum that described
    // it. `presetBaseline` is deliberately NOT in the StateSet — it is not
    // serialized, and putting it there would be an ADR-0007 schema change and
    // a Hard Stop — but undo/redo restore the whole slot INCLUDING
    // `presetName`, so leaving the baseline behind left the name and the datum
    // describing different presets: undoing a preset apply restored the
    // previous state while the top bar kept comparing against the applied
    // preset. The stacks are session-local and never serialized, so carrying
    // the pair costs nothing outside memory and settles the pairing at the one
    // place entries are made and taken.
    struct UndoEntry
    {
        juce::ValueTree slot;       // the StateSet to restore
        juce::ValueTree baseline;   // `presetBaseline` as it was beside it
    };
    juce::Array<UndoEntry> undoStacks[anabasis::kNumAbSlots],
                           redoStacks[anabasis::kNumAbSlots];
    juce::ValueTree gesturePreState;     // armed at first gesture-begin

    // §7 history ownership, settled at round 42. The four stacks and the
    // gesture snapshot are plain `juce::Array<ValueTree>` / `ValueTree`, so they
    // have exactly one legal thread — and `setStateInformation` used to CLEAR
    // them, from a callback VST3 does not promise on the message thread (the
    // premise KI-003, `restoreFrozenTrims` and the `stopDraining` split all
    // rest on), while the editor read `canUndo()` at 24 Hz and `undo()` popped
    // from the same arrays. A load could therefore race a `clear()` against a
    // `removeAndReturn()`.
    //
    // The fix is ownership, not a lock: the loader now only BUMPS this counter,
    // which is the whole message ("the session you have is not the session I
    // just installed"), and the message thread does the clearing itself at the
    // next `syncHistory()`. No thread but the message thread touches the
    // containers, so there is nothing left to race — and nothing blocks in a
    // host callback, which a mutex here would have done on every load.
    //
    // RELAXED on both sides, and that is this build's own test applied rather
    // than laziness (THREADING_POLICY's publication-flag row): observing this
    // counter gates a CLEAR, not a read of state the loader wrote, so it
    // announces no payload. The session data the loader writes beside it —
    // `apvts.replaceState`, `liveDetachMask`, `livePresetName` — is unordered
    // against the editor for reasons KI-003 owns and that no ordering here
    // would fix.
    std::atomic<juce::uint32> historyEpoch { 0 };
    juce::uint32 historyEpochSeen = 0;    // message thread only, hence not atomic

    // Message thread ONLY. Idempotent, and cheap in the common case: one
    // relaxed load and a compare. Called at the top of every path that reads or
    // writes the history, so "the stacks are reconciled before use" is one rule
    // with one enforcement point rather than a condition each caller repeats.
    void syncHistory() noexcept
    {
        const auto epoch = historyEpoch.load (std::memory_order_relaxed);
        if (epoch == historyEpochSeen)
            return;
        historyEpochSeen = epoch;
        for (int slot = 0; slot < anabasis::kNumAbSlots; ++slot)
        {
            undoStacks[slot].clear();
            redoStacks[slot].clear();
        }
        // The in-flight snapshot goes with them: it describes a session that is
        // no longer loaded, so a drag still open across the load must not push
        // it. (`openGestureBits` is an atomic and the loader clears it directly,
        // so the end would match nothing anyway — this is the second half of
        // the same statement, kept beside the first.)
        gesturePreState = {};
    }
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
    // The §2.9 Waveform-Statistics additions (ADR-0020). Same row, same
    // contract — relaxed, one publish per block, cleared by the same list.
    float meterPeakMaxDb()  const noexcept { return pubPeakMaxDb.load (std::memory_order_relaxed); }
    // MATHEMATICAL reference (a full-scale sine reads -3.01). The AES-17
    // convention is +3.01 dB and is applied at DISPLAY time from the Settings
    // choice — see `RmsMeter`'s header for why the offset is deferred.
    float meterRmsDb()      const noexcept { return pubRmsDb.load (std::memory_order_relaxed); }
    // BOTH integrated readings are published; WHICH is shown is the Settings
    // choice, resolved on the message thread. Publishing both is what keeps
    // the audio thread from ever reading a UI preference (ADR-0020).
    float meterLufsIUngated() const noexcept { return pubLufsIUngated.load (std::memory_order_relaxed); }
    float meterLra()          const noexcept { return pubLra.load (std::memory_order_relaxed); }
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
    //
    // The DISPLAY clear is part of the request, not something a caller adds
    // beside it. The ENGINE half genuinely has to wait for a block top (it is
    // audio-thread state); the six published atomics do not, and a request
    // that only sets the flag is INVISIBLE until audio flows — indefinitely so
    // if the host is stopped, which is the ordinary condition for both
    // callers. `setStateInformation` learned that at round 33 and paired the
    // two calls at its own site; the meter panel's click did not, so the same
    // reset read as "the button does nothing" from the GUI while working from
    // a session load. One caller having to remember a second call is the shape
    // this file keeps removing: the pairing lives HERE, so both are consistent
    // by construction and a third caller cannot get it wrong. Relaxed stores
    // throughout (see publishSilentMeters), so this stays callable from any
    // thread; if audio is in fact running, the next block's own publish
    // overwrites the cleared values within one block.
    void requestMeterReset() noexcept
    {
        // PUBLISH FIRST, ANNOUNCE SECOND, and the order is the fix rather than
        // a preference. The other way round, the audio thread could consume the
        // flag and complete a whole block — clearing `dbTpMaxHold`, running the
        // engine, publishing fresh readings — in the gap between the two
        // statements, after which this thread's `publishSilentMeters()` wrote
        // the silent placeholders OVER readings that were already post-reset.
        // The display then showed a blank meter that the audio had already
        // restarted, which is the one transient a reset should never produce.
        //
        // Reversed, the worst case is the opposite and benign: a block already
        // past its top publishes pre-reset values after the silence, and the
        // NEXT block top consumes the flag and clears again — the reset lands
        // one frame late instead of the display going stale-blank.
        //
        // RELEASE on the flag, ACQUIRE on the `exchange` that consumes it, for
        // the reason THREADING_POLICY's publication-flag row gives: all six
        // meter atomics are relaxed and carry no ordering of their own, so
        // source order alone would not stop the consumer observing the flag
        // before the values. The flag announces them, so it orders them.
        publishSilentMeters();
        meterResetPending.store (true, std::memory_order_release);
    }

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
    // Audio-thread session max-holds. `samplePeakMaxHold` joined `dbTpMaxHold`
    // with the stats row (ADR-0020) and is cleared by exactly the same two
    // sites, for the same reason: both are session-cumulative, so both belong
    // to `requestMeterReset`'s contract rather than to the rolling windows.
    float dbTpMaxHold = -144.0f, samplePeakMaxHold = -144.0f;
    std::atomic<bool> meterResetPending { false };
    std::atomic<float> pubLufsM { anabasis::LoudnessMeter::kSilentLufs },
                       pubLufsS { anabasis::LoudnessMeter::kSilentLufs },
                       pubLufsI { anabasis::LoudnessMeter::kSilentLufs },
                       pubDbTpMax { -144.0f }, pubPlr { 0.0f }, pubGrDb { 0.0f },
                       pubPeakMaxDb { -144.0f },
                       pubRmsDb { anabasis::RmsMeter::kSilentDb },
                       pubLufsIUngated { anabasis::LoudnessMeter::kSilentLufs },
                       pubLra { anabasis::LoudnessMeter::kNoLra };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnabasisAudioProcessor)
};
