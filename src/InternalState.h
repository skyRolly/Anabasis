#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "dsp/EngineParameters.h"
#include <atomic>
#include <functional>

// ============================================================================
//  InternalState — host-hidden session state (DESIGN §4.3, ADR-0010).
//
//  Copy-and-adapt provenance (ADR-0009): pattern from
//  Anamorph:src/InternalState.h:10-29 [Verified]. `withAutomatable(false)`
//  does not hide a VST3 parameter in every host (REAPER lists them all), so
//  anything non-musical stays OUT of the parameter tree entirely.
//
//  These fields persist with the session (so offline renders reproduce) but
//  NEVER participate in A/B, undo, or presets.
//
//  The three latency inputs — int_oversample, int_osPhase,
//  int_offlineQuality — drive the DSP through atomic mirrors, and their
//  onChanged fires the PDC-recompute callback (ADR-0004 item 5; the fourth
//  trigger, setNonRealtime(), lives in the processor).
// ============================================================================

namespace iid
{
    inline const juce::Identifier oversample     { "int_oversample" };     // 0..4 = Off/2x/4x/8x/16x
    inline const juce::Identifier osPhase        { "int_osPhase" };        // 0 min, 1 linear
    inline const juce::Identifier offlineQuality { "int_offlineQuality" }; // 0 Follow, 1 Force Max
    inline const juce::Identifier ceilingLock    { "int_ceilingLock" };    // bool
    inline const juce::Identifier uiScale        { "int_uiScale" };        // percent: 80..200
    inline const juce::Identifier tooltipsOn     { "int_tooltipsOn" };     // bool
    inline const juce::Identifier uiAnimations   { "int_uiAnimations" };   // bool
    inline const juce::Identifier spectrumOn     { "int_spectrumOn" };     // bool (dismissible, brief §6)
    inline const juce::Identifier meterTargets   { "int_meterTargets" };   // bitmask, all on
    inline const juce::Identifier tpMeterOn      { "int_tpMeterOn" };      // bool
}

class InternalState : private juce::ValueTree::Listener
{
public:
    InternalState()
    {
        tree = juce::ValueTree ("ANABASIS_INTERNAL");
        setDefaults();
        tree.addListener (this);
        syncAtomics();
    }

    void setDefaults()
    {
        tree.setProperty (iid::oversample,     0,     nullptr);   // ⊕ Off
        tree.setProperty (iid::osPhase,        0,     nullptr);   // ⊕ min-phase
        tree.setProperty (iid::offlineQuality, 0,     nullptr);   // ⊕ Follow
        tree.setProperty (iid::ceilingLock,    false, nullptr);
        tree.setProperty (iid::uiScale,        100,   nullptr);
        tree.setProperty (iid::tooltipsOn,     false, nullptr);
        tree.setProperty (iid::uiAnimations,   true,  nullptr);
        tree.setProperty (iid::spectrumOn,     true,  nullptr);
        tree.setProperty (iid::meterTargets,   ~0,    nullptr);
        tree.setProperty (iid::tpMeterOn,      true,  nullptr);
    }

    ~InternalState() override { tree.removeListener (this); }

    // --- audio-thread mirrors (read once per block into the POD) -----------
    anabasis::OversampleFactor oversampleFactor() const noexcept
    { return (anabasis::OversampleFactor) osMirror.load (std::memory_order_relaxed); }
    anabasis::OsPhaseMode osPhaseMode() const noexcept
    { return (anabasis::OsPhaseMode) phaseMirror.load (std::memory_order_relaxed); }
    bool forceMaxOffline() const noexcept
    { return offlineMirror.load (std::memory_order_relaxed) != 0; }
    bool ceilingLocked() const noexcept
    { return lockMirror.load (std::memory_order_relaxed) != 0; }

    // Fired on the three latency-input changes so the wrapper re-reports PDC.
    std::function<void()> onLatencyInputChanged;

    // --- session persistence (message thread) ------------------------------
    juce::ValueTree& state() { return tree; }

