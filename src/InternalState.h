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
        tree.addListener (this);
        syncAtomics();
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
        if (! incoming.isValid() || ! incoming.hasType ("ANABASIS_INTERNAL"))
            return;                                    // missing → keep defaults
        for (int i = 0; i < incoming.getNumProperties(); ++i)
        {
            const auto name = incoming.getPropertyName (i);
            if (tree.hasProperty (name))               // unknown fields ignored (schema v1 read rules)
                tree.setProperty (name, incoming.getProperty (name), nullptr);
        }
    }

private:
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
        if (onLatencyInputChanged
            && (prop == iid::oversample || prop == iid::osPhase || prop == iid::offlineQuality))
            onLatencyInputChanged();
    }

    juce::ValueTree tree;
    std::atomic<int> osMirror { 0 }, phaseMirror { 0 }, offlineMirror { 0 }, lockMirror { 0 };
};
