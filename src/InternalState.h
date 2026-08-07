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
    inline const juce::Identifier uiScale        { "int_uiScale" };        // percent; legal set = ui_scale::steps
    inline const juce::Identifier tooltipsOn     { "int_tooltipsOn" };     // bool
    inline const juce::Identifier uiAnimations   { "int_uiAnimations" };   // bool
    inline const juce::Identifier spectrumOn     { "int_spectrumOn" };     // bool: graph-well mode — true spectrum, false GR history (ADR-0016)
    // ADR-0020 (0.1.1). `int_tpMeterOn` was REMOVED with the same record: the
    // stats panel shows the true peak unconditionally, so a field whose only
    // job was hiding one row had nothing left to gate. These two replace it —
    // both select which STANDARD a shown reading follows, never whether it is
    // shown, which is the distinction the removed field failed to draw.
    inline const juce::Identifier integratedStd  { "int_integratedStd" };  // 0 BS.1770-2+ (gated), 1 BS.1770-1 (ungated)
    inline const juce::Identifier rmsRef         { "int_rmsRef" };         // 0 AES-17 (FS sine = 0 dB), 1 mathematical
}

// The LEGAL VALUES of `iid::uiScale`, beside the identifier that names it
// rather than in the editor that renders it. The ladder is the field's schema —
// "what may this property hold?" is a state question, and putting the answer
// here is what lets `replaceFrom` apply the §4.4 read rule to it like every
// other field, without the editor and the state layer keeping two lists that
// can disagree (the ladder already had two representations once; round 50
// removed the second).
//
// NEAREST rather than a range clamp, because the legal set is a ladder: 110 →
// 100, 92 → 85, 50 → 75, 300 → 150 (worked against the XS..XL steps below —
// these read 90/80/200 while the seven-step ladder was current, so the examples
// described a ladder the code no longer had). Returning an INDEX as well as a value is
// what lets the editor make the rendered transform and the displayed combo
// selection ONE decision instead of two that happen to agree.
namespace ui_scale
{
    // The SIBLING'S ladder, adopted 2026-08-05 (owner directive): five steps
    // shown as XS/S/M/L/XL with M the original size — the percents are
    // Anamorph's `applyUiScale` scales ×100. The field stays a PERCENT in the
    // schema; only the legal set and the display changed. A session written by
    // the old seven-step ladder converges through `nearest` like any other
    // out-of-list value (80→75, 90→85, 175/200→150).
    inline constexpr int steps[]  = { 75, 85, 100, 125, 150 };
    inline constexpr const char* names[] = { "XS", "S", "M", "L", "XL" };
    inline constexpr int numSteps = (int) (sizeof (steps) / sizeof (steps[0]));
    static_assert (sizeof (names) / sizeof (names[0]) == (size_t) numSteps,
                   "one display name per ladder step");

    inline constexpr int nearestIndex (int pct) noexcept
    {
        int best = 0, bestDist = pct > steps[0] ? pct - steps[0] : steps[0] - pct;
        for (int i = 1; i < numSteps; ++i)
            if (const int d = pct > steps[i] ? pct - steps[i] : steps[i] - pct; d < bestDist)
            { best = i; bestDist = d; }
        return best;
    }

    inline constexpr int nearest (int pct) noexcept { return steps[nearestIndex (pct)]; }

    // The field's default, HERE rather than as a literal at each site, so
    // `setDefaults()` and every fallback read name the same number. It must be
    // a legal step, or the default would itself need normalising — which is the
    // shape the read rule exists to remove.
    inline constexpr int defaultPercent = 100;
    static_assert (nearest (defaultPercent) == defaultPercent,
                   "the default UI scale must be one of the ladder steps");
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
        tree.setProperty (iid::uiScale,        ui_scale::defaultPercent, nullptr);
        tree.setProperty (iid::tooltipsOn,     false, nullptr);
        tree.setProperty (iid::uiAnimations,   true,  nullptr);
        tree.setProperty (iid::spectrumOn,     true,  nullptr);
        // ⊕ Defaults: the GATED integrated reading, because it is what
        // BS.1770-2 onward and every delivery spec written since mean by
        // "integrated LUFS"; and the AES-17 RMS reference, because a
        // full-scale sine reading 0 dBFS is the mastering convention. Both
        // alternatives exist for the engineer who needs the other one.
        tree.setProperty (iid::integratedStd,  0,     nullptr);
        tree.setProperty (iid::rmsRef,         0,     nullptr);
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
        // NORMALISE THE LADDER FIELD HERE, which is where a value the schema
        // cannot represent enters: a hand-edited session, or one written by a
        // build whose step list has since changed. Every OTHER field's read
        // rule is already applied at adoption — the overlay above drops unknown
        // properties, and `syncAtomics` clamps the four mirrors — and
        // `iid::uiScale` was the exception: it was clamped on READ by the
        // editor and never corrected in the tree, so `getStateInformation`
        // re-serialised the illegal percent for ever.
        //
        // It used to be corrected by the editor's 24 Hz settings poll, which
        // made a display timer a writer of this tree — an opposing writer to
        // this very function, which VST3 does not promise on the message thread
        // (KI-003), on the one poll round 51 had just cleaned of ValueTree
        // access. Correcting it at adoption removes that pairing outright: the
        // editor now only READS, and by the time it can read, the value is
        // already legal.
        //
        // `setProperty` compares before assigning, so a legal percent — every
        // session this build ever wrote — writes nothing and sends no change
        // message. This is not a latency input, so the batch above is
        // indifferent to it.
        //
        // THE FALLBACK IS THE 100 % DEFAULT, NOT `var()`'s. `setDefaults()`
        // runs immediately above and always writes the field, so the read
        // cannot miss today — but "correct because of what the line above did"
        // is the reasoning this file avoids elsewhere, and here it fails
        // QUIETLY rather than loudly: a missing property reads as `var()`,
        // which converts to 0, and 0's nearest ladder step is the smallest one
        // — **75** on the XS..XL ladder this now ships (it was 80 on the
        // seven-step ladder the paragraph was written against, and the number
        // is quoted rather than derived precisely because the argument is about
        // a silent wrong answer). An absent field would silently become the
        // SMALLEST legal scale instead of the default one, at either value.
        // Naming the default in the read makes this total on its
        // own, and it is the same default `setDefaults()` writes — the value,
        // not a second opinion about it.
        tree.setProperty (iid::uiScale,
                          ui_scale::nearest ((int) tree.getProperty (
                              iid::uiScale, ui_scale::defaultPercent)),
                          nullptr);

        // …and the SAME rule for the two ADR-0020 selectors, which arrived in
        // 0.1.1 without it. They are two-valued ints with no atomic mirror to
        // clamp them (`syncAtomics` covers the four latency/lock fields only),
        // so an out-of-range stored value reached BOTH readers uncorrected —
        // and the two readers disagreed about it: `LoudnessMeterView::tick`
        // asks `== 1`, so a stored 7 read as "BS.1770-2+", while the Settings
        // combo re-seed clamps the index with `jlimit`, so the same 7 DISPLAYED
        // as "BS.1770-1". The panel then showed one standard and computed the
        // other, with nothing to make the disagreement visible. Clamping at
        // adoption — where the illegal value enters — makes both readers see
        // the same number, which is the property the `uiScale` clause above
        // exists for and the sentence beginning "Every OTHER field's read rule
        // is already applied at adoption" claimed for the whole set.
        for (const auto& id : { iid::integratedStd, iid::rmsRef })
            tree.setProperty (id, juce::jlimit (0, 1, (int) tree.getProperty (id, 0)), nullptr);

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