    void replaceFrom (const juce::ValueTree& incoming)
    {
        // §4.4 read rules, applied the same way the A/B slot fields are
        // (PluginProcessor::resetSlotFieldsToDefaults): DEFAULTS FIRST, then
        // overlay whatever the incoming tree actually carries. Returning early
        // on a missing child — or leaving absent properties alone — would let
        // the previous session's oversampling factor, phase mode, offline
        // quality or ceiling lock survive into a newly loaded one, which is
        // the "chimera of two sessions" class this schema forbids.
        // The whole read is ONE latency event, not one per property. Without
        // the batch, setDefaults() writes three latency inputs and the overlay
        // rewrites them, so `onLatencyInputChanged` — i.e. updateLatency() →
        // setLatencySamples() — fires up to six times per session load, walking
        // the reported figure through the DEFAULT (Off) value before landing on
        // the session's. That is invisible at P1 only because osLatencySamples()
        // returns 0; once oversampling lands it is a burst of PDC changes
        // mid-load, which is exactly what ADR-0004's constant allowance exists
        // to prevent. The atomics still sync per property — they are read per
        // block and cost nothing.
        const ScopedLatencyBatch batch (*this);

        setDefaults();
        if (incoming.isValid() && incoming.hasType ("ANABASIS_INTERNAL"))
            for (int i = 0; i < incoming.getNumProperties(); ++i)
            {
                const auto name = incoming.getPropertyName (i);
                if (tree.hasProperty (name))           // unknown fields ignored (schema v1 read rules)
                    tree.setProperty (name, incoming.getProperty (name), nullptr);
            }
        // …and the single fire happens in ~ScopedLatencyBatch, so an early
        // return could not skip it. A missing child still lands on defaults.
    }

private:
    // Coalesces a bulk read's latency notifications into exactly one, fired on
    // the way out whether or not any latency input actually moved: a session
    // load re-reports PDC once, which is the ADR-0004 item 5 contract, and the
    // wrapper's setLatencySamples no-ops when the figure is unchanged.
    struct ScopedLatencyBatch
    {
        explicit ScopedLatencyBatch (InternalState& s) noexcept : owner (s)
        { owner.batchingLatencyNotify = true; }

        ~ScopedLatencyBatch()
        {
            owner.batchingLatencyNotify = false;
            if (owner.onLatencyInputChanged)
                owner.onLatencyInputChanged();
        }

        InternalState& owner;
        JUCE_DECLARE_NON_COPYABLE (ScopedLatencyBatch)
    };

    void syncAtomics()
    {
        osMirror.store      (juce::jlimit (0, 4, (int) tree.getProperty (iid::oversample)),     std::memory_order_relaxed);
        phaseMirror.store   (juce::jlimit (0, 1, (int) tree.getProperty (iid::osPhase)),        std::memory_order_relaxed);
        offlineMirror.store ((bool) tree.getProperty (iid::offlineQuality) ? 1 : 0,             std::memory_order_relaxed);
        lockMirror.store    ((bool) tree.getProperty (iid::ceilingLock) ? 1 : 0,                std::memory_order_relaxed);
    }

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& prop) override
    {
        syncAtomics();
        if (onLatencyInputChanged && ! batchingLatencyNotify
            && (prop == iid::oversample || prop == iid::osPhase || prop == iid::offlineQuality))
            onLatencyInputChanged();
    }

    juce::ValueTree tree;
    std::atomic<int> osMirror { 0 }, phaseMirror { 0 }, offlineMirror { 0 }, lockMirror { 0 };

    // Suppresses the per-property latency callback for the duration of a bulk
    // write; see replaceFrom.
    bool batchingLatencyNotify = false;

    // CODE_STYLE §Structure. This class registers ITSELF as a listener on a
    // tree it owns: a copy would share the tree without being registered,
    // while its destructor would still call removeListener — deregistering
    // the original. That is the failure the guard exists to make impossible.
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InternalState)
};
