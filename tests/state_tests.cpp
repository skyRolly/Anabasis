// ============================================================================
//  AnabasisStateTests — state / parameter-compatibility suite.
//
//  Compiles the REAL wrapper + GUI sources (the shared ANABASIS_PLUGIN_SOURCES
//  list), so it exercises the actual AnabasisAudioProcessor: the registry
//  snapshot (--write-snapshot gate), raw-exact round-trip, corrupt/foreign
//  robustness, and testMacroDefaultIsFixedPoint — which lives HERE because
//  only this target compiles the wrapper (ADR-0005's named-binary rule).
// ============================================================================

#include "../src/PluginProcessor.h"
#include "../src/MacroEngine.h"
#include "../src/gui/GrHistoryView.h"
#include "../src/gui/PluginEditor.h"
#include "../src/gui/CurveView.h"
#include "../src/gui/SpectrumView.h"
#include <array>
#include <cstdio>
#include <thread>

static int failures = 0;
static int checks   = 0;

static void check (bool condition, const char* what)
{
    ++checks;
    if (! condition)
    {
        ++failures;
        std::printf ("FAIL: %s\n", what);
    }
}

// ---------------------------------------------------------------------------
// One line per parameter, in layout order. Captures ID, name, range, default,
// step count and automatability — everything PARAMETER_COMPATIBILITY_POLICY
// rules 1/3 freeze (names captured too; a deliberate rename re-freezes the
// snapshot, rule 2's normal workflow).
static juce::String buildRegistryDump (AnabasisAudioProcessor& proc)
{
    juce::String out;
    for (auto* p : proc.getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr)
            continue;
        const auto& r = rp->getNormalisableRange();
        out << rp->paramID << "|" << rp->getName (64) << "|"
            << juce::String (r.start, 6) << "|" << juce::String (r.end, 6) << "|"
            << juce::String (r.convertFrom0to1 (rp->getDefaultValue()), 6) << "|"
            << rp->getNumSteps() << "|" << (rp->isAutomatable() ? "auto" : "manual") << "\n";
    }
    return out;
}

static void testRegistrySnapshot (bool writeSnapshot)
{
    AnabasisAudioProcessor proc;
    const juce::File fixture = juce::File (ANABASIS_FIXTURE_DIR)
                                   .getChildFile ("parameter_registry.snapshot");
    const auto dump = buildRegistryDump (proc);

    if (writeSnapshot)
    {
        fixture.getParentDirectory().createDirectory();
        fixture.replaceWithText (dump, false, false, "\n");   // LF: the comparison is byte-wise
        std::printf ("registry snapshot written: %s\n", fixture.getFullPathName().toRawUTF8());
        return;
    }

    check (fixture.existsAsFile(), "registry: snapshot fixture exists (run --write-snapshot once)");
    if (fixture.existsAsFile())
    {
        // Compare with line endings normalised, and ONLY those: every
        // character of every field stays byte-exact, which is the whole
        // assertion. `.gitattributes` pins the fixture to LF, but a clone that
        // predates it (or an editor that rewrites on save) still holds CRLF in
        // the working tree, and "\r" is not part of the parameter surface —
        // failing on it reports a Hard Stop that did not happen.
        const auto onDisk = fixture.loadFileAsString().replace ("\r\n", "\n");
        check (onDisk == dump,
               "registry: parameter surface matches the frozen snapshot "
               "(an ID/range/default/order/flag change is a Hard Stop)");

        // A bare FAIL here costs a whole CI round to diagnose — the failing
        // line is the diagnosis. Report the first mismatch and the line
        // counts; do not dump 49 lines nobody asked for.
        if (onDisk != dump)
        {
            juce::StringArray was, now;
            was.addLines (onDisk);
            now.addLines (dump);
            // Both strings end in "\n", so addLines leaves a trailing empty
            // entry. Drop it, or the reported count reads as one parameter
            // more than the file holds.
            for (auto* a : { &was, &now })
                if (a->size() > 0 && (*a)[a->size() - 1].isEmpty())
                    a->remove (a->size() - 1);

            for (int i = 0; i < juce::jmax (was.size(), now.size()); ++i)
                if (was[i] != now[i])
                {
                    std::printf ("      first difference at line %d of %d/%d\n"
                                 "        snapshot: %s\n"
                                 "        built   : %s\n",
                                 i + 1, was.size(), now.size(),
                                 was[i].toRawUTF8(), now[i].toRawUTF8());
                    break;
                }
        }
    }

    int count = 0, nonAuto = 0;
    for (auto* p : proc.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            ++count;
            if (! rp->isAutomatable())
                ++nonAuto;
        }
    check (count == 50,  "registry: exactly 50 parameters (DESIGN §4.2's 49 + ADR-0019's compStereoLink)");
    check (nonAuto == 9, "registry: exactly nine non-automatable (ADR-0010)");
}

// ---------------------------------------------------------------------------
static void testStateRoundTrip()
{
    AnabasisAudioProcessor proc;

    // Move a few things off default, including a mid-step raw on a discrete
    // param — the case the Raw* classes + `raw` attribute exist for.
    proc.apvts.getParameter (pid::inputGain)->setValueNotifyingHost (0.73f);
    proc.apvts.getParameter (pid::colourModel)->setValueNotifyingHost (0.807f);
    proc.internalState.state().setProperty (iid::oversample, 2, nullptr);

    juce::MemoryBlock a, b;
    proc.getStateInformation (a);

    AnabasisAudioProcessor restored;
    restored.setStateInformation (a.getData(), (int) a.getSize());
    restored.getStateInformation (b);

    check (a == b, "roundTrip: save → load → save is byte-identical (raw-exact contract)");

    const float rawBack = restored.apvts.getParameter (pid::colourModel)->getValue();
    check (std::abs (rawBack - 0.807f) < 1.0e-4f,
           "roundTrip: mid-step raw value restored exactly (pluginval contract)");
}

// ---------------------------------------------------------------------------
static void testCorruptAndForeignState()
{
    AnabasisAudioProcessor proc;
    juce::MemoryBlock before;
    proc.getStateInformation (before);

    const char garbage[] = "\x00\xff\x13garbage-not-a-state";
    proc.setStateInformation (garbage, (int) sizeof (garbage));

    juce::XmlElement foreign ("SomeOtherPlugin");
    juce::MemoryBlock foreignBlock;
    juce::AudioProcessor::copyXmlToBinary (foreign, foreignBlock);
    proc.setStateInformation (foreignBlock.getData(), (int) foreignBlock.getSize());

    juce::MemoryBlock after;
    proc.getStateInformation (after);
    check (before == after, "robustness: corrupt/foreign state is a no-op, defaults intact");
}

// ---------------------------------------------------------------------------
// MODE_AND_ADAPTATION_POLICY invariant 1's named guard: M(0,0,0) equals every
// managed parameter's declared default, or the first macro gesture jumps the
// factory patch instead of gliding from it.
static void testMacroDefaultIsFixedPoint()
{
    AnabasisAudioProcessor proc;
    auto defaultOf = [&] (const char* id)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (id));
        return p->getNormalisableRange().convertFrom0to1 (p->getDefaultValue());
    };

    check (juce::exactlyEqual (macro_curves::limGainDb (0.0f),       defaultOf (pid::limGain)),       "fixedPoint: limGain");
    check (juce::exactlyEqual (macro_curves::compThresholdDb (0.0f), defaultOf (pid::compThreshold)), "fixedPoint: compThreshold");
    check (juce::exactlyEqual (macro_curves::compRatio (0.0f),       defaultOf (pid::compRatio)),     "fixedPoint: compRatio");
    check (juce::exactlyEqual (macro_curves::clipDriveDb (0.0f),     defaultOf (pid::clipDrive)),     "fixedPoint: clipDrive");
    check (juce::exactlyEqual (macro_curves::clipShape (0.0f),       defaultOf (pid::clipShape)),     "fixedPoint: clipShape");
    check (juce::exactlyEqual (macro_curves::colourDepthPct (0, 0),  defaultOf (pid::colourDepth)),   "fixedPoint: colourDepth");
    check (juce::exactlyEqual (macro_curves::dynTiltDb (0.0f),       defaultOf (pid::dynTilt)),       "fixedPoint: dynTilt");
    check (juce::exactlyEqual (macro_curves::eqTiltDb (0.0f),        defaultOf (pid::eqTilt)),        "fixedPoint: eqTilt");
    check (juce::exactlyEqual (macro_curves::colourTone (0.0f),      defaultOf (pid::colourTone)),    "fixedPoint: colourTone");
}

// ---------------------------------------------------------------------------
static void testAbSlotsAndTiers()
{
    AnabasisAudioProcessor proc;
    proc.apvts.getParameter (pid::limGain)->setValueNotifyingHost (1.0f);   // 18 dB in slot A
    proc.apvts.getParameter (pid::bypass)->setValueNotifyingHost (1.0f);    // view tier: must NOT travel

    proc.switchToSlot (1);
    const float limInB    = proc.apvts.getRawParameterValue (pid::limGain)->load();
    const float bypassInB = proc.apvts.getRawParameterValue (pid::bypass)->load();
    check (juce::exactlyEqual (limInB, 0.0f), "ab: managed value does not leak across the switch");
    check (bypassInB >= 0.5f, "ab: view-tier bypass stays live across the switch");

    proc.switchToSlot (0);
    check (juce::exactlyEqual (proc.apvts.getRawParameterValue (pid::limGain)->load(), 18.0f),
           "ab: slot A's value survives the round trip");

    check (isPresetExcludedParam (pid::freeze) && ! isViewTierParam (pid::freeze),
           "tiers: freeze is preset-excluded only (travels in A/B and undo)");

    // ADR-0018: advancedMode left the view tier (it undoes) but is still
    // preset-excluded BY NAME, and still pinned across an A/B switch — the
    // pin moved from the shared predicate into applySlotToLive.
    check (isPresetExcludedParam (pid::advancedMode) && ! isViewTierParam (pid::advancedMode),
           "tiers: advancedMode is preset-excluded by name, no longer view-tier (ADR-0018)");
    proc.apvts.getParameter (pid::advancedMode)->setValueNotifyingHost (1.0f);
    proc.switchToSlot (1);
    check (proc.apvts.getRawParameterValue (pid::advancedMode)->load() >= 0.5f,
           "tiers: advancedMode stays live across the A/B switch (the editor never resizes on a compare)");
    proc.switchToSlot (0);
}

// ---------------------------------------------------------------------------
// §5.3: a restore is not a macro gesture. Restoring a session whose managed
// parameters sit OFF the macro curves must leave them exactly where the
// restore put them — the armed mapping is dropped, not applied. A genuine
// gesture still maps. (The unfixed code clobbered every restore as soon as
// the message loop ran; this suite flushes deterministically instead.)
static void testMacroRestoreDoesNotClobber()
{
    AnabasisAudioProcessor a;
    a.apvts.getParameter (pid::loudness)->setValueNotifyingHost (0.5f);   // gesture: L = 50
    a.getMacroEngine().flushPendingMapping();
    const float curveClip = a.apvts.getRawParameterValue (pid::clipDrive)->load();
    a.apvts.getParameter (pid::clipDrive)->setValueNotifyingHost (6.0f / 24.0f);  // manual 6 dB, off-curve
    juce::MemoryBlock state;
    a.getStateInformation (state);

    AnabasisAudioProcessor b;
    b.setStateInformation (state.getData(), (int) state.getSize());
    b.getMacroEngine().flushPendingMapping();   // what the message loop would do next
    check (juce::exactlyEqual (b.apvts.getRawParameterValue (pid::clipDrive)->load(), 6.0f),
           "macro: restore does not clobber off-curve managed values");
    check (! juce::exactlyEqual (curveClip, 6.0f),
           "macro: (test premise) 6 dB really is off the L=50 curve");

    b.apvts.getParameter (pid::loudness)->setValueNotifyingHost (0.5f + 1.0f / 100.0f);
    b.getMacroEngine().flushPendingMapping();
    check (! juce::exactlyEqual (b.apvts.getRawParameterValue (pid::clipDrive)->load(), 6.0f),
           "macro: a genuine gesture still re-maps the managed set");
}

// ---------------------------------------------------------------------------
// The stronger half of §5.3: dropping the armed mapping at the END of a
// restore only holds while the restore out-races the 30 ms drain timer, which
// a host restoring off the message thread does not promise. A drain that fires
// INSIDE the restore must be a no-op — that is what MacroEngine::ScopedRestore
// exists for, and a flush inside the scope is exactly the timer's behaviour
// there, without needing a second thread to reproduce it.
static void testDrainInsideRestoreIsSuppressed()
{
    AnabasisAudioProcessor proc;
    proc.apvts.getParameter (pid::loudness)->setValueNotifyingHost (0.5f);
    proc.getMacroEngine().flushPendingMapping();
    proc.apvts.getParameter (pid::clipDrive)->setValueNotifyingHost (6.0f / 24.0f);   // off-curve

    {
        const MacroEngine::ScopedRestore guard (proc.getMacroEngine());

        // Stand in for the restore's own notifications arming the mapping…
        proc.apvts.getParameter (pid::loudness)->setValueNotifyingHost (0.9f);
        // …and for the drain timer firing before the restore has finished.
        proc.getMacroEngine().flushPendingMapping();

        check (juce::exactlyEqual (proc.apvts.getRawParameterValue (pid::clipDrive)->load(), 6.0f),
               "macro: a drain inside a restore scope does not rewrite the managed set");
    }

    // Leaving the scope drops the armed mapping rather than deferring it: the
    // restore's values stand until the next real gesture.
    proc.getMacroEngine().flushPendingMapping();
    check (juce::exactlyEqual (proc.apvts.getRawParameterValue (pid::clipDrive)->load(), 6.0f),
           "macro: the scope drops the armed mapping on exit, it does not queue it");
}

// The OTHER half of the same tick. The restore guard used to sit inside
// `drainPendingMapping`, so a tick landing in a restore still ran the WRAPPER's
// drain — which writes `liveDetachMask`, the plain `juce::StringArray` the
// restore is itself replacing. On the message thread that is only wasted work;
// on the off-message-thread `setStateInformation` VST3 permits it made the tick
// a second concurrent writer of that array, widening KI-003's window instead of
// leaving it as found. The guard now covers the whole tick, and the outcome is
// unchanged either way — `replaceDetachMask` drops the staged bits — which is
// exactly why only the mask DURING the restore can tell the two apart.
static void testTheWholeTickIsSuppressedInsideARestore()
{
    AnabasisAudioProcessor proc;
    auto* limGain = proc.apvts.getParameter (pid::limGain);

    // A gestured managed edit delivered OFF the message thread: the bit is
    // staged and `drainDetachBitsSoon` refuses to post, so only a tick can
    // turn it into mask text.
    std::thread offThread ([limGain]
    {
        limGain->beginChangeGesture();
        limGain->setValueNotifyingHost (limGain->getNormalisableRange().convertTo0to1 (2.0f));
        limGain->endChangeGesture();
    });
    offThread.join();
    check (proc.detachMask().isEmpty(), "restoreTick: (premise) the bit is staged, not in the mask");

    {
        const MacroEngine::ScopedRestore guard (proc.getMacroEngine());
        proc.getMacroEngine().drainTick();
        check (proc.detachMask().isEmpty(),
               "restoreTick: a tick inside a restore does not write the detach mask either");
    }

    // Deferred, not dropped: outside the scope the same tick applies it. A
    // real restore path clears it at `replaceDetachMask` instead, which is the
    // step this test deliberately does not perform.
    proc.getMacroEngine().drainTick();
    check (proc.detachMask().contains (pid::limGain),
           "restoreTick: …and the tick after the restore still applies it");
}

// ---------------------------------------------------------------------------
// ADR-0007: every slot-copy path is raw-exact. A mid-step discrete value and
// a log-taper float must survive A -> B -> A bit-exactly.
static void testAbRawExact()
{
    AnabasisAudioProcessor proc;
    proc.apvts.getParameter (pid::colourModel)->setValueNotifyingHost (0.807f);
    proc.apvts.getParameter (pid::scHpfFreq)->setValueNotifyingHost (0.377f);
    proc.switchToSlot (1);
    proc.switchToSlot (0);
    check (std::abs (proc.apvts.getParameter (pid::colourModel)->getValue() - 0.807f) < 1.0e-6f,
           "abRaw: mid-step discrete value survives A->B->A exactly");
    check (std::abs (proc.apvts.getParameter (pid::scHpfFreq)->getValue() - 0.377f) < 1.0e-6f,
           "abRaw: log-taper float survives A->B->A without pow/log drift");
}

// ---------------------------------------------------------------------------
// ADR-0007's named obligation: the round-trip fixture must include a frozen
// slot and a non-clear detach mask, or frozenTrims/detachMask are exercised
// vacuously. Built by editing a real session tree, then checked byte-wise.
static void testFrozenSlotRoundTrip()
{
    AnabasisAudioProcessor a;
    juce::MemoryBlock blank;
    a.getStateInformation (blank);
    auto root = juce::ValueTree::fromXml (*juce::AudioProcessor::getXmlFromBinary (blank.getData(), (int) blank.getSize()));

    auto ab   = root.getChildWithName ("AB");
    auto slot = ab.getChild (0);
    slot.setProperty ("presetName", "Frozen A", nullptr);
    // FREEZE ON in the slot's own parameter tree, because that is what a
    // frozen slot IS: §5.4/MODE invariant 3 give a freeze-OFF slot nothing to
    // latch, and since round 38 `saveSlotFromLive` emits no `FROZEN_TRIMS`
    // child for one. The fixture used to leave `freeze` at its default and
    // still expect the child back — a state the product cannot produce, so the
    // round trip it pinned was of an inconsistent tree rather than of ADR-0014.
    // BOTH surfaces, because they are different trees: the ACTIVE slot's live
    // values come from the root-level `ANABASIS` child (`setStateInformation`
    // adopts that one), while the `AB` child carries the per-slot copies. A
    // fixture that set only the latter loaded with Freeze OFF.
    auto setFreezeOn = [] (juce::ValueTree params)
    {
        if (auto fz = params.getChildWithProperty ("id", "freeze"); fz.isValid())
        {
            fz.setProperty ("value", 1.0, nullptr);
            fz.setProperty ("raw",   1.0, nullptr);
        }
    };
    setFreezeOn (root.getChildWithName ("ANABASIS"));
    setFreezeOn (slot.getChildWithName ("ANABASIS"));
    juce::ValueTree trims ("FROZEN_TRIMS");
    trims.setProperty ("releaseOctaves", 0.25, nullptr);   // the ADR-0014 field names
    trims.setProperty ("stereoLink",     -0.1, nullptr);
    trims.setProperty ("scHpfHz",        3.0,  nullptr);
    trims.setProperty ("dynTiltDb",      0.4,  nullptr);
    slot.appendChild (trims, nullptr);
    auto mask = slot.getChildWithName ("DETACH_MASK");
    juce::ValueTree m ("PARAM");
    m.setProperty ("id", "clipDrive", nullptr);
    mask.appendChild (m, nullptr);

    juce::MemoryBlock in;
    juce::AudioProcessor::copyXmlToBinary (*root.createXml(), in);

    AnabasisAudioProcessor b;
    b.setStateInformation (in.getData(), (int) in.getSize());
    juce::MemoryBlock out1, out2;
    b.getStateInformation (out1);
    AnabasisAudioProcessor c;
    c.setStateInformation (out1.getData(), (int) out1.getSize());
    c.getStateInformation (out2);
    check (out1 == out2, "frozenSlot: save -> load -> save is byte-identical WITH trims + mask");

    const auto r2 = juce::ValueTree::fromXml (*juce::AudioProcessor::getXmlFromBinary (out1.getData(), (int) out1.getSize()));
    const auto s2 = r2.getChildWithName ("AB").getChild (0);
    check (s2.getChildWithName ("FROZEN_TRIMS").isValid()
             && juce::exactlyEqual ((double) s2.getChildWithName ("FROZEN_TRIMS").getProperty ("releaseOctaves"), 0.25),
           "frozenSlot: the frozen trim vector survives the round trip");
    check (s2.getChildWithName ("DETACH_MASK").getNumChildren() == 1,
           "frozenSlot: the non-clear detach mask survives the round trip");

    // Per-slot travel: switching away and back must carry both fields.
    b.switchToSlot (1);
    b.switchToSlot (0);
    juce::MemoryBlock out3;
    b.getStateInformation (out3);
    const auto r3 = juce::ValueTree::fromXml (*juce::AudioProcessor::getXmlFromBinary (out3.getData(), (int) out3.getSize()));
    const auto s3 = r3.getChildWithName ("AB").getChild (0);
    check (s3.getChildWithName ("FROZEN_TRIMS").isValid() && s3.getChildWithName ("DETACH_MASK").getNumChildren() == 1,
           "frozenSlot: trims + mask travel per-slot through the A/B swap paths");

    // The other half of the same rule: a slot the user UN-freezes stops
    // serialising a latch it no longer holds. Before this, `frozen` started
    // from the mirror unconditionally, so the vector was written into every
    // later save — and, being a child of the tree `presetDirty()` compares,
    // kept flipping the edited mark for a change no preset could carry.
    b.apvts.getParameter (pid::freeze)->setValueNotifyingHost (0.0f);
    juce::MemoryBlock out4;
    b.getStateInformation (out4);
    const auto r4 = juce::ValueTree::fromXml (*juce::AudioProcessor::getXmlFromBinary (out4.getData(), (int) out4.getSize()));
    check (! r4.getChildWithName ("AB").getChild (0).getChildWithName ("FROZEN_TRIMS").isValid(),
           "frozenSlot: a freeze-OFF slot serialises no frozen vector at all");
}

// ---------------------------------------------------------------------------
// ADR-0014 (OQ-013): FROZEN_TRIMS is a RESTORED vector, not just a carried
// child. The save captures the published latch; the load stages it on
// ADR-0012's record row; the engine applies it where every restore-driven
// discontinuity lands — the §2.8 duck's silent bottom (primed) or the
// unprimed direct-adopt — and Freeze then holds it bit-exactly. Freeze OFF on
// the adopted surface stages nothing.
//
// Stimulus calibration: the release-trim target is (refOnset − onset) × 0.15
// against the 4.0 factory reference, deadband 0.05 — so the latch pass runs
// 10 clicks/s (target ≈ −0.9, far off zero) and the A/B away-pass runs a
// click-FREE tone (target ≈ +0.6, far off the latch, so the un-frozen vector
// genuinely moves; keeping the clicks would leave it inside the deadband and
// the "moved" premise vacuous).
static void testFrozenTrimRestore()
{
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, 512);

    // clickPeriod in samples; 0 = pure tone.
    auto feed = [&] (AnabasisAudioProcessor& p, int blocks, int t0, int clickPeriod)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                const int t = t0 + b * 512 + n;
                float v = 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi
                                           * 220.0f * (float) t / 48000.0f);
                if (clickPeriod > 0 && (t % clickPeriod) < 96) v += 0.6f;
                buf.setSample (0, n, v);
                buf.setSample (1, n, v);
            }
            p.processBlock (buf, midi);
        }
    };
    auto set = [] (AnabasisAudioProcessor& p, const char* id, float denorm)
    {
        auto* par = p.apvts.getParameter (id);
        par->setValueNotifyingHost (par->getNormalisableRange().convertTo0to1 (denorm));
    };
    auto trimsOf = [] (const AnabasisAudioProcessor& p)
    {
        const auto& ad = p.adaptiveReadout();
        return std::array<float, 4> { ad.publishedTrimRelease(), ad.publishedTrimLink(),
                                      ad.publishedTrimHpf(),     ad.publishedTrimTilt() };
    };
    auto sameVector = [] (const std::array<float, 4>& x, const std::array<float, 4>& y)
    {
        return juce::exactlyEqual (x[0], y[0]) && juce::exactlyEqual (x[1], y[1])
            && juce::exactlyEqual (x[2], y[2]) && juce::exactlyEqual (x[3], y[3]);
    };
    auto parse = [] (const juce::MemoryBlock& mb)
    {
        return juce::ValueTree::fromXml (*juce::AudioProcessor::getXmlFromBinary (
                                             mb.getData(), (int) mb.getSize()));
    };

    // Latch a genuinely moved vector and save it.
    AnabasisAudioProcessor a;
    a.prepareToPlay (48000.0, 512);
    set (a, pid::limGain, 6.0f);                 // limiting engaged, like the DSP suite
    feed (a, 600, 0, 4800);                      // ~6.4 s at 10 clicks/s
    set (a, pid::freeze, 1.0f);
    feed (a, 4, 600 * 512, 4800);                // the latch lands at a block top
    const auto saved = trimsOf (a);
    check (std::abs (saved[0]) > 0.2f,
           "frozenRestore: (premise) the latched release trim is well off zero");

    juce::MemoryBlock state;
    a.getStateInformation (state);

    // The state carries the captured latch (a dropped saveSlotFromLive capture
    // dies here, not three checks later).
    {
        const auto ft = parse (state).getChildWithName ("AB").getChildWithName ("SLOT")
                            .getChildWithName ("FROZEN_TRIMS");
        check (ft.isValid()
                 && std::abs ((float) (double) ft.getProperty ("releaseOctaves") - saved[0]) < 1.0e-6f,
               "frozenRestore: the save captured the published latch into FROZEN_TRIMS");
    }

    // (1) Session load into an UNPRIMED engine: the vector lands on the first
    //     block (direct-adopt), and Freeze holds it exactly.
    {
        AnabasisAudioProcessor fresh;
        fresh.prepareToPlay (48000.0, 512);
        fresh.setStateInformation (state.getData(), (int) state.getSize());
        // The RETAINED generation before the vector lands. `setStateInformation`
        // only stages, and `adoptFrozenMirror` has just re-based
        // `slotFrozenBase` to this value — so nothing the engine holds belongs
        // to the incoming slot yet.
        const auto genBefore = fresh.adaptiveReadout().retainedTrimGeneration();
        check (! fresh.adaptiveReadout().hasRetainedTrims(),
               "frozenRestore: (premise) a freshly loaded instance has latched nothing yet");
        buf.clear();
        fresh.processBlock (buf, midi);
        check (sameVector (trimsOf (fresh), saved),
               "frozenRestore: an unprimed session load restores the vector on the first block");

        // …AND THE RESTORE COUNTS AS A LATCH. `injectTrims` publishes with
        // `meaningful = true`, so it writes the retained set and advances the
        // generation exactly as an audible `finishBlock` does — and with Freeze
        // ON nothing else publishes, so this injection is the ONLY thing that
        // can move it. That bump is what carries the generation past the base
        // `adoptFrozenMirror` just set, which is how `engineFrozenTrimsIfLive()`
        // learns the incoming slot may now answer for itself. Publish without
        // counting and the slot withholds its latch from every save until the
        // next AUDIBLE block — never, on a stopped transport — so it would go on
        // re-serialising the mirror instead of what the engine is applying.
        // Silent, and only reachable with the transport stopped, which is why it
        // is asserted here rather than left to the value checks: they cannot see
        // it, because both answers are the same vector.
        check (fresh.adaptiveReadout().retainedTrimGeneration() != genBefore,
               "frozenRestore: an ADR-0014 restore advances the retained generation");
        check (fresh.adaptiveReadout().hasRetainedTrims(),
               "frozenRestore: …so the restored slot owns a latch it can save on a stopped transport");
    }

    // (2) Session load into a PRIMED engine: the vector lands at the duck's
    //     silent bottom and holds exactly from there.
    {
        AnabasisAudioProcessor primed;
        primed.prepareToPlay (48000.0, 512);
        feed (primed, 12, 0, 4800);
        check (! sameVector (trimsOf (primed), saved),
               "frozenRestore: (premise) the primed processor's own vector differs");
        primed.setStateInformation (state.getData(), (int) state.getSize());
        feed (primed, 30, 12 * 512, 4800);       // through the duck and out
        check (sameVector (trimsOf (primed), saved),
               "frozenRestore: a primed session load lands the vector at the duck bottom");
    }

    // (3) A/B: the away-pass (freeze off, click-free tone) moves the vector;
    //     switching back restages the slot's memory through applySlotToLive.
    {
        a.switchToSlot (1);                      // slot B: defaults, freeze off
        feed (a, 120, 604 * 512, 0);
        check (! sameVector (trimsOf (a), saved),
               "frozenRestore: (premise) the un-frozen away-pass moved the vector");
        a.switchToSlot (0);
        feed (a, 30, 724 * 512, 0);
        check (sameVector (trimsOf (a), saved),
               "frozenRestore: switching back to the frozen slot restores its vector");
    }

    // (4) Load → save with NO audio between: the loaded vector is the truth,
    //     not the engine's stale published trims (the same mirror rule the
    //     ADAPTIVE child follows; kills a dropped frozenRestorePending guard).
    {
        AnabasisAudioProcessor noAudio;
        noAudio.prepareToPlay (48000.0, 512);
        noAudio.setStateInformation (state.getData(), (int) state.getSize());
        juce::MemoryBlock resaved;
        noAudio.getStateInformation (resaved);   // deliberately no processBlock
        const auto ft = parse (resaved).getChildWithName ("AB").getChildWithName ("SLOT")
                            .getChildWithName ("FROZEN_TRIMS");
        check (ft.isValid()
                 && std::abs ((float) (double) ft.getProperty ("releaseOctaves") - saved[0]) < 1.0e-6f,
               "frozenRestore: a no-audio load then save keeps the loaded vector, not the stale latch");
    }

    // (5a) UNDO/REDO stage the vector too — and an undo is a bulk swap, so it
    //      must request the duck that is the vector's only landing site. An
    //      undo whose step moves no discrete stage never reaches a bottom by
    //      itself: the vector then sat pending indefinitely and was injected
    //      at the next unrelated duck, into whatever slot was live by then.
    //      Vector V is latched into an undo step, the trims are then moved to
    //      W with Freeze off and re-latched, and the undo must bring V back.
    {
        auto* gain = a.apvts.getParameter (pid::limGain);
        gain->beginChangeGesture();          // the step's pre-state carries V
        gain->setValueNotifyingHost (gain->getNormalisableRange().convertTo0to1 (3.0f));
        gain->endChangeGesture();
        check (a.canUndo(), "frozenRestore/undo: (premise) the drag pushed a step");

        set (a, pid::freeze, 0.0f);           // let the vector move to W…
        feed (a, 200, 800 * 512, 0);
        set (a, pid::freeze, 1.0f);           // …and latch it there
        feed (a, 4, 1000 * 512, 0);
        const auto latchedW = trimsOf (a);
        check (! sameVector (latchedW, saved),
               "frozenRestore/undo: (premise) the vector really moved before the undo");

        a.undo();
        feed (a, 30, 1004 * 512, 0);          // through the duck the undo owes
        check (sameVector (trimsOf (a), saved),
               "frozenRestore/undo: an undo restores the step's frozen vector (it ducks for it)");
    }

    // (5b) The MIRROR WINDOW. `frozenPending` is cleared by the block-top
    //      consume, but the vector is only published at the duck bottom a
    //      block or more later. A save landing in between read the engine's
    //      PRE-restore trims — and, because the capture also rewrote the
    //      mirror, permanently replaced the loaded vector with them. The
    //      editor polls saveSlotFromLive() through presetDirty() at ~3 Hz, so
    //      this window is reached in ordinary use, not only under a test.
    {
        AnabasisAudioProcessor mid;
        mid.prepareToPlay (48000.0, 512);
        feed (mid, 12, 0, 4800);                       // primed, own vector ≠ saved
        mid.setStateInformation (state.getData(), (int) state.getSize());
        feed (mid, 1, 12 * 512, 4800);                 // consumed at the block top…
        check (! sameVector (trimsOf (mid), saved),
               "frozenRestore/window: (premise) one block in, the vector has not landed yet");

        juce::MemoryBlock inWindow;
        mid.getStateInformation (inWindow);            // …and a save lands here
        const auto ft = parse (inWindow).getChildWithName ("AB").getChildWithName ("SLOT")
                            .getChildWithName ("FROZEN_TRIMS");
        check (ft.isValid()
                 && std::abs ((float) (double) ft.getProperty ("releaseOctaves") - saved[0]) < 1.0e-6f,
               "frozenRestore/window: a save between the consume and the duck bottom keeps the loaded vector");

        feed (mid, 30, 13 * 512, 4800);
        check (sameVector (trimsOf (mid), saved),
               "frozenRestore/window: and the restore still lands afterwards");
    }

    // (5) The SAME session with the freeze parameter flipped off must stage
    //     nothing: after one block the trims are still near zero, nowhere
    //     near the saved latch a wrongly-staged inject would have published.
    {
        auto root = parse (state);
        auto params = root.getChildWithName ("ANABASIS");
        for (int i = 0; i < params.getNumChildren(); ++i)
        {
            auto node = params.getChild (i);
            if (node.hasType ("PARAM") && node.getProperty ("id").toString() == pid::freeze)
            {
                node.setProperty ("value", 0.0, nullptr);
                if (node.hasProperty ("raw"))
                    node.setProperty ("raw", 0.0, nullptr);
            }
        }
        juce::MemoryBlock off;
        juce::AudioProcessor::copyXmlToBinary (*root.createXml(), off);

        AnabasisAudioProcessor unfrozen;
        unfrozen.prepareToPlay (48000.0, 512);
        unfrozen.setStateInformation (off.getData(), (int) off.getSize());
        buf.clear();
        unfrozen.processBlock (buf, midi);
        check (std::abs (trimsOf (unfrozen)[0]) < 0.5f * std::abs (saved[0]),
               "frozenRestore: freeze-off on the adopted surface stages no injection");
    }
}

// ---------------------------------------------------------------------------
// §4.4 read rules, the two AB shapes the tolerance rules admit: a root with
// NO AB child resets every slot field to defaults (no chimera of two
// sessions), and an unknown child INSIDE AB must not shift the slots.
static void testAbToleranceRules()
{
    AnabasisAudioProcessor proc;
    // Load the frozen-slot session first so the slot fields are non-default.
    juce::MemoryBlock blank;
    proc.getStateInformation (blank);
    auto root = juce::ValueTree::fromXml (*juce::AudioProcessor::getXmlFromBinary (blank.getData(), (int) blank.getSize()));
    auto slot0 = root.getChildWithName ("AB").getChild (0);
    slot0.setProperty ("presetName", "Session X", nullptr);
    slot0.appendChild (juce::ValueTree ("FROZEN_TRIMS"), nullptr);
    juce::MemoryBlock withTrims;
    juce::AudioProcessor::copyXmlToBinary (*root.createXml(), withTrims);
    proc.setStateInformation (withTrims.getData(), (int) withTrims.getSize());

    // Now a valid root WITHOUT an AB child: slot fields must go to defaults.
    juce::ValueTree minimal ("AnabasisRoot");
    minimal.setProperty ("schemaVersion", 1, nullptr);
    juce::MemoryBlock minBlock;
    juce::AudioProcessor::copyXmlToBinary (*minimal.createXml(), minBlock);
    proc.setStateInformation (minBlock.getData(), (int) minBlock.getSize());
    juce::MemoryBlock after;
    proc.getStateInformation (after);
    const auto rAfter = juce::ValueTree::fromXml (*juce::AudioProcessor::getXmlFromBinary (after.getData(), (int) after.getSize()));
    const auto sAfter = rAfter.getChildWithName ("AB").getChild (0);
    check (! sAfter.getChildWithName ("FROZEN_TRIMS").isValid()
             && sAfter.getProperty ("presetName").toString() == "Default",
           "tolerance: a root without AB resets the slot fields to defaults "
           "(and the presetName field's default IS the Default preset's name)");

    // Unknown child inside AB must not shift the SLOT children.
    AnabasisAudioProcessor d;
    d.apvts.getParameter (pid::limGain)->setValueNotifyingHost (0.5f);   // 9 dB in slot A
    juce::MemoryBlock shifted;
    d.getStateInformation (shifted);
    auto rs = juce::ValueTree::fromXml (*juce::AudioProcessor::getXmlFromBinary (shifted.getData(), (int) shifted.getSize()));
    auto abT = rs.getChildWithName ("AB");
    abT.addChild (juce::ValueTree ("AB_META"), 0, nullptr);   // tolerated foreign child, FIRST
    juce::MemoryBlock shiftedIn;
    juce::AudioProcessor::copyXmlToBinary (*rs.createXml(), shiftedIn);
    AnabasisAudioProcessor e;
    e.setStateInformation (shiftedIn.getData(), (int) shiftedIn.getSize());
    check (juce::exactlyEqual (e.apvts.getRawParameterValue (pid::limGain)->load(), 9.0f),
           "tolerance: an unknown first child inside AB does not shift the slots");
}

// ---------------------------------------------------------------------------
// Preset contract (ADR-0007): snapped values in the file, and a locked
// ceiling is SKIPPED on apply — never written and reverted.
static void testPresetContract()
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("anabasis-test-presets");
    dir.createDirectory();
    const auto file = dir.getChildFile ("t.anabasis");

    AnabasisAudioProcessor a;
    a.apvts.getParameter (pid::colourModel)->setValueNotifyingHost (0.807f);  // mid-step raw
    a.apvts.getParameter (pid::ceiling)->setValueNotifyingHost (0.7f);        // -6 dBTP
    check (a.getPresetManager().savePreset (file, {}), "preset: save succeeds");

    const auto xml = juce::XmlDocument::parse (file);
    bool snapped = false;
    for (auto* p : xml->getChildWithTagNameIterator ("PARAM"))
        if (p->getStringAttribute ("id") == pid::colourModel)
            snapped = juce::exactlyEqual ((float) p->getDoubleAttribute ("value"), 2.0f);
    check (snapped, "preset: discrete values are written SNAPPED, not mid-step");

    AnabasisAudioProcessor b;
    const float lockedNorm = b.apvts.getParameter (pid::ceiling)->getValue();  // default -1 dBTP
    b.internalState.state().setProperty (iid::ceilingLock, true, nullptr);
    check (b.applyPresetFile (file), "preset: apply succeeds");
    check (juce::exactlyEqual (b.apvts.getParameter (pid::ceiling)->getValue(), lockedNorm),
           "preset: a locked ceiling is never moved by a preset apply");
    check (! juce::exactlyEqual (b.apvts.getRawParameterValue (pid::colourModel)->load(), 1.0f),
           "preset: unlocked parameters do land from the preset");

    // The mirror of the factory-preset rule, and the reason the restore guard
    // stays on THIS path: a file carries every parameter, MANAGED ones
    // included, so its values are authoritative and the macro mapping must not
    // re-derive them from the stored macro position. Saved here with a limiter
    // gain deliberately off the §5.5 curve for the macro the file also carries.
    {
        AnabasisAudioProcessor c;
        auto* gain = c.apvts.getParameter (pid::limGain);
        auto* loud = c.apvts.getParameter (pid::loudness);
        loud->setValueNotifyingHost (loud->getNormalisableRange().convertTo0to1 (60.0f));
        c.getMacroEngine().flushPendingMapping();          // let the curve land first…
        const float offCurve = macro_curves::limGainDb (0.6f) - 5.0f;   // …then leave it
        gain->setValueNotifyingHost (gain->getNormalisableRange().convertTo0to1 (offCurve));
        const auto handEdited = dir.getChildFile ("hand-edited.anabasis");
        check (c.getPresetManager().savePreset (handEdited, {}), "preset: (premise) save succeeds");

        AnabasisAudioProcessor d;
        check (d.applyPresetFile (handEdited), "preset: (premise) the hand-edited file applies");
        check (std::abs (d.apvts.getRawParameterValue (pid::limGain)->load() - offCurve) < 0.05f,
               "preset: a FILE preset's managed values survive — the mapping does not re-derive them");
        handEdited.deleteFile();
    }
    file.deleteFile();
}

// ---------------------------------------------------------------------------
// §4.4 structural tolerance: a VALID root that OMITS a child means "that
// child's fields are at their defaults", not "keep whatever the previous
// session left live". Both children are checked against an already-dirtied
// processor, so the old early-return behaviour (leave the live values alone)
// fails here rather than silently producing a chimera of two sessions.
static void testMissingChildrenReadAsDefaults()
{
    auto rootOfPristineSession = []
    {
        AnabasisAudioProcessor pristine;
        juce::MemoryBlock mb;
        pristine.getStateInformation (mb);
        const auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
        return xml != nullptr ? juce::ValueTree::fromXml (*xml) : juce::ValueTree();
    };
    auto dirty = [] (AnabasisAudioProcessor& p)
    {
        p.apvts.getParameter (pid::inputGain)->setValueNotifyingHost (0.9f);
        p.internalState.state().setProperty (iid::oversample,  3,    nullptr);
        p.internalState.state().setProperty (iid::ceilingLock, true, nullptr);
    };
    auto apply = [] (AnabasisAudioProcessor& p, const juce::ValueTree& root)
    {
        juce::MemoryBlock mb;
        if (const auto xml = root.createXml())
            juce::AudioProcessor::copyXmlToBinary (*xml, mb);
        p.setStateInformation (mb.getData(), (int) mb.getSize());
    };

    // The reference is a pristine processor's LIVE value, not getDefaultValue():
    // the default slot restores through the raw path, so this is the byte-exact
    // value the read rule is required to land on.
    const float defaultInputGain = AnabasisAudioProcessor().apvts
                                       .getParameter (pid::inputGain)->getValue();

    {   // ANABASIS_INTERNAL absent → the int_ fields reset, parameters honoured
        auto root = rootOfPristineSession();
        root.removeChild (root.getChildWithName ("ANABASIS_INTERNAL"), nullptr);

        AnabasisAudioProcessor p;
        dirty (p);
        apply (p, root);
        check (p.internalState.oversampleFactor() == anabasis::OversampleFactor::off
                && ! p.internalState.ceilingLocked(),
               "readRules: absent ANABASIS_INTERNAL resets the int_ fields to defaults");
    }

    {   // ANABASIS absent → the parameters reset, int_ fields honoured
        auto root = rootOfPristineSession();
        root.removeChild (root.getChildWithName ("ANABASIS"), nullptr);
        root.getChildWithName ("ANABASIS_INTERNAL").setProperty (iid::oversample, 2, nullptr);

        AnabasisAudioProcessor p;
        dirty (p);
        apply (p, root);
        check (juce::exactlyEqual (p.apvts.getParameter (pid::inputGain)->getValue(),
                                   defaultInputGain),
               "readRules: absent ANABASIS resets the parameter surface to defaults");
        check (p.internalState.oversampleFactor() == anabasis::OversampleFactor::x4,
               "readRules: absent ANABASIS does not discard the internal child that IS present");
    }

    {   // A single PARAM child absent → THAT parameter resets, the rest restore.
        // This is NOT implemented by our code — it is pinned JUCE behaviour,
        // and the pin is why the test exists. replaceState's reconnection
        // (updateParameterConnectionsToChildTrees) appends an id-only child
        // for the missing parameter; the APVTS hears its own appendChild via
        // valueTreeChildAdded → setNewState, whose value-property fallback is
        // getDenormalisedDefaultValue() — so the parameter lands on its
        // declared default BEFORE flushParameterValuesToValueTree writes that
        // default back into the child. A review claimed the flush seeds the
        // child from the previous session's CURRENT value (the chimera one
        // level below the whole-child cases above); the claim reads plausibly
        // from the flush alone but is falsified by the childAdded round trip
        // — and by this test, which was written to fail before any fix and
        // passed on the unmodified code. It stays as the tripwire that fires
        // if a JUCE upgrade ever changes the reconnection semantics.
        auto root = rootOfPristineSession();
        auto params = root.getChildWithName ("ANABASIS");
        auto gainNode = params.getChildWithProperty ("id", juce::String (pid::inputGain));
        check (gainNode.isValid(), "readRules: (premise) the session tree carries the inputGain child");
        params.removeChild (gainNode, nullptr);

        AnabasisAudioProcessor p;
        dirty (p);
        check (! juce::exactlyEqual (p.apvts.getParameter (pid::inputGain)->getValue(),
                                     defaultInputGain),
               "readRules: (premise) the dirtied value is off-default, so a keep-live bug is visible");
        apply (p, root);
        check (juce::exactlyEqual (p.apvts.getParameter (pid::inputGain)->getValue(),
                                   defaultInputGain),
               "readRules: an absent PARAM child resets that parameter, not keeps the live value");

        // …and the reset value must round-trip: the next save carries the
        // default, not the leftover.
        juce::MemoryBlock saved;
        p.getStateInformation (saved);
        AnabasisAudioProcessor q;
        q.setStateInformation (saved.getData(), (int) saved.getSize());
        check (juce::exactlyEqual (q.apvts.getParameter (pid::inputGain)->getValue(),
                                   defaultInputGain),
               "readRules: the filled-in default survives the next save/load");
    }
}

// ---------------------------------------------------------------------------
// ADR-0004 item 5: a session load re-reports PDC ONCE. `replaceFrom` writes
// every property twice over (defaults, then the overlay), three of them latency
// inputs, so an unbatched notification walks the reported figure through the
// DEFAULT (Off) value up to six times per load. That is invisible while
// osLatencySamples() returns 0 and becomes a burst of host PDC changes
// mid-load the moment oversampling lands — the thing the constant allowance
// exists to prevent.
static void testLatencyNotifyIsBatchedAcrossARead()
{
    InternalState s;
    int fires = 0;
    s.onLatencyInputChanged = [&fires] { ++fires; };

    // A single interactive write still reports immediately — the batch must
    // not turn every latency change into a deferred one.
    s.state().setProperty (iid::oversample, 2, nullptr);
    check (fires == 1, "latency: a single int_ write still fires one recompute");

    juce::ValueTree incoming ("ANABASIS_INTERNAL");
    incoming.setProperty (iid::oversample,     3, nullptr);
    incoming.setProperty (iid::osPhase,        1, nullptr);
    incoming.setProperty (iid::offlineQuality, 1, nullptr);

    fires = 0;
    s.replaceFrom (incoming);
    check (fires == 1, "latency: a whole session read fires ONE recompute, not one per property");
    check (s.oversampleFactor() == anabasis::OversampleFactor::x8
            && s.osPhaseMode() == anabasis::OsPhaseMode::linear
            && s.forceMaxOffline(),
           "latency: the batched read still lands every value");
}

// ---------------------------------------------------------------------------
// The raw-exact contract needs the normalised round trip to be a FIXED POINT,
// not merely accurate: save writes raw = getValue(), load feeds it back, and
// the next save writes getValue() again. Byte-identity therefore requires
// h(h(x)) == h(x) exactly, where h is one setValue → getValue pass. A taper
// change that breaks this shows up here — naming the parameter — instead of
// intermittently reddening testStateRoundTrip on whichever value it was set
// to. (Note this is NOT h(x) == x: the log tapers are knowingly off by ulps,
// see the frozen-snapshot note in docs/procedures/TESTING.md.)
static void testRawRoundTripIsIdempotent()
{
    AnabasisAudioProcessor proc;
    juce::String drifting;

    for (auto* p : proc.getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr)
            continue;

        for (const float probe : { 0.0f, 0.017f, 0.25f, 1.0f / 3.0f, 0.5f, 0.63f, 0.9f, 1.0f })
        {
            rp->setValueNotifyingHost (probe);
            const float once = rp->getValue();
            rp->setValueNotifyingHost (once);
            const float twice = rp->getValue();

            if (! juce::exactlyEqual (once, twice))
                drifting << " " << rp->paramID << "@" << juce::String (probe, 3);
        }
    }

    check (drifting.isEmpty(),
           "rawRoundTrip: one save→load→save pass is a fixed point for every parameter");
    if (drifting.isNotEmpty())
        std::printf ("      drifting:%s\n", drifting.toRawUTF8());
}

// ---------------------------------------------------------------------------
// §2.8 wiring, wrapper side: switchToSlot requests the forced duck BEFORE the
// swap, so a mid-stream A/B compare dips through the silent bottom instead of
// stepping. (The duck mechanism itself is pinned in the DSP suite; THIS test
// pins that the wrapper actually asks for it — remove the requestForcedDuck()
// call and the dip vanishes.)
static void testAbSwitchRequestsDuck()
{
    AnabasisAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, 512);

    auto runBlock = [&] (int blockIdx, std::vector<float>& out)
    {
        for (int n = 0; n < 512; ++n)
        {
            const float v = 0.4f * std::sin (2.0f * juce::MathConstants<float>::pi
                                             * 300.0f * (float) (blockIdx * 512 + n) / 48000.0f);
            buf.setSample (0, n, v);
            buf.setSample (1, n, v);
        }
        proc.processBlock (buf, midi);
        for (int n = 0; n < 512; ++n)
            out.push_back (buf.getSample (0, n));
    };

    std::vector<float> out;
    for (int b = 0; b < 10; ++b) runBlock (b, out);
    proc.switchToSlot (1);                       // must request the duck first
    for (int b = 10; b < 30; ++b) runBlock (b, out);

    float minEnv = 1.0f, tailPeak = 0.0f;
    for (size_t n = 10 * 512; n < 10 * 512 + 2000; n += 60)
    {
        float peak = 0.0f;
        for (size_t k = n; k < n + 240; ++k)
            peak = juce::jmax (peak, std::abs (out[k]));
        minEnv = juce::jmin (minEnv, peak);
    }
    for (size_t n = out.size() - 2400; n < out.size(); ++n)
        tailPeak = juce::jmax (tailPeak, std::abs (out[n]));

    check (minEnv < 0.02f,  "abDuck: the A/B switch dips through the silent bottom");
    check (tailPeak > 0.3f, "abDuck: and the stream recovers");
}

// ---------------------------------------------------------------------------
// The THIRD bulk-swap route DSP_POLICY invariant 8 enumerates — "a preset
// load, an A/B switch, or an undo step, three routes through the same forced
// duck, each owed its own test". The undo route was the one the code did not
// take: it restores a whole StateSet (discrete rewires included) and, since
// ADR-0014, stages the frozen-trim vector whose only landing site is the
// bottom. The edited parameter here is deliberately INAUDIBLE at this
// stimulus (compressor knee, nothing over threshold), so the only thing that
// can produce near-silence is the duck itself.
static void testUndoRequestsDuck()
{
    AnabasisAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, 512);

    std::vector<float> out;
    auto runBlock = [&] (int blockIdx)
    {
        for (int n = 0; n < 512; ++n)
        {
            const float v = 0.4f * std::sin (2.0f * juce::MathConstants<float>::pi
                                             * 300.0f * (float) (blockIdx * 512 + n) / 48000.0f);
            buf.setSample (0, n, v);
            buf.setSample (1, n, v);
        }
        proc.processBlock (buf, midi);
        for (int n = 0; n < 512; ++n)
            out.push_back (buf.getSample (0, n));
    };

    auto* knee = proc.apvts.getParameter (pid::compKnee);
    knee->beginChangeGesture();
    knee->setValueNotifyingHost (knee->getNormalisableRange().convertTo0to1 (9.0f));
    knee->endChangeGesture();
    proc.flushPendingDetach();
    check (proc.canUndo(), "undoDuck: (premise) the drag pushed a step");

    for (int b = 0; b < 10; ++b) runBlock (b);
    proc.undo();
    for (int b = 10; b < 30; ++b) runBlock (b);

    float minEnv = 1.0f, tailPeak = 0.0f;
    for (size_t n = 10 * 512; n < 10 * 512 + 2000; n += 60)
    {
        float peak = 0.0f;
        for (size_t k = n; k < n + 240; ++k)
            peak = juce::jmax (peak, std::abs (out[k]));
        minEnv = juce::jmin (minEnv, peak);
    }
    for (size_t n = out.size() - 2400; n < out.size(); ++n)
        tailPeak = juce::jmax (tailPeak, std::abs (out[n]));

    check (minEnv < 0.02f,  "undoDuck: an undo step dips through the silent bottom");
    check (tailPeak > 0.3f, "undoDuck: and the stream recovers");
}

// ---------------------------------------------------------------------------
// §2.9 meter publication: the wrapper measures the OUTPUT and publishes once
// per block through the THREAD_MODEL meter atomics. A −20 dBFS 997 Hz tone
// must read −20 LUFS momentary/integrated, ~−20 dBTP (sine ISP ≈ sample peak
// at 997 Hz), PLR = dbTpMax − lufsI, GR 0; the GR-history ring advances one
// entry per block with a release-stored index.
static void testMeterPublication()
{
    AnabasisAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, 512);

    const int blocks = (int) (6.0 * 48000.0 / 512.0);   // 6 s: I-gate warm
    for (int b = 0; b < blocks; ++b)
    {
        for (int n = 0; n < 512; ++n)
        {
            const float v = 0.1f * std::sin (2.0f * juce::MathConstants<float>::pi
                                             * 997.0f * (float) (b * 512 + n) / 48000.0f);
            buf.setSample (0, n, v);
            buf.setSample (1, n, v);
        }
        proc.processBlock (buf, midi);
    }

    auto near = [] (float a, float b, float tol) { return std::abs (a - b) <= tol; };
    check (near (proc.meterLufsM(), -20.0f, 0.3f), "meters: momentary reads the tone's loudness");
    check (near (proc.meterLufsI(), -20.0f, 0.2f), "meters: integrated agrees");
    check (near (proc.meterDbTpMax(), -20.0f, 0.2f),
           "meters: dBTP max-hold reads the sine's true peak");
    check (near (proc.meterPlr(), proc.meterDbTpMax() - proc.meterLufsI(), 1.0e-4f),
           "meters: PLR is dbTpMax minus integrated");
    check (near (proc.meterGrDb(), 0.0f, 0.01f), "meters: no reduction on a -20 dBFS tone");

    const auto& ring = proc.grHistory();
    check (ring.available() == (int64_t) blocks,
           "meters: the GR history ring advanced exactly one entry per block");
    const auto last = ring.peek (ring.available() - 1);
    check (near (last.peak, 0.1f, 0.01f), "meters: the history entry carries the block peak");

    // A block the engine short-circuits produces no render-tap values, so the
    // publish must be skipped too — otherwise the previous block's peaks are
    // re-reported and a duplicate entry lands in the ring, breaking exactly
    // the one-entry-per-block property asserted above.
    const auto before = ring.available();
    const float tpBefore = proc.meterDbTpMax();
    juce::AudioBuffer<float> empty (2, 0);
    proc.processBlock (empty, midi);
    check (ring.available() == before,
           "meters: a zero-length block pushes no history entry");
    check (juce::exactlyEqual (proc.meterDbTpMax(), tpBefore),
           "meters: a zero-length block re-publishes nothing");

    // The per-channel per-stage lanes (0.1.2 item 12): quiet tone → both
    // stages at zero on both channels; then +18 dB of limiter push → real
    // reduction on BOTH lanes, and at the default 100 % link the two lanes
    // agree exactly (one shared detector level, identical envelopes).
    check (juce::exactlyEqual (proc.meterLimGrDbCh (0), 0.0f)
               && juce::exactlyEqual (proc.meterCompGrDbCh (0), 0.0f),
           "meters: the per-channel GR lanes read zero on a -20 dBFS tone");
    {
        auto* par = proc.apvts.getParameter (pid::limGain);
        par->setValueNotifyingHost (par->getNormalisableRange().convertTo0to1 (18.0f));
        for (int b = 0; b < 30; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                const float v = 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                 * 997.0f * (float) (b * 512 + n) / 48000.0f);
                buf.setSample (0, n, v);
                buf.setSample (1, n, v);
            }
            proc.processBlock (buf, midi);
        }
        check (proc.meterLimGrDbCh (0) < -3.0f && proc.meterLimGrDbCh (1) < -3.0f,
               "meters: +18 dB of push shows real reduction on both limiter lanes");
        check (std::abs (proc.meterLimGrDbCh (0) - proc.meterLimGrDbCh (1)) < 1.0e-4f,
               "meters: at 100% link the two lanes agree");

        // …and the display-clear guarantee reaches them (0.1.2 review). A
        // meter reset with NO audio flowing must blank the lanes — that is
        // what `publishSilentMeters` promises for every published meter, and
        // the reason it now ends in an engine call: the limiter lane moved off
        // the wrapper's `pubGrDb` onto the engine's per-channel atomics at
        // 0.1.2 and stopped obeying the list, while the comp lane had never
        // obeyed it. Driven from the message-thread path deliberately: the
        // audio-thread consume would clear them by processing a block, which
        // is not the case that was broken.
        proc.requestMeterReset();
        check (juce::exactlyEqual (proc.meterLimGrDbCh (0), 0.0f)
                   && juce::exactlyEqual (proc.meterLimGrDbCh (1), 0.0f)
                   && juce::exactlyEqual (proc.meterCompGrDbCh (0), 0.0f)
                   && juce::exactlyEqual (proc.meterCompGrDbCh (1), 0.0f),
               "meters: a meter reset with no audio blanks both GR lanes on both stages");
    }
}

// The ADR-0020 Waveform-Statistics rows, driven through the REAL wrapper on
// the same stimulus the row above uses: a −20 dBFS 997 Hz sine, whose every
// statistic is known in closed form.
//   sample peak  = −20.00 dBFS (the amplitude)
//   RMS (math.)  = −23.01 dBFS (a sine's RMS is 1/√2 of its peak)
//   RMS (AES-17) = −20.00 dBFS (the same number, +3.01, which is the WHOLE
//                  content of the reference choice — so asserting both pins
//                  the offset without a second stimulus)
//   LRA          = 0 LU (a steady tone has no range at all)
// The ungated and gated integrated readings agree here BY CONSTRUCTION — no
// block of a continuous tone falls below either gate — which is what makes
// the third case below meaningful rather than tautological: it feeds silence
// after the tone, which the gates treat differently.
static void testTheWaveformStatisticsRowsReadTheirStandards()
{
    AnabasisAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, 512);
    auto near = [] (float a, float b, float tol) { return std::abs (a - b) <= tol; };

    auto runTone = [&] (int blocks, float amp)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                const float v = amp * std::sin (2.0f * juce::MathConstants<float>::pi
                                                * 997.0f * (float) (b * 512 + n) / 48000.0f);
                buf.setSample (0, n, v);
                buf.setSample (1, n, v);
            }
            proc.processBlock (buf, midi);
        }
    };
    runTone ((int) (8.0 * 48000.0 / 512.0), 0.1f);      // 8 s: LRA needs > 3 s of window

    check (near (proc.meterPeakMaxDb(), -20.0f, 0.05f),
           "stats: the sample-peak hold reads the tone's amplitude");
    check (near (proc.meterRmsDb(), -23.01f, 0.15f),
           "stats: the published RMS is the MATHEMATICAL reference (a full-scale sine is -3.01)");
    check (proc.meterPeakMaxDb() > proc.meterRmsDb() + 2.5f,
           "stats: …and it sits below the sample peak by the sine's crest factor");
    check (near (proc.meterLra(), 0.0f, 0.6f),
           "stats: a steady tone has essentially no loudness range");
    check (near (proc.meterLufsIUngated(), proc.meterLufsI(), 0.1f),
           "stats: on a continuous tone the gated and ungated integrated readings agree");

    // SILENCE AFTER THE TONE is what separates the two integrated standards:
    // BS.1770-2's absolute gate drops the silent blocks, so the gated figure
    // holds at the tone's loudness; BS.1770-1 has no gate, so the ungated
    // figure is dragged down by them. A mutant that publishes the same value
    // twice, or that applies the -70 gate to the ungated accumulator, fails
    // exactly here.
    const float gatedBefore = proc.meterLufsI();
    for (int b = 0; b < (int) (8.0 * 48000.0 / 512.0); ++b)
    {
        buf.clear();
        proc.processBlock (buf, midi);
    }
    check (near (proc.meterLufsI(), gatedBefore, 0.2f),
           "stats: the GATED integrated reading ignores the silence that follows");
    check (proc.meterLufsIUngated() < gatedBefore - 2.0f,
           "stats: the UNGATED (BS.1770-1) reading is dragged down by it");

    // THE PLR ROW IS THE DIFFERENCE OF THE TWO ROWS ABOVE IT, under whichever
    // integrated standard §3.5 selects. `meterPlr()` is published against the
    // GATED figure alone, so it is the wrong reference the moment BS.1770-1 is
    // showing — and the silence just run is what drives the two standards more
    // than 2 LU apart, which makes that a visible disagreement rather than a
    // theoretical one. The rule lives in `plrFromShown`, which takes both
    // operands, so the suite can pin it without driving the panel's
    // FrameClock tick.
    const float tpNow = proc.meterDbTpMax();
    check (near (LoudnessMeterView::plrFromShown (tpNow, proc.meterLufsI()),
                 proc.meterPlr(), 1.0e-4f),
           "stats: under BS.1770-2 the PLR row reproduces the published gated figure exactly");
    check (near (LoudnessMeterView::plrFromShown (tpNow, proc.meterLufsIUngated()),
                 tpNow - proc.meterLufsIUngated(), 1.0e-4f),
           "stats: under BS.1770-1 the PLR row is TP minus the UNGATED figure it shows");
    check (std::abs (LoudnessMeterView::plrFromShown (tpNow, proc.meterLufsIUngated())
                     - proc.meterPlr()) > 2.0f,
           "stats: …and that is NOT the published PLR, which is why the row derives its own");
    check (juce::exactlyEqual (LoudnessMeterView::plrFromShown (
                                   tpNow, anabasis::LoudnessMeter::kSilentLufs), 0.0f),
           "stats: with no integrated reading the PLR row reads 0, as the published figure does");

    // The reset clears both peak holds, not only the true peak's.
    proc.requestMeterReset();
    buf.clear();
    proc.processBlock (buf, midi);
    check (proc.meterPeakMaxDb() < -100.0f,
           "stats: the meter reset clears the sample-peak hold too");

    // ---- The RMS numeric row's two display rules (0.1.3) -------------------
    // Pinned through the statics for the reason the PLR rule is: they carry
    // the argument, and pinning them needs no FrameClock tick.
    //
    // The CADENCE (item 1): the row must not chase the ~24 Hz meter tick.
    const float aReading = -18.0f, another = -11.0f;
    check (! LoudnessMeterView::shouldAdoptRms (another, /*holding*/ true, 0.04),
           "rmsRow: a fresh measurement one meter frame after the last is NOT adopted");
    check (LoudnessMeterView::shouldAdoptRms (another, true, 0.40),
           "rmsRow: …and IS adopted once the readable hold has expired");
    // The two sentinel bypasses, which is what keeps a meter reset honest:
    // the sentinel itself blanks the row on the next frame, and the first real
    // reading after one lands immediately instead of waiting out an interval
    // it was never part of.
    check (LoudnessMeterView::shouldAdoptRms (anabasis::RmsMeter::kSilentDb, true, 0.0),
           "rmsRow: the meter-reset sentinel is adopted immediately, hold or not");
    check (LoudnessMeterView::shouldAdoptRms (aReading, /*holding*/ false, 0.0),
           "rmsRow: the first reading after a reset is adopted immediately");

    // The REFERENCE (the 0.1.3 review fix): the §3.5 choice is applied to
    // whatever measurement is on screen, so it can never be gated by the hold.
    // Asserted as a property of the PAIR — for any held measurement the two
    // references differ by exactly the AES-17 offset — rather than by
    // re-deriving the constant here.
    check (near (LoudnessMeterView::rmsWithReference (aReading, true)
                 - LoudnessMeterView::rmsWithReference (aReading, false), 3.0103f, 1.0e-4f),
           "rmsRow: the AES-17 reference is exactly the offset above the mathematical one");
    check (juce::exactlyEqual (
               LoudnessMeterView::rmsWithReference (anabasis::RmsMeter::kSilentDb, true),
               anabasis::RmsMeter::kSilentDb),
           "rmsRow: the sentinel passes the reference untouched, so \"-\" stays \"-\"");
    // The mutant this pair kills is the shipped 0.1.3 form, which applied the
    // reference INSIDE the hold: `rmsWithReference` is a pure function of the
    // held raw value, so the row's response to a Settings flip is one frame by
    // construction — there is no state between the choice and the print.
    check (! juce::exactlyEqual (LoudnessMeterView::rmsWithReference (aReading, true),
                                 LoudnessMeterView::rmsWithReference (aReading, false)),
           "rmsRow: …and the two references are distinguishable, so the flip is visible");
}

// ---------------------------------------------------------------------------
// §7 factory presets: compiled-in override tables — defaults first, then the
// intents — through the SAME lock/exclusion semantics as file presets, with
// an empty detach mask (nothing loads pre-detached), one undo step, and the
// dirty marker clean right after an apply and set by the next edit.
// ---------------------------------------------------------------------------
// A preset application that RESTORES NOTHING is not a new user action: it must
// mint no undo step and must leave the redo line alone. Re-applying the same
// preset over an EDITED surface is a real restore and stays undoable.
//
// The distinction is a test of the STATE, not of which row was clicked, which
// is why both halves click the SAME preset and differ only in whether the
// surface moved in between.
static void testANoOpPresetApplyIsNotAUserAction()
{
    AnabasisAudioProcessor proc;
    auto& apvts = proc.apvts;

    int count = 0;
    const auto* table = PresetManager::factoryPresets (count);
    const juce::String applied = table[3].name;

    // 1) apply a preset  2) edit it  3) undo  4) a redo line exists
    check (proc.applyFactoryPreset (3), "noOpApply: (premise) the preset applies");
    auto* knee = apvts.getParameter (pid::compKnee);
    knee->beginChangeGesture();
    knee->setValueNotifyingHost (knee->getNormalisableRange().convertTo0to1 (1.0f));
    knee->endChangeGesture();
    proc.flushPendingDetach();
    check (proc.canUndo(), "noOpApply: (premise) the edit pushed a step");
    check (proc.presetDirty(), "noOpApply: (premise) the edit dirties the preset");
    proc.undo();
    check (proc.canRedo(), "noOpApply: (premise) the undo leaves a redo line");
    check (! proc.presetDirty(), "noOpApply: (premise) the undo lands back on the preset");

    // 5) re-apply the ALREADY-CURRENT, unchanged preset.
    check (proc.applyFactoryPreset (3), "noOpApply: (premise) the same preset re-applies");

    // 6) the redo line survives. This is the whole point: `pushUndoStep` clears
    //    redo unconditionally, so a step pushed here would have destroyed it.
    check (proc.canRedo(), "noOpApply: re-applying the current preset KEEPS the redo line");

    // 7) …and no dead step was minted. Read behaviourally rather than through a
    //    depth accessor: one Undo must now reach past the re-apply to the
    //    ORIGINAL apply and leave the preset. A dead step would have been
    //    consumed by this press instead, leaving the preset still applied.
    check (proc.canUndo(), "noOpApply: (premise) the original apply is still undoable");
    proc.undo();
    const juce::String afterUndo = proc.currentPresetName();
    check (afterUndo != applied,
           juce::String ("noOpApply: no dead step -- one Undo reaches the original apply, got '"
                         + afterUndo + "'").toRawUTF8());

    // The other half: the same preset over a MOVED surface IS a restore.
    AnabasisAudioProcessor p2;
    check (p2.applyFactoryPreset (3), "noOpApply: (premise) second instance applies it");
    auto* drive = p2.apvts.getParameter (pid::clipDrive);
    const float driveBefore = p2.apvts.getRawParameterValue (pid::clipDrive)->load();
    // Move it RELATIVE to whatever the preset holds — a fixed target would be a
    // no-op the day a preset happens to name that value, and the premise check
    // below would then be the only thing failing.
    const float driveNorm = drive->getValue();
    drive->beginChangeGesture();
    drive->setValueNotifyingHost (driveNorm > 0.5f ? driveNorm - 0.4f : driveNorm + 0.4f);
    drive->endChangeGesture();
    p2.flushPendingDetach();
    const float driveEdited = p2.apvts.getRawParameterValue (pid::clipDrive)->load();
    check (std::abs (driveEdited - driveBefore) > 0.5f, "noOpApply: (premise) the edit moved Clip Drive");
    check (p2.presetDirty(), "noOpApply: (premise) it dirties the preset");

    check (p2.applyFactoryPreset (3), "noOpApply: (premise) re-apply over the edit");
    check (! p2.presetDirty(), "noOpApply: re-applying over an edit restores the preset");
    // A REAL restore, so it is undoable and the edit comes back.
    check (p2.canUndo(), "noOpApply: (premise) the restore is undoable");
    p2.undo();
    check (std::abs (p2.apvts.getRawParameterValue (pid::clipDrive)->load() - driveEdited) < 0.01f,
           "noOpApply: re-applying over an EDITED surface is a real, undoable step");
}

// Retracting a no-op preset apply must restore the undo history's DEPTH, not
// just its top. `pushCapped` trims from the FRONT once the 128-step cap is
// exceeded, so on a full stack the bracket's push costs the oldest entry —
// and popping the entry it pushed does not bring that back. Left unfixed,
// re-clicking the loaded preset quietly shortens how far back a long session
// can undo, one step per click.
// ---------------------------------------------------------------------------
// The restored-session leg of the no-op re-apply, which the two tests above do
// NOT reach: both build a fresh `AnabasisAudioProcessor`, whose constructor
// seeds a VALID `presetBaseline`. `setStateInformation` runs
// `resetSlotFieldsToDefaults`, which deliberately invalidates that datum (the
// dirty marker is not serialized, so a load cannot honestly restore it), and
// nothing rebuilds it until the next apply or save. So in every project opened
// from a host, the bracket captures an INVALID `preBaseline`, the apply installs
// a VALID one, and the equivalence test in `closePresetUndoBracket` compares
// invalid against valid — never equivalent — so the retraction is refused and
// the redo line the push cleared is never re-seated.
//
// Open a project, click the preset name that is already showing: redo gone, and
// an undo step that does nothing. Which is the behaviour 0.1.4 says it fixed.
static void testANoOpPresetApplyIsNotAUserActionAfterASessionRestore()
{
    AnabasisAudioProcessor proc;
    auto& apvts = proc.apvts;

    int count = 0;
    const auto* table = PresetManager::factoryPresets (count);
    const juce::String applied = table[3].name;

    check (proc.applyFactoryPreset (3), "restoredNoOp: (premise) the preset applies");

    // Round-trip through the HOST path, which is what makes this different from
    // the fresh-processor legs above.
    juce::MemoryBlock blob;
    proc.getStateInformation (blob);
    AnabasisAudioProcessor q;
    q.setStateInformation (blob.getData(), (int) blob.getSize());
    check (q.currentPresetName() == applied,
           "restoredNoOp: (premise) the restored session carries the preset's name");

    // Edit → undo, so there IS a redo line to lose.
    auto* knee = q.apvts.getParameter (pid::compKnee);
    knee->beginChangeGesture();
    knee->setValueNotifyingHost (knee->getNormalisableRange().convertTo0to1 (1.0f));
    knee->endChangeGesture();
    q.flushPendingDetach();
    check (q.canUndo(), "restoredNoOp: (premise) the edit pushed a step");
    q.undo();
    check (q.canRedo(), "restoredNoOp: (premise) the undo leaves a redo line");

    // Re-apply the preset that is already loaded, over an unedited surface.
    check (q.applyFactoryPreset (3), "restoredNoOp: (premise) the same preset re-applies");

    check (q.canRedo(),
           "restoredNoOp: re-applying the current preset KEEPS the redo line in a RESTORED session");

    // …and minted no step. Read as a DEPTH here rather than behaviourally: a
    // restore clears the per-slot history, so after edit-then-undo the stack is
    // legitimately EMPTY and there is nothing beneath a dead step to reach past.
    // That makes the depth the sharper instrument in this leg — `canUndo()` is
    // true if and ONLY if the bracket's push survived, which is the defect.
    check (! q.canUndo(),
           "restoredNoOp: …and mints no dead step -- the bracket's own push is retracted");

    // The redo line is not merely present, it still carries the edit.
    const float kneeBefore = q.apvts.getRawParameterValue (pid::compKnee)->load();
    q.redo();
    check (! juce::exactlyEqual (q.apvts.getRawParameterValue (pid::compKnee)->load(), kneeBefore),
           "restoredNoOp: the surviving redo line still restores the edit it was holding");

    // THE DIRTY MARKER, which the legs above measure only through undo/redo
    // depth. It is the one place the retraction is observable at all:
    // `setStateInformation` leaves `presetBaseline` INVALID on purpose (the
    // marker is not serialized, so a load cannot honestly restore it) and
    // `presetDirty()` answers false unconditionally while it is absent. The
    // retraction un-records the bookkeeping WITHOUT rolling the datum back, so
    // the session comes out of a no-op re-apply holding a working marker where
    // it had none. Both halves of that are asserted here rather than argued:
    // still clean straight after — nothing about the sound changed — and
    // ANSWERING again, which is what a marker seeded by the re-apply buys and
    // what a rolled-back one would not.
    juce::MemoryBlock blobD;
    proc.getStateInformation (blobD);
    AnabasisAudioProcessor d;
    d.setStateInformation (blobD.getData(), (int) blobD.getSize());
    check (! d.presetDirty(),
           "restoredNoOp: (premise) a restored session reads CLEAN — the marker is absent");
    check (d.applyFactoryPreset (3), "restoredNoOp: (premise) the same preset re-applies over it");
    check (! d.presetDirty(),
           "restoredNoOp: the retracted re-apply leaves the preset reading clean");

    auto* kneeD = d.apvts.getParameter (pid::compKnee);
    kneeD->beginChangeGesture();
    kneeD->setValueNotifyingHost (kneeD->getNormalisableRange().convertTo0to1 (1.0f));
    kneeD->endChangeGesture();
    d.flushPendingDetach();
    check (d.presetDirty(),
           "restoredNoOp: …and the marker now WORKS -- an edit after it reads dirty, "
           "where before the re-apply the same edit read clean");

    // THE OTHER DIRECTION, and it is the one that stops this fix from being a
    // loosening: in a restored session the baseline half is now satisfied by
    // construction, so the SURFACE half is the only thing left deciding. A real
    // restore must still mint its step.
    AnabasisAudioProcessor r;
    check (r.applyFactoryPreset (3), "restoredNoOp: (premise) third instance applies it");
    juce::MemoryBlock blob2;
    r.getStateInformation (blob2);
    AnabasisAudioProcessor s;
    s.setStateInformation (blob2.getData(), (int) blob2.getSize());

    auto* drive = s.apvts.getParameter (pid::clipDrive);
    const float driveNorm   = drive->getValue();
    const float driveBefore = s.apvts.getRawParameterValue (pid::clipDrive)->load();
    drive->beginChangeGesture();
    drive->setValueNotifyingHost (driveNorm > 0.5f ? driveNorm - 0.4f : driveNorm + 0.4f);
    drive->endChangeGesture();
    s.flushPendingDetach();
    check (std::abs (s.apvts.getRawParameterValue (pid::clipDrive)->load() - driveBefore) > 0.5f,
           "restoredNoOp: (premise) the edit moved Clip Drive in the restored session");

    check (s.applyFactoryPreset (3), "restoredNoOp: (premise) the preset re-applies over the edit");
    check (s.canUndo(),
           "restoredNoOp: re-applying over an EDITED surface is still a real restore, and keeps "
           "its undo step even though the restored session has no baseline");
    s.undo();
    check (std::abs (s.apvts.getRawParameterValue (pid::clipDrive)->load() - driveBefore) > 0.5f,
           "restoredNoOp: …and that step undoes back to the edit, not past it");
    juce::ignoreUnused (apvts, count);
}

static void testANoOpPresetApplyDoesNotEatTheOldestUndoStep()
{
    // Drains the history to count it — destructive, so it is the last thing done
    // to an instance.
    auto depthOf = [] (AnabasisAudioProcessor& p)
    {
        int n = 0;
        while (p.canUndo() && n < 1000) { p.undo(); ++n; }
        return n;
    };
    auto fill = [] (AnabasisAudioProcessor& p, int pushes)
    {
        auto* k = p.apvts.getParameter (pid::compKnee);
        const auto& r = k->getNormalisableRange();
        for (int i = 0; i < pushes; ++i)
        {
            k->beginChangeGesture();
            k->setValueNotifyingHost (r.convertTo0to1 (1.0f + 0.05f * (float) (i % 20)));
            k->endChangeGesture();
            p.flushPendingDetach();
        }
    };

    // The cap is private; measure it rather than quote a number this test would
    // then have to be kept in step with.
    int cap = 0;
    {
        AnabasisAudioProcessor probe;
        fill (probe, 400);
        cap = depthOf (probe);
        check (cap > 0, "undoCap: (premise) gestured edits push undo steps");
        check (cap < 400, "undoCap: (premise) the stack is capped, so a push can evict");
    }

    // CONTROL: fill past the cap, then ONE real preset restore. The surface has
    // moved, so this apply legitimately keeps its step.
    int control = 0;
    {
        AnabasisAudioProcessor a;
        fill (a, cap + 20);
        check (a.applyFactoryPreset (3), "undoCap: (premise) the control applies the preset");
        control = depthOf (a);
        check (control == cap, "undoCap: (premise) the control history sits at the cap");
    }

    // SUBJECT: identical, plus a second apply of the SAME preset. By then the
    // surface is exactly that preset and unedited, so the second apply restores
    // nothing and must be retracted — including the entry its push evicted from
    // the FRONT of a full stack, which `removeLast()` alone cannot give back.
    AnabasisAudioProcessor b;
    fill (b, cap + 20);
    check (b.applyFactoryPreset (3), "undoCap: (premise) the subject applies the preset");
    check (! b.presetDirty(), "undoCap: (premise) the surface now IS the preset, unedited");
    check (b.applyFactoryPreset (3), "undoCap: (premise) and the same preset re-applies");

    const int subject = depthOf (b);
    const auto msg = juce::String ("undoCap: a no-op re-apply costs no history depth (subject "
                                   + juce::String (subject) + " vs control "
                                   + juce::String (control) + ")");
    check (subject == control, msg.toRawUTF8());
}

// A stored A/B slot that is structurally present but carries no usable
// parameter payload must not lend its NAME and IDENTITY to a sound that came
// from somewhere else. The read rule is the one the live surface already
// follows: a missing payload means DEFAULTS, never "keep whatever is live".

// THE OTHER HALF of the payload-less-slot rule, and the one nothing pinned.
// `setStateInformation` gates the ACTIVE slot's metadata on `liveSurfaceRestored`
// — the ROOT `ANABASIS` child being present — rather than on the slot's own
// payload. So a blob that keeps a COMPLETE `AB` but loses the root surface loads
// the default sound AND drops the active slot's name, identity, macro baseline,
// FROZEN TRIMS and DETACH MASK, even though the slot itself carries all of them.
//
// That is a strictly larger behaviour change than the malformed-STORED-slot case
// above, and it is the deliberate one: metadata describes a parameter surface,
// and the surface this restore installed came from defaults, so a mask naming
// detachments from a mapping the session never had is not a mask worth keeping.
// Deliberate is not the same as pinned: the DETACH MASK is the carrier a user
// cannot otherwise recover, and it had no assertion at all on this path.
static void testARootlessSurfaceDropsTheActiveSlotsMetadataToo()
{
    AnabasisAudioProcessor proc;
    check (proc.applyFactoryPreset (4), "rootlessActive: (premise) a preset is applied");
    // Detach a parameter so the mask is NON-EMPTY — an empty one would satisfy
    // the assertion below by accident.
    //
    // SCOPE, stated rather than left to be discovered: this covers the detach
    // mask and the name, not the frozen latch. `FROZEN_TRIMS` is written only
    // once `retainedTrimGeneration()` is non-zero, which needs the adaptive
    // engine to have latched trims from real audio — the setup
    // `testFrozenTrimRestore` already builds. Asserting it here without that
    // setup would assert the absence of a child that was never written, which is
    // the vacuous shape this suite keeps finding. The gate is the same one for
    // all five carriers; this pins the two a user cannot otherwise recover.
    if (auto* gain = proc.apvts.getParameter (pid::limGain))
    {
        gain->beginChangeGesture();          // a GESTURED managed write detaches (§5.3)
        gain->setValueNotifyingHost (0.80f);
        gain->endChangeGesture();
    }
    proc.getMacroEngine().drainTick();   // stages -> mask, as `restoreTick` does
    const auto maskBefore = proc.detachMask();
    check (! proc.currentPresetName().isEmpty(), "rootlessActive: (premise) the slot has a name");

    juce::MemoryBlock blob;
    proc.getStateInformation (blob);
    auto xml = juce::AudioProcessor::getXmlFromBinary (blob.getData(), (int) blob.getSize());
    check (xml != nullptr, "rootlessActive: (premise) the session blob parses");
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);

    // The premise that makes the assertions mean something: the ACTIVE slot
    // still carries everything. Only the ROOT surface is removed.
    auto ab = root.getChildWithName ("AB");
    check (ab.isValid(), "rootlessActive: (premise) the blob carries an AB child");
    if (! ab.isValid()) return;
    const int active = (int) ab.getProperty ("active", 0);
    juce::Array<juce::ValueTree> slots;
    for (int i = 0; i < ab.getNumChildren(); ++i)
        if (ab.getChild (i).hasType ("SLOT"))
            slots.add (ab.getChild (i));
    check (slots.size() == 2, "rootlessActive: (premise) both slots are present");
    if (slots.size() != 2) return;
    auto liveSlot = slots[active];
    check (liveSlot.getChildWithName ("ANABASIS").isValid(),
           "rootlessActive: (premise) the ACTIVE slot keeps its full parameter payload");
    check (liveSlot.getChildWithName ("DETACH_MASK").getNumChildren() > 0,
           "rootlessActive: (premise) the active slot carries a NON-EMPTY detach mask");

    root.removeChild (root.getChildWithName ("ANABASIS"), nullptr);   // the only edit

    // A PREMISE, not a convenience. The earlier form defaulted to a `<dummy/>`
    // element when `createXml()` returned null — which `setStateInformation`
    // correctly declines as a foreign root, so every assertion below would then
    // pass trivially against a processor that had loaded nothing: default name,
    // empty mask, default surface, all three "correct" for the wrong reason. A
    // test that cannot fail is worse than an absent one.
    const auto outXml = root.createXml();
    check (outXml != nullptr, "rootlessActive: (premise) the edited tree re-serialises");
    if (outXml == nullptr) return;
    juce::MemoryBlock rewritten;
    juce::AudioProcessor::copyXmlToBinary (*outXml, rewritten);

    AnabasisAudioProcessor q;
    q.setStateInformation (rewritten.getData(), (int) rewritten.getSize());

    check (q.currentPresetName() == "Default",
           "rootlessActive: a rootless blob loads the DEFAULT name, not the slot's");
    check (q.detachMask().isEmpty(),
           "rootlessActive: ...and an EMPTY detach mask, not the slot's — the mask describes "
           "detachments from a mapping this restore never installed");
    check (! maskBefore.isEmpty(),
           "rootlessActive: (premise) the mask being compared against was non-empty");
    // The sound is the defaults', which is the rule this widening comes from.
    AnabasisAudioProcessor fresh;
    check (juce::exactlyEqual (q.apvts.getRawParameterValue (pid::limGain)->load(),
                               fresh.apvts.getRawParameterValue (pid::limGain)->load()),
           "rootlessActive: ...and the DEFAULT surface, so metadata and sound agree");
}

static void testAMalformedStoredSlotCannotSplitSoundFromMetadata()
{
    AnabasisAudioProcessor proc;

    // A real session first: a named preset with a moved surface.
    check (proc.applyFactoryPreset (4), "malformedSlot: (premise) a preset is applied");
    auto* gain = proc.apvts.getParameter (pid::limGain);
    gain->setValueNotifyingHost (0.80f);

    juce::MemoryBlock blob;
    proc.getStateInformation (blob);
    auto xml = juce::AudioProcessor::getXmlFromBinary (blob.getData(), (int) blob.getSize());
    check (xml != nullptr, "malformedSlot: (premise) the session blob parses");
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    auto ab = root.getChildWithName ("AB");
    check (ab.isValid(), "malformedSlot: (premise) the blob carries an AB child");
    if (! ab.isValid()) return;

    const int active = (int) ab.getProperty ("active", 0);
    juce::Array<juce::ValueTree> slots;
    for (int i = 0; i < ab.getNumChildren(); ++i)
        if (ab.getChild (i).hasType ("SLOT"))
            slots.add (ab.getChild (i));
    check (slots.size() == 2, "malformedSlot: (premise) both slots are present");
    if (slots.size() != 2) return;

    // The truncation: a SLOT that is a valid node, carries metadata, and has no
    // parameter payload at all.
    auto stored = slots[1 - active];
    stored.removeChild (stored.getChildWithName ("ANABASIS"), nullptr);
    stored.setProperty ("presetName", "Ghost Session", nullptr);
    check (! stored.getChildWithName ("ANABASIS").isValid(),
           "malformedSlot: (premise) the stored slot now has no parameter payload");

    // Restore into the SAME instance, then switch into the broken slot.
    const auto doc = root.createXml();
    juce::MemoryBlock hacked;
    juce::AudioProcessor::copyXmlToBinary (*doc, hacked);
    proc.setStateInformation (hacked.getData(), (int) hacked.getSize());
    proc.switchToSlot (1 - proc.activeSlotIndex());

    // Both halves of the slot resolved to pristine defaults. The failure this
    // pins is the metadata arriving WITHOUT its sound.
    const juce::String nameAfter = proc.currentPresetName();
    check (nameAfter != "Ghost Session",
           juce::String ("malformedSlot: a payload-less slot lent its preset name to another state ('"
                         + nameAfter + "')").toRawUTF8());
    check (nameAfter == "Default",
           juce::String ("malformedSlot: the slot resolves to the default name, got '"
                         + nameAfter + "'").toRawUTF8());
    auto* gp = proc.apvts.getParameter (pid::limGain);
    const float gainDefault = gp->getNormalisableRange().convertFrom0to1 (gp->getDefaultValue());
    check (std::abs (proc.apvts.getRawParameterValue (pid::limGain)->load() - gainDefault) < 0.01f,
           "malformedSlot: the slot's SOUND is the default, not the previous state's");
}

static void testFactoryPresets()
{
    AnabasisAudioProcessor proc;
    auto& apvts = proc.apvts;

    int count = 0;
    const auto* table = PresetManager::factoryPresets (count);
    check (count == 13, "factory: the bank is Default + the brief's >=12 (5 named + 7 owner-approved 2026-08-02)");

    // Index 0 is "Default" with an EMPTY override table, and the fresh state
    // already carries its identity: the plugin opens ON a preset, not on a
    // nameless placeholder (owner directive 2026-08-05, the sibling's pattern).
    check (juce::String (table[0].name) == "Default" && table[0].numOverrides == 0,
           "factory: index 0 is Default with zero overrides");
    check (proc.currentPresetName() == "Default" && ! proc.presetDirty(),
           "factory: a fresh instance reads as a clean Default");
    {
        auto* push = apvts.getParameter (pid::loudness);
        push->setValueNotifyingHost (push->getNormalisableRange().convertTo0to1 (30.0f));
        check (proc.presetDirty(), "factory: editing the fresh Default stars it");
        check (proc.applyFactoryPreset (0), "factory: (premise) re-apply Default");
        check (std::abs (apvts.getRawParameterValue (pid::loudness)->load()) < 0.5f
                   && ! proc.presetDirty(),
               "factory: re-applying Default restores the default patch, clean");
    }

    // Apply EDM Club (index 3): macros land, style lands.
    check (proc.applyFactoryPreset (3), "factory: apply succeeds");
    check (proc.currentPresetName() == juce::String (table[3].name),
           "factory: the preset name is the table's");
    check (std::abs (apvts.getRawParameterValue (pid::loudness)->load() - 80.0f) < 0.5f,
           "factory: the loudness macro landed");
    // EDM Club's explicit ceiling override was REMOVED 2026-08-05 when every
    // ceiling moved to the -0.1 default (owner directive) — the slot now
    // proves both halves of "defaults + intents": an override that exists
    // lands (limStyle 2 = Loud), and a value with no override sits at the
    // REGISTERED default rather than at some earlier preset's leftovers.
    check (std::abs (apvts.getRawParameterValue (pid::limStyle)->load() - 2.0f) < 0.01f,
           "factory: the limStyle override landed");
    check (std::abs (apvts.getRawParameterValue (pid::ceiling)->load() - (-0.1f)) < 0.01f,
           "factory: an un-overridden field sits at the registered default");

    // …and the macro position is TRANSLATED. A factory table is defaults plus a
    // few intents and expresses itself through the macros (PresetManager.h), so
    // the nine §5.5 managed parameters come out of the defaults pass at their
    // defaults and only the mapping can give the preset its sound. The apply
    // runs inside a ScopedRestore — correct for a FILE preset, which carries
    // every parameter itself — whose destructor aborts the mapping the macro
    // writes armed; with nothing re-running it, "EDM Club" moved `loudness` to
    // 80 and left the compressor, clipper, limiter and EQ at M(0,0,0).
    // Asserted against the curves themselves, not against magic numbers: these
    // are ⊕ drafts and the test must follow them when they are tuned.
    {
        const float l = apvts.getRawParameterValue (pid::loudness)->load() / 100.0f;
        auto* gainParam = apvts.getParameter (pid::limGain);
        const float gainDefault = gainParam->getNormalisableRange().convertFrom0to1 (
                                      gainParam->getDefaultValue());
        check (std::abs (macro_curves::limGainDb (l) - gainDefault) > 1.0f,
               "factory: (premise) the mapped limiter gain differs from the default");
        check (std::abs (apvts.getRawParameterValue (pid::limGain)->load()
                           - macro_curves::limGainDb (l)) < 0.05f,
               "factory: the macro position is mapped onto the managed parameters");
        check (std::abs (apvts.getRawParameterValue (pid::compThreshold)->load()
                           - macro_curves::compThresholdDb (l)) < 0.05f,
               "factory: …the whole managed set, not just the one the table names");
    }
    check (proc.detachMask().isEmpty(), "factory: nothing loads pre-detached");
    check (! proc.presetDirty(), "factory: clean right after the apply");
    check (proc.canUndo(), "factory: the apply is one undoable step");

    // An override table is DEFAULTS + intents: a stray value from before the
    // apply must not survive into a preset that does not name it.
    auto* knee = apvts.getParameter (pid::compKnee);
    knee->setValueNotifyingHost (knee->getNormalisableRange().convertTo0to1 (1.0f));
    check (proc.presetDirty(), "factory: an edit sets the dirty marker");
    check (proc.applyFactoryPreset (0), "factory: (premise) second apply");
    const float kneeAfter = apvts.getRawParameterValue (pid::compKnee)->load();
    auto* kneeParam = apvts.getParameter (pid::compKnee);
    const float kneeDefault = kneeParam->getNormalisableRange().convertFrom0to1 (
                                  kneeParam->getDefaultValue());
    check (std::abs (kneeAfter - kneeDefault) < 0.01f,
           "factory: an unnamed parameter returns to its default (defaults + intents)");

    // Ceiling lock: browsing factory presets never moves a locked ceiling.
    //
    // WHICH HALF OF THE RULE THIS REACHES, since it is no longer both. An apply
    // writes every non-excluded parameter to its default and then the table's
    // intents over the top, and the lock skips the parameter in that one walk —
    // so with no factory table naming `ceiling` (EDM Club's −0.5 override went
    // with the ADR-0015 default change, which made it redundant), this check
    // exercises the DEFAULTS pass alone: unlocked, the apply would reset the
    // ceiling to −0.1, and the lock is what keeps it at −6. That is a real
    // assertion, not a vacuous one. The other half — an OVERRIDE aimed at the
    // locked parameter — has no expression left in the shipped bank, and is
    // pinned instead by `testALockedCeilingSurvivesAPresetThatNamesIt` below,
    // on the file-preset path where a document naming `ceiling` is reachable
    // without adding a preset to the product to test with.
    auto* ceiling = apvts.getParameter (pid::ceiling);
    ceiling->setValueNotifyingHost (ceiling->getNormalisableRange().convertTo0to1 (-6.0f));
    proc.internalState.state().setProperty (iid::ceilingLock, true, nullptr);
    check (proc.applyFactoryPreset (2), "factory: (premise) locked apply");
    check (std::abs (apvts.getRawParameterValue (pid::ceiling)->load() - (-6.0f)) < 0.01f,
           "factory: a locked ceiling is never written by a factory preset");
    proc.internalState.state().setProperty (iid::ceilingLock, false, nullptr);

    // An out-of-range index is refused before the undo bracket.
    while (proc.canUndo()) proc.undo();
    check (! proc.applyFactoryPreset (99), "factory: (premise) bad index refused");
    check (! proc.canUndo(), "factory: a refused index cost no undo step");

    // The dirty datum is PER SLOT, and it is not serialized. `livePresetName`
    // is per-slot state, so one engine-wide baseline described the wrong slot
    // the moment the user switched — and survived a session load, which cannot
    // restore it (a session records which preset a slot holds, never whether it
    // had been edited since).
    // BOTH slots must hold a NAMED preset, or `presetDirty()` early-returns on
    // the empty name and the stimulus never reaches the datum at all (the first
    // version of this test did exactly that and the mutant strolled past it).
    {
        AnabasisAudioProcessor p2;
        check (p2.applyFactoryPreset (1), "dirtyDatum: (premise) slot A takes a preset");
        const auto nameA = p2.currentPresetName();
        juce::MemoryBlock sessionWithA;
        p2.getStateInformation (sessionWithA);       // A, clean, saved for the load case

        p2.switchToSlot (1);
        check (p2.applyFactoryPreset (2), "dirtyDatum: (premise) slot B takes a DIFFERENT preset");
        check (! p2.presetDirty(), "dirtyDatum: (premise) B is clean right after its apply");

        p2.switchToSlot (0);                         // back to A, untouched since its apply
        check (p2.currentPresetName() == nameA, "dirtyDatum: (premise) A's name came back");
        check (! p2.presetDirty(),
               "dirtyDatum: A is clean — its own baseline came back with it, not B's");

        // A load cannot restore the datum (a session records WHICH preset a
        // slot holds, never whether it had been edited since), so it must be
        // dropped rather than left describing the preset applied before it.
        check (p2.applyFactoryPreset (2), "dirtyDatum: (premise) apply a different preset first");
        p2.setStateInformation (sessionWithA.getData(), (int) sessionWithA.getSize());
        check (p2.currentPresetName() == nameA, "dirtyDatum: (premise) the load restored A's name");
        check (! p2.presetDirty(),
               "dirtyDatum: a session load drops the datum instead of marking the loaded name");
    }
}

// ---------------------------------------------------------------------------
// The OTHER half of "browsing presets never moves a locked ceiling"
// (DESIGN §4.2): a preset that explicitly NAMES the locked parameter.
//
// `testFactoryPresets` above reaches the defaults half only — no factory table
// names `ceiling` since ADR-0015 made EDM Club's −0.5 override redundant, and
// adding one to the shipped bank to have something to test with would change
// the product to suit the suite. The file-preset path expresses the collision
// directly: a `PARAM` element for `ceiling` IS an override aimed at a locked
// parameter, and both apply paths share the rule (`applyOnePresetValue`'s skip
// and `applyFactoryPreset`'s, the same `internal.ceilingLocked()` test).
//
// Driven through `applyPreset (const XmlElement&, …)` — the overload the
// wrapper's own preset ring uses, so this is the shipped path with the
// filesystem left out, not a private hook opened for the test.
//
// The UNLOCKED pass is the premise that makes the locked one mean something: it
// proves the document really does move the ceiling, so the locked check cannot
// pass by describing a preset that was never going to write it. A second,
// unlocked parameter rides along to prove the apply ran at all rather than
// bailing out early.
static void testALockedCeilingSurvivesAPresetThatNamesIt()
{
    AnabasisAudioProcessor proc;
    auto* ceiling = proc.apvts.getParameter (pid::ceiling);
    auto* knee    = proc.apvts.getParameter (pid::compKnee);
    check (ceiling != nullptr && knee != nullptr,
           "lockedOverride: (premise) the ceiling and the probe parameter exist");
    if (ceiling == nullptr || knee == nullptr)
        return;

    const auto& ceilRange = ceiling->getNormalisableRange();
    const float parked = -6.0f;              // away from BOTH the default and the preset's value
    auto park = [&] { ceiling->setValueNotifyingHost (ceilRange.convertTo0to1 (parked)); };
    auto ceilingNow = [&] { return proc.apvts.getRawParameterValue (pid::ceiling)->load(); };

    juce::XmlElement doc ("AnabasisPreset");
    auto* pc = doc.createNewChildElement ("PARAM");
    pc->setAttribute ("id", pid::ceiling);
    pc->setAttribute ("value", -12.0);
    auto* pk = doc.createNewChildElement ("PARAM");
    pk->setAttribute ("id", pid::compKnee);
    pk->setAttribute ("value", 3.0);

    PresetManager pm (proc.apvts, proc.internalState);
    juce::StringArray mask;

    // Unlocked: the override lands. Without this the locked check below would
    // also pass against a document that names nothing.
    park();
    proc.internalState.state().setProperty (iid::ceilingLock, false, nullptr);
    check (pm.applyPreset (doc, mask), "lockedOverride: (premise) the document applies");
    check (std::abs (ceilingNow() - (-12.0f)) < 0.01f,
           "lockedOverride: (premise) an UNLOCKED ceiling does take the preset's value");

    // Locked: the same document, the same value, skipped.
    park();
    proc.internalState.state().setProperty (iid::ceilingLock, true, nullptr);
    knee->setValueNotifyingHost (knee->getNormalisableRange().convertTo0to1 (9.0f));
    check (pm.applyPreset (doc, mask), "lockedOverride: (premise) the locked apply succeeds");
    check (std::abs (ceilingNow() - parked) < 0.01f,
           "lockedOverride: a preset override naming the locked ceiling does not move it");
    check (std::abs (proc.apvts.getRawParameterValue (pid::compKnee)->load() - 3.0f) < 0.01f,
           "lockedOverride: …while the rest of the same preset still applies");

    proc.internalState.state().setProperty (iid::ceilingLock, false, nullptr);
}

// ---------------------------------------------------------------------------
// The preset WRITER and the dirty MARKER must describe the same set. They share
// `forEachPresetParameter` now, but the invariant is worth pinning from outside
// that function, because until round 52 they walked different collections —
// `apvts.state`'s PARAM children and `getParameters()` — and agreed only because
// APVTS creates one tree child per parameter. Both directions are checked: the
// file's ids against the tree-derived set (the two collections), and every id
// the file carries against the marker (the two consumers).
static void testThePresetWriterAndTheDirtyMarkerCoverTheSameParameters()
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("anabasis-test-presets");
    dir.createDirectory();
    const auto file = dir.getChildFile ("shape-parity.anabasis");

    AnabasisAudioProcessor proc;
    check (proc.getPresetManager().savePreset (file, {}), "shapeParity: (premise) save succeeds");
    const auto xml = juce::XmlDocument::parse (file);
    check (xml != nullptr, "shapeParity: (premise) the file parses");
    if (xml == nullptr)
        return;

    juce::StringArray written;
    for (auto* p : xml->getChildWithTagNameIterator ("PARAM"))
        written.add (p->getStringAttribute ("id"));

    // The OTHER collection, derived independently here so the check does not
    // simply re-run the shared walk.
    juce::StringArray fromTree;
    for (const auto node : proc.apvts.state)
        if (node.hasType ("PARAM"))
            if (const auto id = node.getProperty ("id").toString();
                ! isPresetExcludedParam (id) && proc.apvts.getParameter (id) != nullptr)
                fromTree.add (id);

    auto sorted = [] (juce::StringArray a) { a.sort (true); return a; };
    check (sorted (written) == sorted (fromTree),
           "shapeParity: the file carries exactly the non-excluded parameters the tree holds");
    check (! written.isEmpty(), "shapeParity: (premise) the set is not empty");

    // ORDER too, which is a serialisation guarantee rather than tidiness. The
    // writer's traversal moved to `getParameters()` (registration order) to
    // share the walk with the marker; the tree order it left behind is JUCE's
    // id-keyed one, and that is what every `.anabasis` file already on disk was
    // written in. `forEachPresetParameter` sorts by id so the bytes are
    // unchanged — nothing semantic depends on position (`applyPreset` looks each
    // id up), but re-saving the whole preset bank into a different element order
    // for no reason is a diff nobody asked for, and registration order would
    // churn again on any future layout reshuffle.
    check (written == fromTree,
           "shapeParity: …in the id order existing preset files were written in");

    // …and every one of them is an id the marker watches. A parameter a preset
    // stores but the marker ignores would mean a saved file that differs from
    // the live state while the top bar calls the preset clean.
    // One reported check for the whole sweep, with the offending ids named:
    // ~46 separate checks would drown the suite output and say nothing more.
    // The per-parameter premise (a fresh apply leaves the preset clean) is
    // folded into `notClean` so a broken fixture cannot masquerade as a pass.
    juce::StringArray unnoticed, notClean;
    for (const auto& id : written)
    {
        auto* param = proc.apvts.getParameter (id);
        if (param == nullptr)
        {
            unnoticed.add (id);
            continue;
        }
        if (! proc.applyFactoryPreset (1) || proc.presetDirty())
        {
            notClean.add (id);
            continue;
        }
        // To the far end of the range from wherever the preset left it, so the
        // move is real for every parameter class including the discrete ones.
        param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f);
        if (! proc.presetDirty())
            unnoticed.add (id);
    }
    // The failing ids go INTO the message (kept alive in a local) — a bare
    // "some parameter is unwatched" would send the next reader back to a
    // 46-way bisect.
    const juce::String cleanMsg = "shapeParity: (premise) a fresh apply leaves the preset clean "
                                  "for every id (" + notClean.joinIntoString (", ") + ")";
    const juce::String seenMsg  = "shapeParity: every id a preset FILE stores is one the dirty "
                                  "marker watches (" + unnoticed.joinIntoString (", ") + ")";
    check (notClean.isEmpty(), cleanMsg.toRawUTF8());
    check (unnoticed.isEmpty(), seenMsg.toRawUTF8());

    // THE THIRD CONSUMER of that set: the factory apply's defaults pass. An
    // override table is "defaults + intents", so the pass must reach EVERY
    // non-excluded parameter — one it skipped would keep the value the PREVIOUS
    // preset left, which is the blend-two-presets failure the pass exists to
    // prevent, and audible rather than cosmetic. Until round 57 this walked
    // `apvts.state`'s PARAM children while the other two walked
    // `getParameters()`.
    //
    // Stated as IDEMPOTENCE AGAINST ARBITRARY PRIOR STATE, which is the whole
    // invariant and needs no second copy of the table logic in the test:
    // snapshot what a preset lands, park every non-excluded parameter at the
    // far end of its range, re-apply the SAME preset, and require every one of
    // them back where it was. A parameter the walk misses keeps its parked
    // value and is named in the failure.
    {
        AnabasisAudioProcessor p3;
        check (p3.applyFactoryPreset (1), "shapeParity: (premise) the factory preset applies");

        std::vector<std::pair<juce::String, float>> landed;
        for (const auto& id : written)
            if (auto* param = p3.apvts.getParameter (id))
                landed.emplace_back (id, param->getValue());

        for (auto& [id, v] : landed)
            if (auto* param = p3.apvts.getParameter (id))
                param->setValueNotifyingHost (v > 0.5f ? 0.0f : 1.0f);

        check (p3.applyFactoryPreset (1), "shapeParity: (premise) the same preset re-applies");

        juce::StringArray stranded;
        for (const auto& [id, v] : landed)
            if (auto* param = p3.apvts.getParameter (id);
                param != nullptr && ! juce::exactlyEqual (param->getValue(), v))
                stranded.add (id);
        const juce::String strandedMsg =
            "shapeParity: the factory defaults pass reaches every parameter a preset stores ("
            + stranded.joinIntoString (", ") + ")";
        check (stranded.isEmpty(), strandedMsg.toRawUTF8());
    }
}

// ---------------------------------------------------------------------------
// A preset apply must NOT clear the frozen-trim state, and the reason is the
// opposite of the BASELINE rule below rather than an exception to it. `freeze`
// is preset-EXCLUDED, so an apply never changes whether the slot is frozen; the
// engine's latch is untouched and is still exactly what the DSP is applying, so
// the slot's record of it stays true. `liveFrozenTrims` is only the FALLBACK
// that answers for the staged-but-not-yet-applied window (ADR-0014) — clearing
// it there would make a save report "no latch" while the engine had one staged
// and about to land at the next duck bottom, which loses the vector rather than
// tidying it. BASELINE is different because it is derived from the parameter
// surface the apply replaces; the trims are derived from the audio.
static void testAPresetApplyKeepsTheFrozenLatchItDidNotChange()
{
    AnabasisAudioProcessor proc;

    // A frozen slot, seeded the way a shipped state produces one: Freeze ON on
    // BOTH surfaces (the live root tree and the AB slot copy) with a
    // FROZEN_TRIMS child beside it — the fixture shape `testFrozenSlotRoundTrip`
    // settled, since a freeze-OFF slot serialises no child at all.
    juce::MemoryBlock blank;
    proc.getStateInformation (blank);
    auto root = juce::ValueTree::fromXml (
        *juce::AudioProcessor::getXmlFromBinary (blank.getData(), (int) blank.getSize()));
    auto slot = root.getChildWithName ("AB").getChild (0);
    auto setFreezeOn = [] (juce::ValueTree params)
    {
        if (auto fz = params.getChildWithProperty ("id", "freeze"); fz.isValid())
        {
            fz.setProperty ("value", 1.0, nullptr);
            fz.setProperty ("raw",   1.0, nullptr);
        }
    };
    setFreezeOn (root.getChildWithName ("ANABASIS"));
    setFreezeOn (slot.getChildWithName ("ANABASIS"));
    juce::ValueTree trims ("FROZEN_TRIMS");
    trims.setProperty ("releaseOctaves", 0.25, nullptr);
    trims.setProperty ("stereoLink",     -0.1, nullptr);
    trims.setProperty ("scHpfHz",        3.0,  nullptr);
    trims.setProperty ("dynTiltDb",      0.4,  nullptr);
    slot.appendChild (trims, nullptr);
    juce::MemoryBlock in;
    juce::AudioProcessor::copyXmlToBinary (*root.createXml(), in);
    proc.setStateInformation (in.getData(), (int) in.getSize());

    auto savedTrims = [&proc]
    {
        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        auto r = juce::ValueTree::fromXml (
            *juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize()));
        return r.getChildWithName ("AB").getChild (proc.activeSlotIndex())
                .getChildWithName ("FROZEN_TRIMS");
    };
    check (savedTrims().isValid(), "frozenKept: (premise) the frozen slot round-trips its vector");
    check (proc.apvts.getRawParameterValue (pid::freeze)->load() >= 0.5f,
           "frozenKept: (premise) the loaded surface is frozen");

    check (proc.applyFactoryPreset (1), "frozenKept: (premise) the factory preset applies");
    check (proc.apvts.getRawParameterValue (pid::freeze)->load() >= 0.5f,
           "frozenKept: a preset apply never moves Freeze — it is preset-excluded");
    const auto after = savedTrims();
    check (after.isValid()
            && std::abs ((double) after.getProperty ("releaseOctaves") - 0.25) < 1.0e-6
            && std::abs ((double) after.getProperty ("dynTiltDb") - 0.4) < 1.0e-6,
           "frozenKept: the latched vector survives a FACTORY apply unchanged");
}

// ---------------------------------------------------------------------------
// BASELINE is runtime-only §5.5 state and a preset file cannot store one, so
// neither apply path may leave a baseline standing after replacing the
// parameter surface it was captured against. The FACTORY path has always
// cleared it ("defaults-based: no macro baseline survives"); the FILE path did
// not, so a preset loaded over a state that carried a baseline kept the
// previous state's vector — and it travelled, into the SLOT tree that A/B
// swaps and `getStateInformation` writes. The two are the same conceptual
// operation and now leave the same internal state.
static void testAPresetApplyDropsTheMacroBaselineOnBothPaths()
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("anabasis-test-presets");
    dir.createDirectory();
    const auto file = dir.getChildFile ("baseline-drop.anabasis");
    {
        AnabasisAudioProcessor src;
        auto* gain = src.apvts.getParameter (pid::limGain);
        gain->setValueNotifyingHost (gain->getNormalisableRange().convertTo0to1 (2.0f));
        check (src.getPresetManager().savePreset (file, {}),
               "baselineDrop: (premise) a preset file exists");
    }

    // Nothing in this build WRITES a baseline yet (P4 owns the capture), so it
    // is seeded the only way a shipped state can carry one: through a loaded
    // session whose slot has the child.
    auto seeded = [] (AnabasisAudioProcessor& p)
    {
        juce::MemoryBlock blank;
        p.getStateInformation (blank);
        auto root = juce::ValueTree::fromXml (
            *juce::AudioProcessor::getXmlFromBinary (blank.getData(), (int) blank.getSize()));
        auto slot = root.getChildWithName ("AB").getChild (0);
        juce::ValueTree baseline ("BASELINE");
        baseline.setProperty ("limGain", 3.5, nullptr);
        slot.appendChild (baseline, nullptr);
        juce::MemoryBlock in;
        juce::AudioProcessor::copyXmlToBinary (*root.createXml(), in);
        p.setStateInformation (in.getData(), (int) in.getSize());
    };
    auto slotHasBaseline = [] (AnabasisAudioProcessor& p)
    {
        juce::MemoryBlock mb;
        p.getStateInformation (mb);
        auto root = juce::ValueTree::fromXml (
            *juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize()));
        return root.getChildWithName ("AB").getChild (p.activeSlotIndex())
                   .getChildWithName ("BASELINE").isValid();
    };

    AnabasisAudioProcessor viaFile;
    seeded (viaFile);
    check (slotHasBaseline (viaFile), "baselineDrop: (premise) the seeded baseline survives a load");
    check (viaFile.applyPresetFile (file), "baselineDrop: (premise) the file applies");
    check (! slotHasBaseline (viaFile), "baselineDrop: a FILE preset apply drops the macro baseline");

    AnabasisAudioProcessor viaFactory;
    seeded (viaFactory);
    check (slotHasBaseline (viaFactory), "baselineDrop: (premise) seeded for the factory path too");
    check (viaFactory.applyFactoryPreset (1), "baselineDrop: (premise) the factory preset applies");
    check (! slotHasBaseline (viaFactory),
           "baselineDrop: …and a FACTORY apply does the same, as it always did");
}

// ---------------------------------------------------------------------------
// The "edited" mark answers one question — does the live state differ from the
// named preset IN A WAY A PRESET FILE COULD RECORD? — so the datum it compares
// has to be preset content. It used to be the SLOT tree, which additionally
// carries the view tier, Freeze, the exact-`raw` attribute, BASELINE and
// FROZEN_TRIMS; a `.anabasis` file stores none of those, so each stimulus below
// lit a mark that no amount of re-saving could honestly clear.
static void testTheDirtyMarkerMeasuresOnlyWhatAPresetCanCarry()
{
    AnabasisAudioProcessor proc;
    auto& apvts = proc.apvts;
    check (proc.applyFactoryPreset (1), "dirtyShape: (premise) a preset is loaded");
    check (! proc.presetDirty(), "dirtyShape: (premise) clean right after the apply");

    // View tier. Switching Simple↔Advanced is the user looking at the plugin
    // differently, not editing the preset — and `isPresetExcludedParam` already
    // says so on the SAVE side, which is precisely the asymmetry: the file
    // omitted it while the marker counted it.
    auto* advanced = apvts.getParameter (pid::advancedMode);
    advanced->setValueNotifyingHost (advanced->getValue() >= 0.5f ? 0.0f : 1.0f);
    check (! proc.presetDirty(), "dirtyShape: a view-tier move is not a preset edit");

    // Freeze — excluded by the same shared predicate, and its ON state grows a
    // FROZEN_TRIMS child on the slot tree as well, so the old compare had two
    // independent reasons to call this an edit.
    auto* freeze = apvts.getParameter (pid::freeze);
    freeze->setValueNotifyingHost (1.0f);
    check (! proc.presetDirty(), "dirtyShape: engaging Freeze is not a preset edit");
    freeze->setValueNotifyingHost (0.0f);
    check (! proc.presetDirty(), "dirtyShape: …nor is releasing it again");

    // A mid-step `raw` move on a discrete parameter: the host-session contract
    // keeps it exactly (ADR-0007), the PRESET contract stores the snapped value,
    // and this move does not change that value. The slot compare saw the `raw`
    // attribute differ and called it an edit.
    auto* model = apvts.getParameter (pid::colourModel);
    const auto& mr = model->getNormalisableRange();
    const float snappedBefore = mr.snapToLegalValue (mr.convertFrom0to1 (model->getValue()));
    model->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, model->getValue() + 0.02f));
    if (juce::exactlyEqual (mr.snapToLegalValue (mr.convertFrom0to1 (model->getValue())),
                            snappedBefore))
        check (! proc.presetDirty(),
               "dirtyShape: a mid-step raw move that snaps to the same value is not an edit");

    // …and the mark still fires for something a preset DOES carry, or every
    // check above is satisfied by a marker that is simply never dirty.
    auto* knee = apvts.getParameter (pid::compKnee);
    knee->setValueNotifyingHost (knee->getNormalisableRange().convertTo0to1 (1.0f));
    check (proc.presetDirty(), "dirtyShape: a stored parameter still marks it edited");

    // The detach mask is preset content too (§5.3 travels in the file), so it
    // must be on the other side of the line from the view tier.
    AnabasisAudioProcessor p2;
    check (p2.applyFactoryPreset (1), "dirtyShape: (premise) second instance takes a preset");
    check (! p2.presetDirty(), "dirtyShape: (premise) clean");
    auto* g = p2.apvts.getParameter (pid::limGain);
    g->beginChangeGesture();
    g->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, g->getValue() + 0.1f));
    g->endChangeGesture();
    check (! p2.detachMask().isEmpty(), "dirtyShape: (premise) the gesture detached it");
    check (p2.presetDirty(), "dirtyShape: a §5.3 detach is a preset edit");
}

// ---------------------------------------------------------------------------
// §7 per-slot undo: the undo unit is the five-field SLOT tree, coalescing is
// gesture-gated, automation folds silently, a preset apply brackets as one
// step, stacks are per slot and a session load clears them. The detach-mask
// assertion is the §7 WIDENING rationale made mechanical: undoing an edit
// must restore the value AND its detach bit together — with the narrow unit
// the value returned and stayed badged as edited.
static void testUndoIsPerSlotGestureCoalescedAndMaskWide()
{
    AnabasisAudioProcessor proc;
    auto& apvts = proc.apvts;
    auto* limGain = apvts.getParameter (pid::limGain);
    auto limGainValue = [&] { return apvts.getRawParameterValue (pid::limGain)->load(); };
    auto norm = [&] (float v)
    { return limGain->getNormalisableRange().convertTo0to1 (v); };

    check (! proc.canUndo() && ! proc.canRedo(), "undo: fresh instance has no history");

    // 1. One DRAG = one step, however many changes it contains.
    const float before = limGainValue();
    limGain->beginChangeGesture();
    limGain->setValueNotifyingHost (norm (2.0f));
    limGain->setValueNotifyingHost (norm (4.0f));
    limGain->setValueNotifyingHost (norm (6.0f));
    limGain->endChangeGesture();
    proc.flushPendingDetach();
    check (proc.canUndo(), "undo: a completed gesture pushed a step");
    proc.undo();
    check (std::abs (limGainValue() - before) < 1.0e-4f,
           "undo: one undo unwinds the whole drag");
    check (proc.canRedo(), "undo: the unwound step is redoable");
    proc.redo();
    check (std::abs (limGainValue() - 6.0f) < 1.0e-3f, "undo: redo re-lands the drag");

    // 2. AUTOMATION (ungestured) folds silently — no step of its own: the
    //    single undo left from step 1 unwinds the drag AND the folded
    //    automation edit together, leaving no further history.
    limGain->setValueNotifyingHost (norm (3.0f));
    proc.undo();
    check (! proc.canUndo(), "undo: automation pushed no step of its own");
    proc.redo();

    // 3. The MASK travels with the value (the widening rationale). Start from
    //    a clean mask (the step-1 drag detached limGain; reset-to-macro is
    //    the sanctioned clear and pushes no step of its own), then a gestured
    //    edit detaches again, and ONE undo reverts value and bit together.
    proc.resetToMacro();
    proc.flushPendingDetach();
    check (proc.detachMask().isEmpty(), "undo: (premise) mask cleared for the round");
    limGain->beginChangeGesture();
    limGain->setValueNotifyingHost (norm (1.5f));
    limGain->endChangeGesture();
    proc.flushPendingDetach();
    check (proc.detachMask().contains ("limGain"), "undo: (premise) the edit detached");
    proc.undo();
    proc.flushPendingDetach();
    check (proc.detachMask().isEmpty(),
           "undo: the detach bit reverted WITH the value (the widened StateSet)");

    // 4. Stacks are PER SLOT: slot B starts with no history, and returning to
    //    A finds A's history intact.
    limGain->beginChangeGesture();
    limGain->setValueNotifyingHost (norm (5.0f));
    limGain->endChangeGesture();
    proc.flushPendingDetach();
    check (proc.canUndo(), "undo: (premise) slot A has history");
    proc.switchToSlot (1);
    check (! proc.canUndo(), "undo: slot B has its own empty stack");
    proc.switchToSlot (0);
    check (proc.canUndo(), "undo: slot A's history survived the round trip");

    // 5. A preset apply is ONE bracketed step; an unreadable file pushes none.
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory);
    auto good = dir.getChildFile ("anabasis_undo_test.anabasis");
    proc.savePresetFile (good);
    limGain->beginChangeGesture();
    limGain->setValueNotifyingHost (norm (9.0f));
    limGain->endChangeGesture();
    proc.flushPendingDetach();
    check (proc.applyPresetFile (good), "undo: (premise) the preset applies");
    proc.undo();                                   // unwinds the APPLY, one step
    check (std::abs (limGainValue() - 9.0f) < 1.0e-3f,
           "undo: one undo unwinds exactly the preset apply");
    auto bad = dir.getChildFile ("anabasis_undo_bad.anabasis");
    bad.replaceWithText ("not xml at all");
    while (proc.canUndo())                     // drain: the next check needs the
        proc.undo();                           // stack EMPTY, not merely unchanged
    check (! proc.applyPresetFile (bad), "undo: (premise) the bad file fails");
    check (! proc.canUndo(),
           "undo: a failed parse cost no undo step (parse before the bracket)");

    // The gate's other half, and the one an is-it-XML test let through: a
    // WELL-FORMED document with a foreign root parses fine, so it opened the
    // bracket and then applied nothing — an undo step for a guaranteed no-op.
    // The gate reads through PresetManager's own predicate, so the two cannot
    // disagree about what "readable" means.
    auto foreign = dir.getChildFile ("anabasis_undo_foreign.anabasis");
    foreign.replaceWithText ("<SomeOtherPlugin><PARAM id=\"limGain\" value=\"3\"/></SomeOtherPlugin>");
    check (! proc.applyPresetFile (foreign), "undo: (premise) a foreign root is refused");
    check (! proc.canUndo(),
           "undo: well-formed XML with a foreign root costs no undo step either");
    good.deleteFile();
    bad.deleteFile();
    foreign.deleteFile();

    // 6. A session LOAD clears the stacks (never serialized).
    juce::MemoryBlock state;
    proc.getStateInformation (state);
    proc.setStateInformation (state.getData(), (int) state.getSize());
    check (! proc.canUndo() && ! proc.canRedo(),
           "undo: a session load starts a fresh history");
}

// ---------------------------------------------------------------------------
// §7 undo, the gesture bookkeeping's SYMMETRY: a begin is counted only on the
// message thread (an off-thread gesture folds into the automation path by
// design), so the matching end must be ignored too. VST3 gesture threading is
// host-defined, and a host that delivers begin off-thread and end on it made a
// bare `--openGestureCount` close a DIFFERENT, still-open drag — pushing that
// drag's step mid-gesture and clearing the snapshot, so its real end pushed
// nothing and the drag became unundoable.
//
// The stimulus is the real thing rather than a synthesised callback: the
// foreign begin runs on a worker thread, which is exactly the condition the
// wrapper discriminates on, and it leaves JUCE's own gesture bookkeeping
// balanced so the later end travels the normal path.
static void testAGestureEndWithoutACountedBeginIsIgnored()
{
    AnabasisAudioProcessor proc;
    auto* limGain = proc.apvts.getParameter (pid::limGain);
    auto* foreign = proc.apvts.getParameter (pid::ceiling);
    const float before = proc.apvts.getRawParameterValue (pid::limGain)->load();

    std::thread offThread ([foreign] { foreign->beginChangeGesture(); });
    offThread.join();                          // uncounted: not the message thread

    limGain->beginChangeGesture();             // the real drag, counted
    limGain->setValueNotifyingHost (
        limGain->getNormalisableRange().convertTo0to1 (7.0f));

    foreign->endChangeGesture();               // …and its end arrives here
    check (! proc.canUndo(),
           "gestureSymmetry: an end whose begin was never counted does not close the open drag");

    limGain->endChangeGesture();               // the drag's own end
    proc.flushPendingDetach();
    check (proc.canUndo(), "gestureSymmetry: the drag's own end still pushes its step");
    proc.undo();
    check (std::abs (proc.apvts.getRawParameterValue (pid::limGain)->load() - before) < 1.0e-3f,
           "gestureSymmetry: and that step is the whole drag");

    // The OPPOSITE asymmetry, and the one that used to be permanent: a begin
    // counted on the message thread whose END arrives off it. Guarding the
    // bookkeeping behind the thread test leaked that drag for ever — the mask
    // never returned to empty, so no later drag on ANY control could push a
    // step again and undo was dead for the rest of the session. The end now
    // clears its bit on whichever thread it arrives on; only the ValueTree
    // work stays message-thread-gated, so the lost step is that one drag's
    // (the documented automation-path degradation), not every future one.
    {
        AnabasisAudioProcessor p2;
        auto* g = p2.apvts.getParameter (pid::limGain);
        g->beginChangeGesture();
        g->setValueNotifyingHost (g->getNormalisableRange().convertTo0to1 (5.0f));
        std::thread endOffThread ([g] { g->endChangeGesture(); });
        endOffThread.join();
        p2.flushPendingDetach();
        check (! p2.canUndo(),
               "gestureSymmetry: (premise) an off-thread end pushes nothing — the automation rule");

        const float mid = p2.apvts.getRawParameterValue (pid::limGain)->load();
        g->beginChangeGesture();               // a NEW, entirely ordinary drag
        g->setValueNotifyingHost (g->getNormalisableRange().convertTo0to1 (11.0f));
        g->endChangeGesture();
        p2.flushPendingDetach();
        check (p2.canUndo(),
               "gestureSymmetry: the off-thread end did not strand the drag — undo still works after it");
        p2.undo();
        check (std::abs (p2.apvts.getRawParameterValue (pid::limGain)->load() - mid) < 1.0e-3f,
               "gestureSymmetry: and the recovered step is the new drag, not the lost one");
    }
}

// ---------------------------------------------------------------------------
// §5.3's precedence rule when a detach and a re-engage land in the SAME drain:
// "the next macro-knob gesture re-engages ALL detached params", so the macro
// gesture wins. That means the detach bits are applied FIRST and the re-engage
// clears over them — the opposite order leaves the parameter detached through
// the very gesture that is supposed to re-engage it.
//
// The two can only coexist off the message thread: on it, `drainDetachBitsSoon`
// drains synchronously per callback, so each lands alone. The stimulus is
// therefore the real asymmetry — the managed drag is delivered from a worker
// thread (uncounted, undrained), and the macro gesture then arrives on the
// message thread and drains both at once.
static void testAMacroGestureWinsADetachRacingItInOneDrain()
{
    AnabasisAudioProcessor proc;
    auto* limGain = proc.apvts.getParameter (pid::limGain);      // managed (§5.5)
    auto* loudness = proc.apvts.getParameter (pid::loudness);    // a macro

    // A gestured managed edit, entirely off the message thread: the gesture
    // bracket is what makes it a DETACH (§5.3 keys on gesture-bracketed, on
    // any thread), and being off-thread is what leaves the bit undrained.
    std::thread offThread ([limGain]
    {
        limGain->beginChangeGesture();
        limGain->setValueNotifyingHost (
            limGain->getNormalisableRange().convertTo0to1 (7.0f));
        limGain->endChangeGesture();
    });
    offThread.join();
    check (proc.detachMask().isEmpty(),
           "drainOrder: (premise) the off-thread detach has not been drained yet");

    // …and now the macro gesture, on the message thread, which drains both.
    loudness->beginChangeGesture();
    loudness->endChangeGesture();
    proc.flushPendingDetach();

    check (proc.detachMask().isEmpty(),
           "drainOrder: a macro gesture re-engages a detach that raced it into the same drain");
    // Asserts the MASK only, on purpose. This gesture moved no value, so
    // nothing armed a mapping and limGain still holds the user's off-curve
    // value while reading as re-engaged — whether §5.3 wants a gesture-driven
    // re-engage to also re-land the curve (as `resetToMacro()` does) is the
    // open question recorded as KI-007 item 8. Pinning either reading here
    // would silently decide it, so the check stops where the rule is settled.
}

// ---------------------------------------------------------------------------
// A RESTORE replaces the detach mask, so it must drop the staged bits too. An
// off-thread gestured edit leaves its bit un-drained for up to one 30 ms tick;
// if a slot switch (or preset apply, or session load) lands inside that window,
// the tick afterwards would add the id to the mask the restore just installed —
// the restored slot coming up with a detach its own tree never carried, which
// is precisely the carry-over the restore paths clear the mask to prevent.
static void testARestoreDropsStagedDetachBits()
{
    AnabasisAudioProcessor proc;
    auto* limGain = proc.apvts.getParameter (pid::limGain);

    std::thread offThread ([limGain]
    {
        limGain->beginChangeGesture();
        limGain->setValueNotifyingHost (
            limGain->getNormalisableRange().convertTo0to1 (7.0f));
        limGain->endChangeGesture();
    });
    offThread.join();
    check (proc.detachMask().isEmpty(),
           "restoreDrop: (premise) the off-thread detach is staged, not yet drained");

    proc.switchToSlot (1);          // slot B's tree carries no detach mask
    proc.flushPendingDetach();      // …and the tick that follows must add nothing
    check (proc.detachMask().isEmpty(),
           "restoreDrop: a slot switch drops the staged bit instead of stamping it on the new slot");

    // EVERY path that replaces the mask, not just the slot switch: the rule was
    // written at one of its five sites and missed the other four, so each is
    // exercised here through the one function they now share.
    auto stageAnOffThreadDetach = [] (AnabasisAudioProcessor& p)
    {
        auto* g = p.apvts.getParameter (pid::limGain);
        std::thread t ([g]
        {
            g->beginChangeGesture();
            g->setValueNotifyingHost (g->getNormalisableRange().convertTo0to1 (5.0f));
            g->endChangeGesture();
        });
        t.join();
    };

    {   // a FACTORY preset ("factory presets load nothing pre-detached")
        AnabasisAudioProcessor p2;
        stageAnOffThreadDetach (p2);
        check (p2.applyFactoryPreset (1), "restoreDrop: (premise) the factory preset applies");
        p2.flushPendingDetach();
        check (p2.detachMask().isEmpty(),
               "restoreDrop: a factory preset comes up with no detach the table did not carry");
    }
    {   // a SESSION LOAD — which rebuilds the mask inline, not via applySlotToLive
        AnabasisAudioProcessor p3;
        juce::MemoryBlock clean;
        p3.getStateInformation (clean);
        stageAnOffThreadDetach (p3);
        p3.setStateInformation (clean.getData(), (int) clean.getSize());
        p3.flushPendingDetach();
        check (p3.detachMask().isEmpty(),
               "restoreDrop: a session load comes up with no detach the session did not carry");
    }
    {   // and RESET-TO-MACRO, whose whole purpose is to re-engage everything
        AnabasisAudioProcessor p4;
        stageAnOffThreadDetach (p4);
        p4.resetToMacro();
        p4.flushPendingDetach();
        check (p4.detachMask().isEmpty(),
               "restoreDrop: reset-to-macro is not undone by a bit staged just before it");
    }
}

// ---------------------------------------------------------------------------
// The drain tick's own ORDER, which is a level above the precedence
// `handleAsyncUpdate` applies internally: the wrapper's bits decide the detach
// mask, and the mapping pass reads that mask to know which managed parameters
// it may write. Mapping first meant a macro gesture and its value change
// arriving together off the message thread — the only way both reach one tick
// — mapped while the parameter was STILL detached (so the pass skipped it) and
// cleared the mask afterwards: the parameter read as re-engaged while holding
// the user's off-curve value, with nothing left to re-arm the mapping.
static void testTheDrainTickReEngagesBeforeItMaps()
{
    AnabasisAudioProcessor proc;
    auto* limGain  = proc.apvts.getParameter (pid::limGain);
    auto* loudness = proc.apvts.getParameter (pid::loudness);
    auto lim = [&] { return proc.apvts.getRawParameterValue (pid::limGain)->load(); };

    // Detach limGain the ordinary way (message thread), so the mask really
    // holds it when the tick runs.
    limGain->beginChangeGesture();
    limGain->setValueNotifyingHost (limGain->getNormalisableRange().convertTo0to1 (2.0f));
    limGain->endChangeGesture();
    proc.flushPendingDetach();
    check (proc.detachMask().contains (pid::limGain),
           "tickOrder: (premise) limGain is detached before the tick");

    // The macro gesture AND its value change, both off the message thread, so
    // neither drains on arrival and both are waiting for one tick.
    std::thread offThread ([loudness]
    {
        loudness->beginChangeGesture();
        loudness->setValueNotifyingHost (
            loudness->getNormalisableRange().convertTo0to1 (70.0f));
        loudness->endChangeGesture();
    });
    offThread.join();
    check (std::abs (lim() - 2.0f) < 1.0e-3f,
           "tickOrder: (premise) nothing has drained yet — limGain still the user's value");

    proc.getMacroEngine().drainTick();          // exactly what the 30 ms timer does

    check (proc.detachMask().isEmpty(), "tickOrder: the macro gesture re-engaged the mask");
    check (std::abs (lim() - macro_curves::limGainDb (0.7f)) < 0.05f,
           "tickOrder: …and the SAME tick's mapping landed the curve on it");
}

// The POSTED half of the same contract. `MacroEngine::parameterChanged` posts
// `triggerAsyncUpdate()` when a macro id changes on the message thread, and for
// one revision `handleAsyncUpdate` ran `drainPendingMapping()` ALONE while the
// timer ran the wrapper's drain first — so the two entry points into "map now"
// disagreed about whether the detach mask was up to date. The stimulus that
// isolates it is host automation of a macro (message thread, NO gesture, so
// nothing re-engages) racing a gestured managed edit delivered off-thread
// (bit staged, `drainDetachBitsSoon` deliberately refusing to post).
static void testThePostedDrainAlsoTakesTheWrapperBitsFirst()
{
    AnabasisAudioProcessor proc;
    auto* limGain  = proc.apvts.getParameter (pid::limGain);
    auto* loudness = proc.apvts.getParameter (pid::loudness);
    auto lim = [&] { return proc.apvts.getRawParameterValue (pid::limGain)->load(); };

    std::thread offThread ([limGain]
    {
        limGain->beginChangeGesture();
        limGain->setValueNotifyingHost (limGain->getNormalisableRange().convertTo0to1 (2.0f));
        limGain->endChangeGesture();
    });
    offThread.join();
    check (proc.detachMask().isEmpty(),
           "postedDrain: (premise) the off-thread detach bit is staged, not yet in the mask");
    check (std::abs (lim() - 2.0f) < 1.0e-3f,
           "postedDrain: (premise) limGain holds the user's value");

    // Ungestured macro write on the message thread: the one stimulus that
    // posts. No gesture means no §5.3 re-engage, so limGain must stay the
    // user's — and it only does if the posted handler drains the wrapper's
    // staged bit BEFORE its own mapping consults the mask.
    loudness->setValueNotifyingHost (loudness->getNormalisableRange().convertTo0to1 (70.0f));
    proc.getMacroEngine().handleAsyncUpdate();     // what the message queue would run

    check (proc.detachMask().contains (pid::limGain),
           "postedDrain: the posted handler took the wrapper's staged bit first");
    check (std::abs (lim() - 2.0f) < 1.0e-3f,
           "postedDrain: …so the SAME handler's mapping skipped limGain");
    check (std::abs (proc.apvts.getRawParameterValue (pid::compThreshold)->load()
                       - macro_curves::compThresholdDb (0.7f)) < 0.05f,
           "postedDrain: the un-detached managed parameters still followed the macro");

    // The THIRD entry point, closing the set: `flushPendingMapping` (reset-to-
    // macro, and the headless tests' own flush). Same stimulus, a second
    // managed parameter, so a revision that fixes two of the three routes and
    // leaves the third calling `drainPendingMapping()` alone still fails here.
    auto* clipDrive = proc.apvts.getParameter (pid::clipDrive);
    std::thread offThread2 ([clipDrive]
    {
        clipDrive->beginChangeGesture();
        clipDrive->setValueNotifyingHost (clipDrive->getNormalisableRange().convertTo0to1 (6.0f));
        clipDrive->endChangeGesture();
    });
    offThread2.join();
    check (! proc.detachMask().contains (pid::clipDrive),
           "postedDrain: (premise) the second bit is staged too, not yet in the mask");

    loudness->setValueNotifyingHost (loudness->getNormalisableRange().convertTo0to1 (40.0f));
    proc.getMacroEngine().flushPendingMapping();

    check (proc.detachMask().contains (pid::clipDrive),
           "postedDrain: the flush ran the same sequence — staged bit first");
    check (std::abs (proc.apvts.getRawParameterValue (pid::clipDrive)->load() - 6.0f) < 1.0e-3f,
           "postedDrain: …and its mapping skipped the freshly detached clipDrive");
}

// Three MECHANICAL invariants this build states in prose and now enforces in
// code, one test each because each has exactly one observable consequence.
static void testTeardownAndReengageInvariants()
{
    // (1) TEARDOWN. `drainTick`, `flushPendingMapping` and `refreshMapping` are
    // public, so "nothing drains after stopDraining" used to be a rule a caller
    // had to remember — and after `~AnabasisAudioProcessor` has called it, the
    // members `onDrainTick` reaches are already destroyed. A one-way latch now
    // enforces it for every trigger at once, since all of them route through
    // `drainTick`.
    {
        AnabasisAudioProcessor proc;
        int ticks = 0;
        proc.getMacroEngine().onDrainTick = [&ticks] { ++ticks; };
        proc.getMacroEngine().drainTick();
        check (ticks == 1, "teardown: (premise) a drain reaches the owner while draining");
        proc.getMacroEngine().stopDraining();
        const bool tick  = proc.getMacroEngine().drainTick();
        const bool flush = proc.getMacroEngine().flushPendingMapping();
        const bool fresh = proc.getMacroEngine().refreshMapping();
        check (ticks == 1, "teardown: no trigger reaches the owner after stopDraining");
        check (! tick && ! flush && ! fresh,
               "teardown: …and each trigger REPORTS the suppression instead of returning silently");
    }

    // (2) §5.3 RE-ENGAGE. Round 30 settled that a re-engage the curve-landing
    // pass never sees leaves a parameter reading as re-engaged while holding
    // the user's off-curve value. A macro gesture that moves NOTHING is the
    // remaining instance: it clears the mask, and until now armed no mapping.
    {
        AnabasisAudioProcessor proc;
        auto* limGain  = proc.apvts.getParameter (pid::limGain);
        auto* loudness = proc.apvts.getParameter (pid::loudness);
        loudness->setValueNotifyingHost (loudness->getNormalisableRange().convertTo0to1 (60.0f));
        proc.getMacroEngine().flushPendingMapping();

        limGain->beginChangeGesture();
        limGain->setValueNotifyingHost (limGain->getNormalisableRange().convertTo0to1 (2.0f));
        limGain->endChangeGesture();
        proc.flushPendingDetach();
        check (proc.detachMask().contains (pid::limGain),
               "reengage: (premise) the gestured edit detached limGain");
        check (std::abs (proc.apvts.getRawParameterValue (pid::limGain)->load() - 2.0f) < 1.0e-3f,
               "reengage: (premise) …and it holds the user's off-curve value");

        // The gesture with NO value change between begin and end.
        loudness->beginChangeGesture();
        loudness->endChangeGesture();
        proc.getMacroEngine().flushPendingMapping();

        check (proc.detachMask().isEmpty(), "reengage: the macro gesture cleared the mask");
        check (std::abs (proc.apvts.getRawParameterValue (pid::limGain)->load()
                           - macro_curves::limGainDb (0.6f)) < 0.05f,
               "reengage: …and the SAME gesture re-landed the curve on it");
    }

    // (3) COPY A→B — ADR-0018 (the sibling's #12 rule): the Copy is an undo
    // step ON THE DESTINATION whose older history is KEPT beneath it. One
    // undo on the copied-into slot reverts the Copy; the next walks the
    // slot's own pre-copy history. Entries are absolute snapshots, which is
    // what makes the kept history coherent. (0.1.0 cleared both stacks here;
    // that behaviour and its rationale are superseded — see the ADR.)
    {
        AnabasisAudioProcessor proc;
        auto* drive = proc.apvts.getParameter (pid::clipDrive);
        const auto driveDb = [&] { return proc.apvts.getRawParameterValue (pid::clipDrive)->load(); };
        proc.switchToSlot (1);                       // edit B so it HAS a history
        drive->beginChangeGesture();
        drive->setValueNotifyingHost (drive->getNormalisableRange().convertTo0to1 (7.0f));
        drive->endChangeGesture();
        check (proc.canUndo(), "copyUndo: (premise) slot B has an undo step");
        proc.switchToSlot (0);
        drive->beginChangeGesture();                 // give A a distinct value to copy
        drive->setValueNotifyingHost (drive->getNormalisableRange().convertTo0to1 (3.0f));
        drive->endChangeGesture();
        proc.copySlotToOther();                      // A → B
        proc.switchToSlot (1);
        check (std::abs (driveDb() - 3.0f) < 1.0e-3f,
               "copyUndo: (premise) the copy itself landed on B");
        check (proc.canUndo(), "copyUndo: the Copy is an undo step on the destination");
        proc.undo();
        check (std::abs (driveDb() - 7.0f) < 1.0e-3f,
               "copyUndo: one undo on the destination reverts the Copy");
        check (proc.canUndo(), "copyUndo: …and the destination's OWN history survives beneath it");
        proc.undo();
        check (std::abs (driveDb() - 0.0f) < 1.0e-3f,
               "copyUndo: the second undo walks the pre-copy history");
        check (proc.canRedo(), "copyUndo: the redo line rebuilt by undoing");
        proc.redo();
        proc.redo();
        check (std::abs (driveDb() - 3.0f) < 1.0e-3f,
               "copyUndo: redo twice re-lands the copied state");
        // The SOURCE slot's history is untouched by a Copy: A had one step
        // (the 3 dB edit) and still has exactly that.
        proc.switchToSlot (0);
        check (proc.canUndo(), "copyUndo: the source slot's history is untouched");
    }

    // (3b) COPY UNDO DOES NOT MOVE THE VIEW — ADR-0018 §Consequences: "Copy
    // never moves the view", and the undo of a Copy is part of Copy's
    // behaviour. The Copy entry is the ONE undo entry whose slot tree was not
    // captured at the moment of its step (it is `storedSlot`, frozen since the
    // last A/B switch), so it is the one entry whose `advancedMode` can be
    // stale — and undo is the one adoption path that adopts `advancedMode`.
    // The sequence below is the minimum that exposes it: the ADV toggle has to
    // land AFTER the switch that froze `storedSlot` and BEFORE the Copy.
    {
        AnabasisAudioProcessor proc;
        auto* adv   = proc.apvts.getParameter (pid::advancedMode);
        auto* drive = proc.apvts.getParameter (pid::clipDrive);
        const auto advOn = [&] { return proc.apvts.getRawParameterValue (pid::advancedMode)->load() >= 0.5f; };

        proc.switchToSlot (1);                       // give slot B a distinct value
        drive->beginChangeGesture();
        drive->setValueNotifyingHost (drive->getNormalisableRange().convertTo0to1 (7.0f));
        drive->endChangeGesture();
        proc.switchToSlot (0);                       // …and FREEZE it into storedSlot, ADV off

        adv->beginChangeGesture();                   // the toggle the frozen snapshot predates
        adv->setValueNotifyingHost (1.0f);
        adv->endChangeGesture();
        check (advOn(), "copyUndoView: (premise) the user is in Advanced when they Copy");

        proc.copySlotToOther();                      // A → B
        proc.switchToSlot (1);
        check (advOn(), "copyUndoView: (premise) the A/B switch left the view alone");
        proc.undo();                                 // revert the Copy on the destination
        check (advOn(),
               "copyUndoView: undoing a Copy reverts the sound and leaves the view mode alone");
        check (std::abs (proc.apvts.getRawParameterValue (pid::clipDrive)->load() - 7.0f) < 1.0e-3f,
               "copyUndoView: …and the SOUND half of that undo still landed");
    }

    // (3c) A COPY THAT CHANGES NOTHING MINTS NOTHING. After the first Copy the
    // destination already holds the live state, so a second press with no edit
    // between would push an entry restoring exactly what it replaces — one
    // Undo press that visibly does nothing, which is the dead step ADR-0018
    // removed from the gesture path. Same change test, same answer.
    {
        AnabasisAudioProcessor proc;
        auto* drive = proc.apvts.getParameter (pid::clipDrive);
        const auto driveDb = [&] { return proc.apvts.getRawParameterValue (pid::clipDrive)->load(); };

        drive->beginChangeGesture();                 // one real edit in A
        drive->setValueNotifyingHost (drive->getNormalisableRange().convertTo0to1 (3.0f));
        drive->endChangeGesture();

        proc.copySlotToOther();                      // A → B: a real change to B
        proc.copySlotToOther();                      // …and again, with nothing between
        proc.copySlotToOther();

        proc.switchToSlot (1);
        check (std::abs (driveDb() - 3.0f) < 1.0e-3f,
               "copyNoOp: (premise) the copied state is on the destination");
        check (proc.canUndo(), "copyNoOp: the FIRST Copy is still a step");
        proc.undo();
        check (std::abs (driveDb() - 0.0f) < 1.0e-3f,
               "copyNoOp: one undo reverts to the pre-copy state — the repeats added no steps");
        check (! proc.canUndo(), "copyNoOp: …and there is nothing left to undo on that slot");
    }

    // (3d) …AND IT DOES NOT CLEAR THE DESTINATION'S REDO LINE. Redo is
    // invalidated by a new ACTION, and a Copy that changes nothing is not one —
    // the gesture path likewise clears no redo when its diff is empty. Staged
    // so the destination (A) is back at its default with a redo entry waiting,
    // and the source (B) holds that same default, which makes the Copy a no-op.
    {
        AnabasisAudioProcessor proc;
        auto* drive = proc.apvts.getParameter (pid::clipDrive);
        const auto driveDb = [&] { return proc.apvts.getRawParameterValue (pid::clipDrive)->load(); };

        drive->beginChangeGesture();
        drive->setValueNotifyingHost (drive->getNormalisableRange().convertTo0to1 (3.0f));
        drive->endChangeGesture();
        proc.undo();                                 // A back to default, redo line armed
        check (proc.canRedo() && std::abs (driveDb()) < 1.0e-3f,
               "copyNoOp: (premise) slot A is at its default with a redo entry waiting");

        proc.switchToSlot (1);                       // B active, also default
        proc.copySlotToOther();                      // B → A: A already holds this state
        proc.switchToSlot (0);
        check (proc.canRedo(), "copyNoOp: a Copy that changes nothing does not clear redo");
        proc.redo();
        check (std::abs (driveDb() - 3.0f) < 1.0e-3f,
               "copyNoOp: …so the redo still re-lands the edit it belonged to");
    }

    // (5) ADV UNDO — ADR-0018's second half: an Advanced-mode toggle is a
    // real undo step (the click is gesture-bracketed by its ButtonAttachment;
    // this drives the same path directly), and the undo restore ADOPTS the
    // slot's advancedMode — the one adoption path that does. A bypass click,
    // by contrast, arms nothing: its diff is unrestorable (applySlotToLive
    // pins it), so it must not eat an Undo press.
    {
        AnabasisAudioProcessor proc;
        auto* adv = proc.apvts.getParameter (pid::advancedMode);
        const auto advOn = [&] { return proc.apvts.getRawParameterValue (pid::advancedMode)->load() >= 0.5f; };
        check (! proc.canUndo(), "advUndo: (premise) fresh instance, empty history");

        adv->beginChangeGesture();                   // the ButtonAttachment's bracket
        adv->setValueNotifyingHost (1.0f);
        adv->endChangeGesture();
        check (advOn(), "advUndo: (premise) the toggle landed");
        check (proc.canUndo(), "advUndo: the ADV toggle minted an undo step");
        proc.undo();
        check (! advOn(), "advUndo: undo restores the previous view mode");
        proc.redo();
        check (advOn(), "advUndo: redo re-applies it");

        auto* bypass = proc.apvts.getParameter (pid::bypass);
        bypass->beginChangeGesture();
        bypass->setValueNotifyingHost (1.0f);
        bypass->endChangeGesture();
        // The bypass click must not have minted a step: undoing now must
        // change ADV (the last real step), not bypass.
        proc.undo();
        check (! advOn(), "advUndo: a bypass click minted NO step — undo reached the ADV toggle");
        check (proc.apvts.getRawParameterValue (pid::bypass)->load() >= 0.5f,
               "advUndo: …and bypass itself was untouched by the restore (pinned view tier)");
    }

    // (4) `refreshMapping()`'s CONTRACT. Its header promised "this is what
    // re-lands the curve values", unconditionally; inside a `ScopedRestore` it
    // arms the flag, `drainTick` suppresses the whole tick, and the scope's
    // exit ABORTS the flag — so the curve is neither landed then nor deferred
    // to afterwards. That is the correct §5.3 outcome (a mapping over a restore
    // is the clobber the guard exists to prevent), so the fix is to the
    // contract: the deferral is now reported rather than silent. Only one
    // caller had ever discovered it, and it documented the discovery at its own
    // site (`applyFactoryPreset` drops its guard first) rather than at the API.
    {
        AnabasisAudioProcessor proc;
        auto* limGain  = proc.apvts.getParameter (pid::limGain);
        auto* loudness = proc.apvts.getParameter (pid::loudness);
        loudness->setValueNotifyingHost (loudness->getNormalisableRange().convertTo0to1 (60.0f));
        proc.getMacroEngine().flushPendingMapping();

        // An UNGESTURED write: §5.3's discriminator needs a gesture, so this
        // detaches nothing and the next mapping pass is free to overwrite it.
        // That makes "did the curve re-land?" a question about the mapping
        // alone, with the detach mask held out of it.
        const float offCurve = 2.0f;
        limGain->setValueNotifyingHost (limGain->getNormalisableRange().convertTo0to1 (offCurve));
        auto held = [&proc] { return proc.apvts.getRawParameterValue (pid::limGain)->load(); };
        check (std::abs (held() - offCurve) < 1.0e-3f,
               "contract: (premise) limGain sits off the curve, undetached");

        {
            const MacroEngine::ScopedRestore guard (proc.getMacroEngine());
            check (! proc.getMacroEngine().refreshMapping(),
                   "contract: refreshMapping REPORTS that a restore suppressed the re-land");
            check (std::abs (held() - offCurve) < 1.0e-3f,
                   "contract: …and the curve was in fact not landed");
        }

        // The arm was dropped, not queued: the next drain must not land it
        // either, or the deferral would be a delayed clobber of the values the
        // restore had carried.
        proc.getMacroEngine().flushPendingMapping();
        check (std::abs (held() - offCurve) < 1.0e-3f,
               "contract: a suppressed re-land is aborted, never deferred past the restore");

        check (proc.getMacroEngine().refreshMapping(),
               "contract: outside a restore it reports that it ran…");
        check (std::abs (held() - macro_curves::limGainDb (0.6f)) < 0.05f,
               "contract: …and the curve is what the parameter now holds");
    }
}

// Round 39: three mechanical invariants, one stimulus each.
static void testPreparedStateAndSlotOwnership()
{
    // (1) A prepared instance that has processed NO block must not serialise a
    // frozen vector built from the published atomics — they are still at their
    // initialisation zeros, and writing them over `liveFrozenTrims` replaces a
    // vector the slot holds with values nothing ever measured. The marker that
    // discriminates the two was set inside `publishTrims()`, which `reset()`
    // also calls, so it read true for every prepared instance.
    {
        AnabasisAudioProcessor proc;
        // A loaded frozen vector, via the same path a session load uses.
        juce::MemoryBlock blank;
        proc.getStateInformation (blank);
        auto root = juce::ValueTree::fromXml (
            *juce::AudioProcessor::getXmlFromBinary (blank.getData(), (int) blank.getSize()));
        auto setFreezeOn = [] (juce::ValueTree params)
        {
            if (auto fz = params.getChildWithProperty ("id", "freeze"); fz.isValid())
            {
                fz.setProperty ("value", 1.0, nullptr);
                fz.setProperty ("raw",   1.0, nullptr);
            }
        };
        setFreezeOn (root.getChildWithName ("ANABASIS"));
        auto slot = root.getChildWithName ("AB").getChild (0);
        setFreezeOn (slot.getChildWithName ("ANABASIS"));
        juce::ValueTree trims ("FROZEN_TRIMS");
        trims.setProperty ("releaseOctaves", 0.25, nullptr);
        trims.setProperty ("stereoLink",     -0.1, nullptr);
        trims.setProperty ("scHpfHz",         3.0, nullptr);
        trims.setProperty ("dynTiltDb",       0.4, nullptr);
        slot.appendChild (trims, nullptr);

        juce::MemoryBlock in;
        juce::AudioProcessor::copyXmlToBinary (*root.createXml(), in);
        proc.setStateInformation (in.getData(), (int) in.getSize());

        // Run the restore all the way in, so `frozenRestorePending()` is false
        // and the capture branch is the one that decides. Without that the
        // mirror wins for a different reason and the marker is untested.
        proc.prepareToPlay (48000.0, 512);
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buf (2, 512);
        for (int n = 0; n < 512; ++n) { buf.setSample (0, n, 0.2f); buf.setSample (1, n, 0.2f); }
        for (int b = 0; b < 24; ++b)
            proc.processBlock (buf, midi);

        auto savedRelease = [&proc]
        {
            juce::MemoryBlock out;
            proc.getStateInformation (out);
            const auto r = juce::ValueTree::fromXml (
                *juce::AudioProcessor::getXmlFromBinary (out.getData(), (int) out.getSize()));
            return r.getChildWithName ("AB").getChild (0).getChildWithName ("FROZEN_TRIMS")
                    .getProperty ("releaseOctaves");
        };
        check (juce::exactlyEqual ((double) savedRelease(), 0.25),
               "preparedSave: (premise) the restored vector is applied and saved back");

        // A host sample-rate change. `AdaptiveEngine::reset()` zeroes `trims`
        // AND republishes them, so the four published atomics are back at
        // their initialisation values — with the restore no longer pending,
        // the next save read them and wrote zeros over the slot's latch.
        proc.prepareToPlay (96000.0, 512);
        check (juce::exactlyEqual ((double) savedRelease(), 0.25),
               "preparedSave: a re-prepare cannot overwrite the held vector with initialisation zeros");
    }

    // (2) The spectrum rings are analyser state that must not survive a
    // re-prepare: `SpectrumView` maps bins through the CURRENT sample rate, so
    // frames captured at the previous one are drawn at the wrong frequencies.
    // The GR history ring has been cleared at `prepareToPlay` since P3.
    {
        AnabasisAudioProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buf (2, 512);
        for (int n = 0; n < 512; ++n) { buf.setSample (0, n, 0.25f); buf.setSample (1, n, 0.25f); }
        for (int b = 0; b < 4; ++b)
            proc.processBlock (buf, midi);
        check (proc.spectrumInRing().writeCount() > 0 && proc.spectrumOutRing().writeCount() > 0,
               "spectrumPrepare: (premise) both rings captured frames");
        proc.prepareToPlay (96000.0, 512);
        check (proc.spectrumInRing().writeCount() == 0 && proc.spectrumOutRing().writeCount() == 0,
               "spectrumPrepare: a re-prepare leaves no frame from the previous rate readable");
    }

    // (3) A §7 pre-state belongs to the slot it was taken in. A drag open
    // across an A/B switch had its snapshot captured from the OLD slot, and
    // the gesture-end then compared it against the NEW slot's values — the
    // difference is the slot change itself, so the end pushed a step onto the
    // new slot's stack describing a state that slot never held.
    {
        AnabasisAudioProcessor proc;
        auto* drive = proc.apvts.getParameter (pid::clipDrive);
        auto set = [drive] (float v)
        { drive->setValueNotifyingHost (drive->getNormalisableRange().convertTo0to1 (v)); };

        // Slot B must differ from slot A, or the gesture-end compares two
        // equivalent trees, pushes nothing for that reason instead, and the
        // guard reads as passing while enforcing nothing.
        proc.switchToSlot (1);
        set (11.0f);
        proc.switchToSlot (0);

        drive->beginChangeGesture();                       // open on slot A
        set (7.0f);
        proc.switchToSlot (1);                             // …and switch under it
        const bool undoBefore = proc.canUndo();
        drive->endChangeGesture();                         // the end lands on slot B
        check (proc.canUndo() == undoBefore,
               "slotGesture: a drag open across an A/B switch pushes no step onto the new slot");
    }

    // (4) The mirror image of (1), and the half (1)'s fix left open. In (1) the
    // vector arrived by RESTORE, so the wrapper's `liveFrozenTrims` held a copy
    // and the re-prepare could only threaten to overwrite it. A vector latched
    // LIVE — the user froze while playing, which no restore path observes —
    // existed ONLY in the engine's published atomics, and `AdaptiveEngine::
    // reset()` zeroes the internal struct and republishes it whatever Freeze
    // says. So a host sample-rate change took the latch with it: before round
    // 39 the save then wrote the post-reset ZEROS (an invalid vector, re-
    // injected on the next load); after it, the save wrote no FROZEN_TRIMS
    // child AT ALL. Same loss, quieter. The vector's durable owner is the
    // wrapper's mirror, so the engine's copy is captured into it at the one
    // point where that copy is about to be destroyed.
    {
        AnabasisAudioProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buf (2, 512);

        // Freeze OFF and audible programme: §5.4's slew is the ONLY path that
        // publishes a measured vector, so this is what makes the latch real
        // rather than the initialisation zeros (1) is about.
        for (int b = 0; b < 60; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                const float v = 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                 * 997.0f * (float) (b * 512 + n) / 48000.0f);
                buf.setSample (0, n, v);
                buf.setSample (1, n, v);
            }
            proc.processBlock (buf, midi);
        }
        check (proc.adaptiveReadout().hasPublishedTrims(),
               "liveLatch: (premise) the engine published a measured vector");
        const double latched = (double) proc.adaptiveReadout().publishedTrimRelease();
        check (std::abs (latched) > 1.0e-6,
               "liveLatch: (premise) the vector MOVED — a zero latch would pass either way");

        // The latch itself. No restore has ever run on this instance, so the
        // wrapper's mirror is empty and the atomics are the only record.
        auto* fz = proc.apvts.getParameter (pid::freeze);
        fz->setValueNotifyingHost (1.0f);

        // A MEASURED trim is not a decimal-exact number the way (1)'s hand-built
        // 0.25 is, and this one makes the full XML round trip, so the tolerance
        // is the text conversion's and nothing else: 1e-9 against a ~4e-4 value
        // still separates "the vector" from both of the failure modes (an
        // absent child reads as the -999 default, a reset one as 0).
        auto savedRelease = [&proc] () -> double
        {
            juce::MemoryBlock out;
            proc.getStateInformation (out);
            const auto r = juce::ValueTree::fromXml (
                *juce::AudioProcessor::getXmlFromBinary (out.getData(), (int) out.getSize()));
            return (double) r.getChildWithName ("AB").getChild (0)
                             .getChildWithName ("FROZEN_TRIMS")
                             .getProperty ("releaseOctaves", -999.0);
        };
        check (std::abs (savedRelease() - latched) < 1.0e-9,
               "liveLatch: (premise) the live latch serialises while the engine still holds it");

        proc.prepareToPlay (96000.0, 512);
        // The two sets part company HERE, which is the whole reason there are
        // two: the PUBLISHED set describes what the adaptive layer is applying
        // and is correctly zeroed with the internal struct (KI-006's audio and
        // readout halves, untouched), while the RETAINED set is persistence
        // state and survives, exactly as `learned`/`refOnsetRate`/`refTiltDb`
        // always have. Asserting both is what stops a future "simplification"
        // from collapsing them back into one.
        check (! proc.adaptiveReadout().hasPublishedTrims(),
               "liveLatch: (premise) the APPLIED vector did not survive re-initialisation");
        check (proc.adaptiveReadout().hasRetainedTrims(),
               "liveLatch: the RETAINED vector did — persistence state outlives a re-prepare");
        check (std::abs (savedRelease() - latched) < 1.0e-9,
               "liveLatch: a re-prepare cannot take a live-latched Freeze with it");
    }
}

// Round 42. `FROZEN_TRIMS` is PER-SLOT; the engine's retained vector is
// engine-wide and carries no slot identity. The two meet in
// `engineFrozenTrimsIfLive()`, and without a third condition the meeting is
// wrong: after an A/B switch into a freeze-ON slot that holds no vector of its
// own, nothing stages a restore (the stage is gated on the mirror being valid),
// so the generation pair stays equal and the incoming slot's next save
// serialised the OUTGOING slot's latch as if it owned it — after which the next
// A/B or undo restore injected it. A runtime cache may only answer for the slot
// it was filled under, which is what the base comparand records.
static void testAFrozenLatchDoesNotFollowTheSlotSwitch()
{
    AnabasisAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, 512);
    for (int b = 0; b < 60; ++b)                     // slot A latches a real vector
    {
        for (int n = 0; n < 512; ++n)
        {
            const float v = 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi
                                             * 997.0f * (float) (b * 512 + n) / 48000.0f);
            buf.setSample (0, n, v);
            buf.setSample (1, n, v);
        }
        proc.processBlock (buf, midi);
    }
    auto* fz = proc.apvts.getParameter (pid::freeze);
    fz->setValueNotifyingHost (1.0f);
    const double latched = (double) proc.adaptiveReadout().retainedTrimRelease();
    check (std::abs (latched) > 1.0e-6, "slotIsolation: (premise) slot A latched a vector");

    // Read a named slot out of a full save. Child 0 is ALWAYS slot 0 and child 1
    // slot 1 — `getStateInformation` orders them by index, not by which is live.
    auto savedRelease = [&proc] (int slot) -> double
    {
        juce::MemoryBlock out;
        proc.getStateInformation (out);
        const auto r = juce::ValueTree::fromXml (
            *juce::AudioProcessor::getXmlFromBinary (out.getData(), (int) out.getSize()));
        return (double) r.getChildWithName ("AB").getChild (slot)
                         .getChildWithName ("FROZEN_TRIMS")
                         .getProperty ("releaseOctaves", -999.0);
    };
    check (std::abs (savedRelease (0) - latched) < 1.0e-9,
           "slotIsolation: (premise) slot A serialises the vector it latched");

    // Into slot B, which is at defaults and has never latched anything. Freeze
    // goes ON *after* the switch, so B is freeze-ON with no FROZEN_TRIMS child —
    // the exact shape that made the capture speak for the wrong slot.
    proc.switchToSlot (1);
    check (proc.apvts.getRawParameterValue (pid::freeze)->load() < 0.5f,
           "slotIsolation: (premise) the defaults slot arrives with Freeze OFF");
    fz->setValueNotifyingHost (1.0f);

    check (juce::exactlyEqual (savedRelease (1), -999.0),
           "slotIsolation: a slot that never latched serialises no vector of another slot's");
    check (std::abs (savedRelease (0) - latched) < 1.0e-9,
           "slotIsolation: …and slot A's own record is untouched by the switch");

    // Back to A: its vector arrives through the mirror and a staged restore,
    // which is the branch that owns the window before the restore lands.
    proc.switchToSlot (0);
    check (std::abs (savedRelease (0) - latched) < 1.0e-9,
           "slotIsolation: switching back restores slot A's own vector");
}

// Round 42. The §7 history is message-thread-owned: `setStateInformation` may
// arrive on any thread, so it announces the session change with a counter and
// the message thread does the clearing. The externally observable half is that
// a load must still start a fresh history — including for a drag that was open
// across it, whose pre-state describes a session that is no longer loaded.
static void testHistoryOwnershipAcrossAStateLoad()
{
    AnabasisAudioProcessor proc;
    auto* drive = proc.apvts.getParameter (pid::clipDrive);
    auto set = [drive] (float v)
    { drive->setValueNotifyingHost (drive->getNormalisableRange().convertTo0to1 (v)); };

    drive->beginChangeGesture();
    set (7.0f);
    drive->endChangeGesture();
    check (proc.canUndo(), "historyEpoch: (premise) a completed drag pushed a step");

    juce::MemoryBlock state;
    proc.getStateInformation (state);

    // A drag OPEN across the load. Its pre-state belongs to the outgoing
    // session, so the end must push nothing — and the stacks must read empty
    // from the first query, not from whenever a clear happens to run.
    drive->beginChangeGesture();
    set (9.0f);
    proc.setStateInformation (state.getData(), (int) state.getSize());
    check (! proc.canUndo() && ! proc.canRedo(),
           "historyEpoch: a load starts a fresh history for both slots");
    set (3.0f);
    drive->endChangeGesture();
    check (! proc.canUndo(),
           "historyEpoch: a drag open across the load pushes no step from the old session");

    // …and the history works again afterwards: the epoch is reconciled once,
    // not latched into a permanently empty state.
    drive->beginChangeGesture();
    set (5.0f);
    drive->endChangeGesture();
    check (proc.canUndo(), "historyEpoch: the next completed drag pushes normally");
}

// Round 41. The durable copy of a frozen latch must live in the LOCK-FREE
// layer, because the two threads that need it cannot be made to take turns:
// `prepareToPlay` is a host callback JUCE does not promise on the message
// thread (THREADING_POLICY's PDC amendment, and the premise KI-003 and the
// `startDraining`/`stopDraining` split both rest on), while the editor polls
// `presetDirty()` → `saveSlotFromLive()` continuously, which reads
// `liveFrozenTrims` and `createCopy()`s it. Round 40 rescued the latch across a
// re-prepare by ASSIGNING that member from `prepareToPlay`: an unsynchronised
// write to a non-thread-safe `juce::ValueTree` — a reference-counted pointer
// swap — opposite a continuous reader, with both sides gated on Freeze being ON
// so the two windows coincided exactly rather than being disjoint.
//
// The stimulus is a stress rather than a proof, and is labelled as one: it runs
// the two callbacks concurrently for long enough that the round-40 write would
// be reading and releasing the same object (it is a hard data race, so a
// sanitiser build is where it is a *certain* failure), and then asserts the
// serialised vector is still the latched one. What makes the defect impossible
// rather than merely unlikely is structural and checked above: nothing in
// `prepareToPlay` touches wrapper `ValueTree` state at all any more.
static void testTheFrozenLatchNeedsNoThreadCrossing()
{
    AnabasisAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, 512);
    for (int b = 0; b < 60; ++b)
    {
        for (int n = 0; n < 512; ++n)
        {
            const float v = 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi
                                             * 997.0f * (float) (b * 512 + n) / 48000.0f);
            buf.setSample (0, n, v);
            buf.setSample (1, n, v);
        }
        proc.processBlock (buf, midi);
    }
    const double latched = (double) proc.adaptiveReadout().retainedTrimRelease();
    check (std::abs (latched) > 1.0e-6, "noCrossing: (premise) a live latch exists to lose");

    auto* fz = proc.apvts.getParameter (pid::freeze);
    fz->setValueNotifyingHost (1.0f);
    // `presetDirty()` returns early with no preset loaded, so the poll would
    // never reach `saveSlotFromLive` and the reader half of the race would not
    // exist. A factory apply gives it a name and a baseline; `freeze` is
    // preset-EXCLUDED, so it stays ON across the apply.
    check (proc.applyFactoryPreset (0), "noCrossing: (premise) a preset is loaded to compare against");
    check (proc.apvts.getRawParameterValue (pid::freeze)->load() >= 0.5f,
           "noCrossing: (premise) …and the apply left Freeze ON");

    std::atomic<bool> hostDone { false };
    std::atomic<int>  polls { 0 };

    // THE OVERLAP IS ESTABLISHED, NOT HOPED FOR. The first form was a bare
    // `while (! hostDone) { poll; }` with the host started first, so the premise
    // held only if the main thread reached the loop condition before the host
    // finished all 60 `prepareToPlay` calls. Nothing made that true. **CI caught
    // it** on 2026-08-14 (run 31801408265, sanitizers job): `polls` was 0 and
    // the suite reported 841 checks / 1 failure while memcheck itself reported
    // 0 errors from 0 contexts — the test failed, not the detector, which is
    // worth stating because the job's name says memcheck.
    //
    // WHAT IS NOT CLAIMED: the exact interleaving. valgrind serialises threads
    // under its own scheduler, which is the obvious suspect, but the same build
    // under the same command reproduces 8440 polls here across repeated runs and
    // never 0 — so the mechanism is unconfirmed and is deliberately not asserted.
    // That is what an unguaranteed premise looks like from the inside, and the
    // fix is to remove the dependency rather than to tune it: the host waits for
    // the poll loop to be RUNNING, and the loop is a do/while, so `polls > 0`
    // holds by construction and the concurrency the stimulus is about is
    // guaranteed instead of raced for. A stronger stimulus than the original,
    // not a weaker one.
    std::atomic<bool> pollingStarted { false };
    std::thread host ([&proc, &hostDone, &pollingStarted]
    {
        while (! pollingStarted.load (std::memory_order_acquire))
            std::this_thread::yield();
        for (int i = 0; i < 60; ++i)                     // the host changing rate
            proc.prepareToPlay ((i & 1) != 0 ? 96000.0 : 48000.0, 512);
        hostDone.store (true, std::memory_order_release);
    });
    do
    {
        proc.presetDirty();                              // what the editor tick does
        polls.fetch_add (1, std::memory_order_relaxed);
        pollingStarted.store (true, std::memory_order_release);
    } while (! hostDone.load (std::memory_order_acquire));
    host.join();
    check (polls.load() > 0, "noCrossing: (premise) the poll actually ran against the re-prepares");

    juce::MemoryBlock out;
    proc.getStateInformation (out);
    const auto r = juce::ValueTree::fromXml (
        *juce::AudioProcessor::getXmlFromBinary (out.getData(), (int) out.getSize()));
    const double saved = (double) r.getChildWithName ("AB").getChild (0)
                                  .getChildWithName ("FROZEN_TRIMS")
                                  .getProperty ("releaseOctaves", -999.0);
    check (std::abs (saved - latched) < 1.0e-9,
           "noCrossing: the latch survives 60 re-prepares racing the dirty poll");
}

// Round 38's state-consistency invariants: each has exactly one observable
// consequence, so each gets exactly one stimulus.
static void testStateReplacementAndHistoryConsistency()
{
    // (1) A state LOAD resets every member of the gesture-state family.
    // `managedGestureBits` was the one it did not: a BEGIN delivered off the
    // message thread with the session replaced before its matching END left
    // the bit standing, and the next UNGESTURED write to that managed
    // parameter then satisfied the "gesture-bracketed" half of §5.3.
    {
        AnabasisAudioProcessor proc;
        juce::MemoryBlock session;
        proc.getStateInformation (session);

        auto* limGain = proc.apvts.getParameter (pid::limGain);
        std::thread offThread ([limGain] { limGain->beginChangeGesture(); });   // no END
        offThread.join();

        proc.setStateInformation (session.getData(), (int) session.getSize());

        // An UNGESTURED write, i.e. automation. With the stale bit it detached.
        limGain->setValueNotifyingHost (limGain->getNormalisableRange().convertTo0to1 (3.0f));
        proc.flushPendingDetach();
        check (proc.detachMask().isEmpty(),
               "loadReset: a gesture left open across a load cannot detach the next automation write");
    }

    // (2) Undo restores the dirty DATUM beside the state it describes.
    // `applySlotToLive` restores `presetName` from the StateSet, so leaving
    // `presetBaseline` behind left the two describing different presets: after
    // undoing a preset apply the top bar compared a previous preset's state
    // against the applied preset's baseline.
    {
        AnabasisAudioProcessor proc;
        proc.applyFactoryPreset (2);          // Loud Pop (Default shifted the bank by one)
        check (! proc.presetDirty(), "undoBaseline: (premise) a fresh apply reads clean");
        proc.applyFactoryPreset (3);          // EDM Club
        check (! proc.presetDirty(), "undoBaseline: (premise) …and so does the second");
        proc.undo();
        check (proc.currentPresetName() == "Loud Pop",
               "undoBaseline: (premise) undo restored the previous preset's name");
        check (! proc.presetDirty(),
               "undoBaseline: …and its baseline with it, so the name reads clean");
        proc.redo();
        check (proc.currentPresetName() == "EDM Club" && ! proc.presetDirty(),
               "undoBaseline: redo carries the pair the other way");
    }

    // (3) Reset-to-macro is a user-visible multi-parameter change, so §7 makes
    // it undoable like every other one. It clears the whole mask and re-lands
    // nine values; the writes are ungestured, so the drag path never saw them.
    {
        AnabasisAudioProcessor proc;
        auto* limGain = proc.apvts.getParameter (pid::limGain);
        proc.apvts.getParameter (pid::loudness)->setValueNotifyingHost (
            proc.apvts.getParameter (pid::loudness)->getNormalisableRange().convertTo0to1 (60.0f));
        proc.getMacroEngine().flushPendingMapping();

        limGain->beginChangeGesture();
        limGain->setValueNotifyingHost (limGain->getNormalisableRange().convertTo0to1 (2.0f));
        limGain->endChangeGesture();
        proc.flushPendingDetach();
        check (proc.detachMask().contains (pid::limGain),
               "resetUndo: (premise) limGain is detached at the user's value");

        proc.resetToMacro();
        check (proc.detachMask().isEmpty() && proc.canUndo(),
               "resetUndo: reset-to-macro re-engages and pushes an undo step");
        proc.undo();
        check (proc.detachMask().contains (pid::limGain)
                   && std::abs (proc.apvts.getRawParameterValue (pid::limGain)->load() - 2.0f) < 1.0e-3f,
               "resetUndo: …and undoing it restores both the mask and the value");
    }
}

// ---------------------------------------------------------------------------
// §5.3 detach / re-engage — ADR-0005's P5 half, the gesture grammar. The
// discriminator's three conditions each get the stimulus that isolates them:
// a GESTURED edit detaches; the SAME edit ungestured (automation) does not;
// a macro-originated write does not; and the next macro gesture re-engages
// everything through the normal mapping, while "reset to macro" re-engages
// in place. The §5.3 rule the mask enforces — the mapping SKIPS a detached
// parameter — is asserted through the mapper itself, not by inspecting bits.
static void testDetachAndReengageGrammar()
{
    AnabasisAudioProcessor proc;
    auto& apvts = proc.apvts;
    auto& macro = proc.getMacroEngine();

    auto* limGain  = apvts.getParameter (pid::limGain);
    auto* loudness = apvts.getParameter (pid::loudness);
    auto limGainValue = [&] { return apvts.getRawParameterValue (pid::limGain)->load(); };

    // Establish a macro position so the curve has somewhere to put limGain.
    loudness->setValueNotifyingHost (loudness->getNormalisableRange().convertTo0to1 (50.0f));
    macro.flushPendingMapping();
    const float mapped50 = limGainValue();
    check (mapped50 > 1.0f, "detach: (premise) the macro mapped limGain off default");

    // 1. An UNGESTURED write — automation playback — must not detach.
    limGain->setValueNotifyingHost (limGain->getNormalisableRange().convertTo0to1 (3.0f));
    proc.flushPendingDetach();
    check (proc.detachMask().isEmpty(), "detach: automation (no gesture) never detaches");
    macro.refreshMapping();               // and the macro takes it right back
    check (std::abs (limGainValue() - mapped50) < 0.01f,
           "detach: the ungestured edit was re-mapped by the macro");

    // 2. A GESTURED edit — a real user drag — detaches, and the macro then
    //    skips that parameter while still driving the others.
    limGain->beginChangeGesture();
    limGain->setValueNotifyingHost (limGain->getNormalisableRange().convertTo0to1 (2.5f));
    limGain->endChangeGesture();
    proc.flushPendingDetach();
    check (proc.detachMask().contains ("limGain"), "detach: a gestured edit sets the bit");

    const float userValue = limGainValue();
    loudness->setValueNotifyingHost (loudness->getNormalisableRange().convertTo0to1 (80.0f));
    macro.flushPendingMapping();
    check (std::abs (limGainValue() - userValue) < 1.0e-4f,
           "detach: the mapping skips the detached parameter");
    const float thr80 = apvts.getRawParameterValue (pid::compThreshold)->load();
    check (std::abs (thr80 - macro_curves::compThresholdDb (0.8f)) < 0.05f,
           "detach: the OTHER managed parameters still follow the macro");

    // 3. MACRO-originated writes never detach (condition 2).
    check (! proc.detachMask().contains ("compThreshold"),
           "detach: macro writes do not detach the parameters they move");

    // 4. Re-engage on the next macro-knob GESTURE: everything follows again.
    loudness->beginChangeGesture();
    loudness->setValueNotifyingHost (loudness->getNormalisableRange().convertTo0to1 (60.0f));
    loudness->endChangeGesture();
    proc.flushPendingDetach();
    macro.flushPendingMapping();
    check (proc.detachMask().isEmpty(), "reengage: a macro gesture clears the mask");
    check (std::abs (limGainValue() - macro_curves::limGainDb (0.6f)) < 0.05f,
           "reengage: the formerly detached parameter follows the curve again");

    // 5. Reset-to-macro: detach again, then re-engage IN PLACE — the macro
    //    does not move, the parameter lands back on the curve at 60.
    limGain->beginChangeGesture();
    limGain->setValueNotifyingHost (limGain->getNormalisableRange().convertTo0to1 (1.0f));
    limGain->endChangeGesture();
    proc.flushPendingDetach();
    check (proc.detachMask().contains ("limGain"), "resetToMacro: (premise) detached again");
    proc.resetToMacro();
    check (proc.detachMask().isEmpty(), "resetToMacro: the mask is cleared");
    check (std::abs (limGainValue() - macro_curves::limGainDb (0.6f)) < 0.05f,
           "resetToMacro: the parameter re-lands on the curve without the macro moving");

    // 6. A RESTORE lands managed values without detaching (condition 3): the
    //    A/B switch runs inside a ScopedRestore and writes the whole set.
    proc.switchToSlot (1);
    proc.flushPendingDetach();
    check (proc.detachMask().isEmpty(), "detach: an A/B restore sets no bits");
    proc.switchToSlot (0);
    proc.flushPendingDetach();

    // 7. The OVERLAP cases conditions 2 and 3 exist for — a write landing
    //    while the user's gesture is OPEN on the same parameter. The gesture
    //    bit alone cannot tell these writers apart; the source conditions can.
    //    (a) the MACRO writes limGain mid-gesture: not a user edit, no detach.
    proc.resetToMacro();
    proc.flushPendingDetach();
    loudness->setValueNotifyingHost (loudness->getNormalisableRange().convertTo0to1 (70.0f));
    limGain->beginChangeGesture();               // user holds the knob, moves nothing
    macro.flushPendingMapping();                 // mapping fires under the open gesture
    limGain->endChangeGesture();
    proc.flushPendingDetach();
    check (proc.detachMask().isEmpty(),
           "detach: a macro write under an open gesture is not a user edit");

    //    (b) a RESTORE writes limGain mid-gesture (the KI-003-adjacent shape):
    //    also not a user edit, no detach.
    limGain->beginChangeGesture();
    proc.switchToSlot (1);                       // ScopedRestore writes the whole set
    limGain->endChangeGesture();
    proc.flushPendingDetach();
    check (proc.detachMask().isEmpty(),
           "detach: a restore under an open gesture is not a user edit");
}

// ---------------------------------------------------------------------------
// §2.9 meter-hold reset (the P5 planned edge, now implemented): the request
// clears the SESSION-CUMULATIVE display state — integrated LUFS and the dBTP
// max-hold — at the next block top, and nothing else. The rolling windows
// keep running (momentary recovers by itself), the §2.7 compensation is
// untouched, and a state LOAD stages the same request, which is the answer to
// THREAD_MODEL's "whether a state load should clear them" question.
static void testMeterResetClearsSessionHolds()
{
    AnabasisAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, 512);

    auto feedTone = [&] (float amp, int blocks, int t0)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                const float v = amp * std::sin (2.0f * juce::MathConstants<float>::pi
                                                * 997.0f * (float) (t0 + b * 512 + n) / 48000.0f);
                buf.setSample (0, n, v);
                buf.setSample (1, n, v);
            }
            proc.processBlock (buf, midi);
        }
    };

    const int warm = (int) (6.0 * 48000.0 / 512.0);
    feedTone (0.5f, warm, 0);                          // ~-9 dBFS: loud programme
    check (proc.meterLufsI() > -20.0f, "meterReset: (premise) integrated reads the loud tone");
    check (proc.meterDbTpMax() > -8.0f, "meterReset: (premise) the dBTP hold is high");

    // DRAIN before resetting: the lookahead line holds ~10 ms of the loud
    // tone, and a reset consumed while that tail is still in flight is
    // immediately re-raised by it — correct meter behaviour (in-flight audio
    // IS programme), wrong stimulus for asserting "post-reset material only".
    feedTone (0.005f, 8, warm * 512);
    proc.requestMeterReset();
    feedTone (0.005f, warm, (warm + 8) * 512);         // ~-49 dBFS
    check (proc.meterDbTpMax() < -40.0f,
           "meterReset: the dBTP hold describes only post-reset material");
    check (proc.meterLufsI() < -40.0f,
           "meterReset: the integrated histogram was cleared");
    check (proc.meterLufsM() < -40.0f && proc.meterLufsM() > -70.0f,
           "meterReset: the rolling windows kept measuring (not blanked)");

    // A state LOAD stages the same request. Save the (quiet) state, feed loud
    // audio to re-raise the holds, load it back: the holds must clear again.
    juce::MemoryBlock state;
    proc.getStateInformation (state);
    feedTone (0.5f, warm, 2 * warm * 512);
    check (proc.meterDbTpMax() > -8.0f, "meterReset: (premise) holds re-raised before the load");
    feedTone (0.005f, 8, 3 * warm * 512);              // drain the loud tail first
    proc.setStateInformation (state.getData(), (int) state.getSize());

    // BEFORE any further audio: opening a project with the transport stopped
    // is the ordinary case, and no block then runs at all. The engine-side
    // clear legitimately waits for its block top, but the DISPLAY must not —
    // the request would otherwise sit pending while an open editor still read
    // the previous programme's integrated LUFS and dBTP maximum.
    check (proc.meterDbTpMax() < -100.0f,
           "meterReset: the load published the cleared dBTP hold with no audio at all");
    check (juce::exactlyEqual (proc.meterLufsI(), anabasis::LoudnessMeter::kSilentLufs),
           "meterReset: …and the cleared integrated reading");

    feedTone (0.005f, 20, (3 * warm + 8) * 512);
    check (proc.meterDbTpMax() < -40.0f,
           "meterReset: a session load cleared the previous programme's holds");

    // The GUI affordance's half of the SAME row, which had nothing behind it.
    // `LoudnessMeterView::mouseDown` calls `requestMeterReset()` and nothing
    // else, and the display publish lived at the state-load call site — so with
    // the transport stopped (exactly when a user reads an integrated figure and
    // decides to clear it) the click set a flag no block ever consumed, and the
    // panel went on showing the previous take's holds until audio ran again.
    // The pairing now lives inside the request, so both callers get it.
    feedTone (0.5f, warm, 4 * warm * 512);
    check (proc.meterDbTpMax() > -8.0f, "meterReset: (premise) holds re-raised for the click");
    feedTone (0.005f, 8, 5 * warm * 512);              // drain the loud tail, as above
    check (proc.meterDbTpMax() > -8.0f,
           "meterReset: (premise) the hold survives the quiet drain — nothing else cleared it");
    proc.requestMeterReset();                          // what the meter panel's click does
    check (proc.meterDbTpMax() < -100.0f,
           "meterReset: a reset request publishes the cleared dBTP hold with no audio at all");
    check (juce::exactlyEqual (proc.meterLufsI(), anabasis::LoudnessMeter::kSilentLufs),
           "meterReset: …and the cleared integrated reading");
}

// ---------------------------------------------------------------------------
// The GR ring's reset-epoch contract (the other half of the same planned
// edge): the write index MAY rewind across a reset; the epoch is odd while
// the host-thread clear is in flight and even when stable, and it moves by
// exactly 2 per reset — which is what lets a reader detect both "my batch
// raced a clear" (odd, or changed underneath the batch) and "the index
// rewound while I was not looking" (epoch differs from my anchor).
static void testGrRingResetEpoch()
{
    AnabasisAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    const auto& ring = proc.grHistory();

    const auto e0 = ring.resetEpoch();
    check ((e0 & 1u) == 0u, "grEpoch: stable state is even");

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, 512);
    buf.clear();
    for (int b = 0; b < 8; ++b)
        proc.processBlock (buf, midi);
    check (ring.available() == 8, "grEpoch: (premise) entries pushed");
    check (ring.resetEpoch() == e0, "grEpoch: pushing does not move the epoch");

    // Same rate and block size: NO reset (0.1.2 item 6) — this is the
    // pause/resume path, where hosts re-prepare on transport start and the
    // scrolling timeline must continue rather than restart.
    proc.prepareToPlay (48000.0, 512);
    check (ring.resetEpoch() == e0 && ring.available() == 8,
           "grEpoch: a re-prepare at the same rate/block keeps the history (pause/resume)");

    // A CHANGED configuration is what the clear exists for: the view maps the
    // window through the prepared rate/size, so stale entries would draw at
    // the wrong time base.
    proc.prepareToPlay (44100.0, 512);                 // reaches GrHistoryBuffer::reset()
    check (ring.resetEpoch() == e0 + 2, "grEpoch: one reset moves the epoch by exactly 2");
    check ((ring.resetEpoch() & 1u) == 0u, "grEpoch: and lands even");
    check (ring.available() == 0,
           "grEpoch: the index rewound — which is why the epoch must exist");
}

// ---------------------------------------------------------------------------
// The Settings panel's STATE→WIDGET direction. Every control there is bound to
// `InternalState`'s tree, which `replaceFrom` rewrites wholesale on every
// session load — so a panel left open across a load must follow. The controls
// that CANNOT use `Value::referTo` are the ones that keep missing it: the three
// combos map index↔value, `uiScaleBox` maps index↔percent, and the three §6.4
// target checkboxes are three BITS of one int. Round 26 fixed the first four;
// round 32 found the checkboxes still seeded once at construction while
// `LoudnessMeterView` read the tree every frame, so the meter moved and the
// boxes did not. This drives the re-seed directly (no message loop runs here,
// so the 24 Hz tick never fires) and finds the widgets by the text the user
// sees, which is the only handle a test outside the class has.
//
// The editor is CONSTRUCTED and never shown: nothing calls `setVisible` or
// `addToDesktop`, so no window peer is created and the suite still runs with no
// display (verified with `DISPLAY` unset — that is why the Linux job needs xvfb
// for pluginval, which DOES open the window, and not for this).
// The handles a test outside the class has: a Button's visible text, and the
// accessibility TITLE the host-hidden combos were given in round 29 (their
// tooltip, tidied) — the combos carry no other stable string until one is
// selected, and asserting on the selection is what the test is FOR.
static juce::Button* findButtonByText (juce::Component& root, const juce::String& text)
{
    for (auto* c : root.getChildren())
    {
        if (auto* b = dynamic_cast<juce::Button*> (c); b != nullptr && b->getButtonText() == text)
            return b;
        if (auto* found = findButtonByText (*c, text))
            return found;
    }
    return nullptr;
}

static int countSliders (juce::Component& root)
{
    int n = 0;
    for (auto* c : root.getChildren())
    {
        if (dynamic_cast<juce::Slider*> (c) != nullptr)
            ++n;
        n += countSliders (*c);
    }
    return n;
}

static juce::Slider* findSliderByTitle (juce::Component& root, const juce::String& title)
{
    for (auto* c : root.getChildren())
    {
        if (auto* sl = dynamic_cast<juce::Slider*> (c); sl != nullptr && sl->getTitle() == title)
            return sl;
        if (auto* found = findSliderByTitle (*c, title))
            return found;
    }
    return nullptr;
}

// The slider's own text box, which the LookAndFeel makes a `ValueBox`.
static juce::Label* findChildLabel (juce::Component& parent)
{
    for (auto* c : parent.getChildren())
        if (auto* l = dynamic_cast<juce::Label*> (c))
            return l;
    return nullptr;
}

static juce::TextEditor* findTextEditor (juce::Component& root)
{
    for (auto* c : root.getChildren())
    {
        if (auto* e = dynamic_cast<juce::TextEditor*> (c))
            return e;
        if (auto* found = findTextEditor (*c))
            return found;
    }
    return nullptr;
}

static juce::Button* findButtonById (juce::Component& root, const juce::String& id)
{
    for (auto* c : root.getChildren())
    {
        if (auto* b = dynamic_cast<juce::Button*> (c); b != nullptr && b->getComponentID() == id)
            return b;
        if (auto* found = findButtonById (*c, id))
            return found;
    }
    return nullptr;
}

static juce::ComboBox* findComboByTitle (juce::Component& root, const juce::String& title)
{
    for (auto* c : root.getChildren())
    {
        if (auto* b = dynamic_cast<juce::ComboBox*> (c); b != nullptr && b->getTitle() == title)
            return b;
        if (auto* found = findComboByTitle (*c, title))
            return found;
    }
    return nullptr;
}

// The Save-Preset name field's focus glow is a two-part contract: the
// LookAndFeel draws the accent-lit rounded border only for a `TextEditor`
// carrying a "glow" property, and the owning component has to set it. For the
// whole of P5 only the first half existed, so `fillTextEditorBackground` and
// `drawTextEditorOutline` fell through to the JUCE default and the designed
// border was unreachable — a styling branch guarded by a flag no owner armed,
// the same shape as `allCombos`/`hov` and `resetSweep`. Pinning the ARMING side
// is what a test can do here; the pixels are a brand-pass question.
// The pop-up shield's geometry, which is the one thing about it a headless test
// CAN reach — and the one thing whose absence makes the whole mechanism inert
// while every other part of it looks correct. A component with empty bounds
// fails the containment test in `getComponentAt`, so `setInterceptsMouseClicks`
// has nothing to intercept: the dismissing click still lands on the control
// underneath, and the z-order work and the open-menu bookkeeping become
// unobservable. Sizing is done in `resized()` like every other overlay.
static void testThePopupShieldActuallyCoversTheEditor()
{
    AnabasisAudioProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
    auto* ed = dynamic_cast<AnabasisAudioProcessorEditor*> (base.get());
    check (ed != nullptr, "shieldBounds: (premise) the editor was created");
    if (ed == nullptr)
        return;

    auto* shield = ed->findChildWithID ("popupShield");
    check (shield != nullptr, "shieldBounds: (premise) the shield is a child of the editor");
    if (shield == nullptr)
        return;

    check (! shield->getBounds().isEmpty(),
           "shieldBounds: the shield has a non-empty area (an empty one intercepts nothing)");
    check (shield->getBounds() == ed->getLocalBounds(),
           "shieldBounds: the shield covers the WHOLE editor");
    // The top bar specifically: it carries the A/B switch, which acts on the
    // press, so a shield that stopped below the bar would leave the worst case
    // of this defect uncovered.
    check (shield->getBounds().getY() == 0,
           "shieldBounds: the shield covers the top bar, not just the body");
    check (shield->isAlwaysOnTop(),
           "shieldBounds: the shield is always-on-top, so it can cover the backdrops");

    // …and it survives a resize, the way the other overlays do.
    const auto grown = ed->getLocalBounds().expanded (0, 40);
    ed->setSize (grown.getWidth(), grown.getHeight());
    check (shield->getBounds() == ed->getLocalBounds(),
           "shieldBounds: the shield tracks the editor's frame across a resize");
}

// A pop-up row that carries a shortcut must not draw its LABEL underneath the
// shortcut. Before this, `drawPopupMenuItem` drew the label across the full text
// rectangle and then the shortcut right-aligned over the same rectangle, so a
// label long enough to fill the row put glyphs under the shortcut's; the case was
// declared unreachable by a pair of `jassert`s, which compile out of every
// shipped build and made a supported JUCE feature a debug-only hard stop instead
// of something the row could render.
//
// The check is pixel-level and needs no eye: render the row twice with the SAME
// shortcut, once with a long label and once with none, and compare only the strip
// the shortcut occupies. If the label stays out of that strip the two are
// identical; if it intrudes they cannot be. The strip's geometry comes from
// `menuMetrics`, the same constants the drawing spends, so the test cannot agree
// with a number the code has stopped using.
static void testAPopupRowKeepsItsLabelOutOfTheShortcutStrip()
{
    using namespace abgui;
    AnabasisLookAndFeel lf;

    constexpr int   w = 220, h = 23;
    const juce::String shortcut ("CTRL+SHIFT+ALT+K");
    const juce::String longLabel ("Compressor Release Time Constant Override");

    auto renderRow = [&] (const juce::String& text)
    {
        juce::Image img (juce::Image::ARGB, w, h, true);
        juce::Graphics g (img);
        lf.drawPopupMenuItem (g, { 0, 0, w, h },
                              /*isSeparator*/ false, /*isActive*/ true,
                              /*isHighlighted*/ false, /*isTicked*/ false,
                              /*hasSubMenu*/ false, text, shortcut, nullptr, nullptr);
        return img;
    };

    // The rectangle `drawPopupMenuItem` hands the shortcut, reconstructed from
    // the shared constants: the row less its insets, less the tick gutter, then
    // the right-hand strip the shortcut's own smaller type needs.
    auto textArea = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h)
                        .reduced (menuMetrics::padX, 0.0f);
    textArea.removeFromLeft (menuMetrics::tickGutter);
    juce::GlyphArrangement ga;
    ga.addLineOfText (lf.getPopupMenuFont().withHeight (menuMetrics::shortcutPt),
                      shortcut, 0.0f, 0.0f);
    const auto strip = textArea.removeFromRight (
                           juce::jmin (ga.getBoundingBox (0, -1, true).getWidth()
                                           + menuMetrics::shortcutGap,
                                       textArea.getWidth() * 0.5f));
    check (strip.getWidth() > 4.0f,
           "menuShortcut: (premise) the shortcut strip is a real region to compare");

    const auto withLabel = renderRow (longLabel);
    const auto bare      = renderRow ({});

    const auto probe = strip.getSmallestIntegerContainer()
                           .getIntersection (juce::Rectangle<int> (0, 0, w, h));
    check (! probe.isEmpty(), "menuShortcut: (premise) the strip lies inside the row");

    int differing = 0;
    for (int y = probe.getY(); y < probe.getBottom(); ++y)
        for (int x = probe.getX(); x < probe.getRight(); ++x)
            if (withLabel.getPixelAt (x, y) != bare.getPixelAt (x, y))
                ++differing;

    check (differing == 0,
           "menuShortcut: a long label leaves the shortcut's strip untouched "
           "(it is reserved before the label is drawn, not painted over)");

    // The premise the comparison rests on: this label really is long enough to
    // have reached the strip. Without it, a row whose label fit anyway would pass
    // while proving nothing.
    juce::GlyphArrangement label;
    label.addLineOfText (lf.getPopupMenuFont(), longLabel, 0.0f, 0.0f);
    check (label.getBoundingBox (0, -1, true).getWidth() > textArea.getWidth(),
           "menuShortcut: (premise) the label overflows the space left beside the shortcut");

    // And the sub-menu chevron renders rather than aborting — the other case the
    // removed assertions forbade.
    juce::Image sub (juce::Image::ARGB, w, h, true);
    {
        juce::Graphics g (sub);
        lf.drawPopupMenuItem (g, { 0, 0, w, h }, false, true, true, true,
                              /*hasSubMenu*/ true, longLabel, {}, nullptr, nullptr);
    }
    check (sub.getWidth() == w, "menuShortcut: a sub-menu row renders");
}

// `drawResizableFrame` is reached by TWO unrelated JUCE callers — a parented
// pop-up menu painting its border ring, and any `ResizableBorderComponent`
// painting itself — and the override must draw nothing for the first and the
// frame for the second. It cannot see the caller, so it decides on the border's
// SHAPE plus whether a menu is on screen at all. The shape test alone is a magic
// number: a drag zone of exactly `getPopupMenuBorderSize()` on all four sides is
// legal to construct and would silently lose its frame, which is the failure the
// override exists to prevent, arriving through the override.
static void testTheResizableFrameOverrideDiscriminatesItsCallers()
{
    using namespace abgui;
    AnabasisLookAndFeel lf;
    const int b = lf.getPopupMenuBorderSize();

    auto inkOf = [&lf] (juce::BorderSize<int> border)
    {
        juce::Image img (juce::Image::ARGB, 60, 40, true);
        {
            juce::Graphics g (img);
            lf.drawResizableFrame (g, 60, 40, border);
        }
        int inked = 0;
        for (int y = 0; y < 40; ++y)
            for (int x = 0; x < 60; ++x)
                if (img.getPixelAt (x, y).getAlpha() != 0)
                    ++inked;
        return inked;
    };

    // Premise: the base implementation actually paints something for a menu-
    // shaped border. Without this the "suppressed" assertions below are vacuous.
    lf.isPopupMenuOnScreen = nullptr;
    const int menuShapedInk = inkOf (juce::BorderSize<int> (b));
    check (menuShapedInk > 0,
           "resizableFrame: (premise) the base implementation inks a menu-shaped border");

    // 1) No menu on screen — the caller cannot be a menu, so the frame is DRAWN
    //    even though the border is exactly the menu's shape.
    lf.isPopupMenuOnScreen = [] { return false; };
    check (inkOf (juce::BorderSize<int> (b)) == menuShapedInk,
           "resizableFrame: a menu-shaped border still draws when no menu is open");

    // 2) Menu on screen + menu-shaped border — suppressed, which is the whole
    //    point of the override.
    lf.isPopupMenuOnScreen = [] { return true; };
    check (inkOf (juce::BorderSize<int> (b)) == 0,
           "resizableFrame: the parented pop-up's doubled edge is suppressed");

    // 3) Menu on screen but a NON-menu border shape — drawn. A resizable
    //    component that repaints while a drop-down happens to be open keeps its
    //    frame, so the state test cannot swallow the shape test.
    check (inkOf (juce::BorderSize<int> (b + 2)) > 0,
           "resizableFrame: a differently-sized border draws even while a menu is open");
    check (inkOf (juce::BorderSize<int> (b, b, b, b + 1)) > 0,
           "resizableFrame: a NON-UNIFORM border draws even while a menu is open");

    lf.isPopupMenuOnScreen = nullptr;
}

// A combo's drop-down must not open WIDER than the control it drops from.
// `getOptionsForComboBoxPopupMenu` already floors the menu at the box width; what
// nothing checked is the ceiling, and the row metrics moved it: `chrome` is
// `padX * 2 + tickGutter` = 38 against the 30 the measurement used before, so
// every menu asks for 8 px more than it did. That is fine only while every combo
// has 8 px of slack, and "fine today" is exactly the kind of claim that stops
// being true when a font, a metric or an item string changes. The tightest margin
// in the editor is the dither combo's, and the assertion carries the number so a
// regression names itself.
static void testEveryComboMenuFitsItsControl()
{
    using namespace abgui;
    AnabasisAudioProcessor proc;
    // Advanced ON, or the zone combos are never laid out and every width reads 0
    // — a walk that measures nothing passes everything.
    if (auto* adv = proc.apvts.getParameter (pid::advancedMode))
    {
        adv->beginChangeGesture();
        adv->setValueNotifyingHost (1.0f);
        adv->endChangeGesture();
    }
    std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
    auto* ed = dynamic_cast<AnabasisAudioProcessorEditor*> (base.get());
    check (ed != nullptr, "comboFit: (premise) the editor was created");
    if (ed == nullptr)
        return;
    ed->setSize (940, 900);

    AnabasisLookAndFeel lf;
    int found = 0, tightest = 1 << 20;
    std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
    {
        if (auto* cb = dynamic_cast<juce::ComboBox*> (&c))
        {
            if (cb->getWidth() > 0 && cb->getNumItems() > 0)
            {
                ++found;
                int worst = 0;
                for (int i = 0; i < cb->getNumItems(); ++i)
                {
                    int w = 0, h = 0;
                    lf.getIdealPopupMenuItemSize (cb->getItemText (i), false, 23, w, h);
                    worst = juce::jmax (worst, w);
                }
                tightest = juce::jmin (tightest, cb->getWidth() - worst);
            }
        }
        for (auto* k : c.getChildren())
            walk (*k);
    };
    walk (*ed);

    // Premise first: a walk that found nothing would satisfy every assertion
    // below by running none of them.
    check (found >= 10, "comboFit: (premise) the walk reached the editor's combo boxes");
    check (tightest >= 0,
           "comboFit: every combo's widest row still fits inside the control that opens it");
    // The margin itself is PRINTED, not asserted, and that is a deliberate
    // downgrade from the `>= 24` check this line used to carry.
    //
    // 24 was a SNAPSHOT of one machine's fonts, item strings and layout
    // constants — `chrome` growing from 30 to 38 moved it by 8 on its own — and
    // this suite runs on Linux, Windows and macOS, where `GlyphArrangement`
    // advances for the DEFAULT system font differ. A platform whose menu font is
    // a few percent wider would have turned the suite red for a metric
    // difference, with a message reading like a layout regression. A gate that
    // can fail without anything being wrong teaches people to ignore it, and the
    // numbers on the other two runners have not been measured here — asserting
    // them would be asserting something unverified.
    //
    // What survives is the requirement that actually protects the user: `>= 0`
    // above, which is platform-independent in MEANING (a row must fit the control
    // that opens it) even though its margin is not. The number is emitted every
    // run, so a metric change is still visible in the log on every platform —
    // which is what "tripwire" was for. Restore the assertion the moment the
    // figure is confirmed on all three runners.
    std::printf ("      comboFit: tightest combo margin %d px (advisory; the gate is >= 0)\n",
                 tightest);

    // WHAT CAN BE ASSERTED ON ALL THREE RUNNERS TODAY, since the pixel floor
    // cannot be: the measurement's ARITHMETIC, which is font-independent because
    // both sides below measure the same string with the same font on the same
    // machine.
    //
    // MEASURED, and the first draft of this comment claimed more than the
    // assertion delivers. Three mutants were run:
    //
    //   * `idealWidth = textW` (chrome dropped from the measurement) — KILLED,
    //     by this assertion naming the cause and by `shortcutRow` reporting the
    //     symptom (-30 px). That is the defect class that produced the original
    //     clipping, so it is the one worth naming.
    //   * `chrome = padX * 2.0f` (a term deleted from the shared constant) —
    //     SURVIVES. It has to: measurement and drawing both read `chrome`, so
    //     redefining it moves them together. That drift is designed out by
    //     sharing the constant rather than tested, which is the stronger
    //     arrangement, but it means no assertion here can catch it.
    //   * `r.reduced (padX * 2.0f, …)` in `drawPopupMenuItem` (the DRAWING
    //     spending more than the constant says) — SURVIVES, and this one is a
    //     real gap. Both width tests RECONSTRUCT the row's rectangles from the
    //     constants instead of observing what was drawn, so a drawing that stops
    //     agreeing with `chrome` is invisible to them. Closing it needs the test
    //     to render a row and measure the result; reconstructing it more
    //     carefully cannot, since the reconstruction is the copy that drifts.
    //
    // None of this replaces the floor: a font whose advances grew until a real
    // row stopped fitting is typography, not arithmetic, which is what the pixel
    // figure is for and why the advisory print stays until the three-runner
    // numbers exist.
    {
        const auto f = lf.getPopupMenuFont();
        const juce::String probe ("A measurement probe long enough to clear the minimum row");
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, probe, 0.0f, 0.0f);
        const int textW = (int) std::ceil (ga.getBoundingBox (0, -1, true).getWidth());

        int w = 0, h = 0;
        lf.getIdealPopupMenuItemSize (probe, false, 23, w, h);

        // Premise: the `jmax` against `minimumRow` must not be the term that
        // wins, or the identity below is asserting the floor instead.
        check (textW + (int) menuMetrics::chrome > menuMetrics::minimumRow,
               "comboFit: (premise) the probe row is wider than the minimum-row floor");
        check (w - textW == (int) menuMetrics::chrome,
               "comboFit: the ideal width is the measured text plus exactly the chrome a row draws");
    }
}

// The width budget for a row that carries a SHORTCUT, which
// `testAPopupRowKeepsItsLabelOutOfTheShortcutStrip` does not cover: that test
// pins the label and the shortcut not OVERLAPPING once the row has a width,
// while this one pins the width being enough in the first place.
//
// Why it needs pinning at all. `menuMetrics::chrome` is `padX * 2 + tickGutter`
// and does NOT include `shortcutGap`, so on paper the measurement allows less
// than the drawing spends. It works out, but through arithmetic no constant
// states — exactly the implicit, drift-prone budget `menuMetrics` was introduced
// to end. JUCE measures `text + "   " + shortcut`
// (`ItemComponent::getTextForMeasurement`) entirely in the 13.5 pt menu font,
// while `drawPopupMenuItem` renders the shortcut at `shortcutPt` (11 pt).
//
// WHAT ACTUALLY PAYS FOR THE GAP, measured rather than reasoned: the tightest
// row here clears the drawing by **5 px**. Equalising the two fonts (shortcut
// drawn at 13.5 pt) leaves **2 px** — still positive. So the font difference is
// worth ~3 px and is NOT the term carrying this; the three measured spaces are,
// at roughly 10 px against an 8 px `shortcutGap`. Worth knowing which lever
// matters: a font change costs a few px, and a `shortcutGap` change spends
// against a 5 px reserve directly — raising it to 20 puts the tightest row at
// −7 and ellipsises the label, which is this test's own mutant.
//
// The alternative — folding `shortcutGap` into `chrome` — is deliberately NOT
// taken, for the same reason the sub-menu chevron is not: it would widen every
// menu in the plug-in by 8 px to pre-pay for a case no menu here uses today.
static void testAShortcutRowIsMeasuredWideEnoughForItsOwnLabel()
{
    using namespace abgui;
    AnabasisLookAndFeel lf;
    const auto menuFont     = lf.getPopupMenuFont();
    const auto shortcutFont = menuFont.withHeight (menuMetrics::shortcutPt);

    auto widthOf = [] (const juce::Font& f, const juce::String& s)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, s, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    };

    // The rows a `TextEditor` context menu would carry if the host attached
    // accelerators, plus a deliberately short shortcut (the least slack case,
    // since the font difference scales with the shortcut's own width).
    const std::pair<const char*, const char*> rows[] = {
        { "Cut",        "Ctrl+X" },
        { "Copy",       "Ctrl+C" },
        { "Paste",      "Ctrl+V" },
        { "Select All", "Ctrl+A" },
        { "Undo",       "Ctrl+Z" },
        { "A considerably longer label than any menu here uses", "Ctrl+Shift+Alt+K" },
        { "Go",         "F1" },
    };

    int tightest = 1 << 20;
    for (const auto& r : rows)
    {
        const juce::String label (r.first), shortcut (r.second);

        int measuredW = 0, measuredH = 0;
        lf.getIdealPopupMenuItemSize (label + "   " + shortcut, false, 23, measuredW, measuredH);

        // What `drawPopupMenuItem` spends out of that width: the chrome, the
        // label at the menu font, and the shortcut strip at its own font plus
        // the gap. (The strip's 50 % cap only ever makes the drawing spend
        // LESS, so ignoring it keeps this the conservative direction.)
        const float drawn = menuMetrics::chrome
                          + widthOf (menuFont, label)
                          + widthOf (shortcutFont, shortcut) + menuMetrics::shortcutGap;

        tightest = juce::jmin (tightest, (int) std::floor ((float) measuredW - drawn));
    }

    // `>= 0` stays a GATE: unlike the combo advisory, this one states a property
    // rather than a snapshot — the measured width must cover what the drawing
    // spends, or the label is ellipsised, which is a defect on any platform. Its
    // MARGIN is font-dependent (5 px here) and is printed for the same reason the
    // combo one is: so a platform difference shows up as a number rather than as
    // a mystery. If this ever fails on macOS or Windows alone, read the printed
    // margin before assuming a layout regression — the fix may be `shortcutGap`,
    // which spends against that margin directly.
    std::printf ("      shortcutRow: tightest shortcut-row margin %d px\n", tightest);
    const auto msg = juce::String ("shortcutRow: a row carrying a shortcut is measured wide "
                                   "enough to draw its label in full — the label is not "
                                   "ellipsised to pay for the shortcut gap (tightest ")
                   + juce::String (tightest) + " px)";
    check (tightest >= 0, msg.toRawUTF8());
}

static void testTheSavePresetNameFieldIsTaggedForItsFocusGlow()
{
    AnabasisAudioProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
    auto* ed = dynamic_cast<AnabasisAudioProcessorEditor*> (base.get());
    check (ed != nullptr, "saveGlow: (premise) the editor was created");
    if (ed == nullptr)
        return;

    // The ONLY `juce::TextEditor` the editor constructs is `saveNameEditor` —
    // the ValueBox edit fields are created on demand by `juce::Label`, so they
    // do not exist in a freshly built tree. If that ever changes this premise
    // fails loudly rather than the check silently finding the wrong field.
    auto* name = findTextEditor (*ed);
    check (name != nullptr, "saveGlow: (premise) the save-preset name field exists");
    if (name == nullptr)
        return;
    check ((bool) name->getProperties().getWithDefault ("glow", false),
           "saveGlow: the name field carries the property its focus styling keys on");

    // Same contract shape, the other half of it: the LookAndFeel's `"icon"`
    // treatment (21 px glyph, rotated) names Undo/Redo in its own header, and
    // those two buttons carried no component ID at all — so a 30 px glyph
    // button drew a ~13 px character through the generic path. Round 54 armed
    // them; this is the arming side, which is the part a headless test can see.
    // The three sibling ids beside it (`"apply"`, `"metersicon"`, `"vtoggle"`)
    // were removed instead, because they style controls this product does not
    // have — there is nothing to assert about an id no component may carry.
    // 0x21BA/0x21BB since 2026-08-05 — the SIBLING'S circle arrows, which
    // still read as undo/redo under the icon treatment's 180° rotation; the
    // half-arrows they replace (0x21B6/0x21B7) inverted their meaning when
    // rotated, which is what the owner's "ugly icons" report was.
    for (const auto glyph : { (juce::juce_wchar) 0x21BA, (juce::juce_wchar) 0x21BB })
    {
        auto* b = findButtonByText (*ed, juce::String::charToString (glyph));
        const juce::String msg = juce::String ("saveGlow: the ")
                               + (glyph == (juce::juce_wchar) 0x21BA ? "undo" : "redo")
                               + " glyph button is tagged for the icon treatment";
        check (b != nullptr && b->getComponentID() == "icon", msg.toRawUTF8());
    }
}

// Round 42. A persisted `uiScale` that is not one of the seven legal steps —
// hand-edited state, or a session written when the list differed — used to be
// IGNORED by three sites with three different fallbacks: `applyUiScale` left the
// transform at 1.0, the constructor showed 100 %, and `refreshInternalSettingsBoxes`
// kept whatever the box already had. The last is the one that diverges on a
// project load: the window renders at one step while the panel displays another.
// One `nearestScaleIndex()` now answers for both, so they cannot disagree, and
// an out-of-list value CLAMPS the way every other persisted value in this tree
// does rather than being silently discarded.
// Round 44. The Settings callbacks are STORED closures invoked long after the
// constructor that built them has returned, and two of them captured a local
// reference variable (`ist`) / a reference parameter (`box`) by reference —
// which captures the variable, not the referent, so calling them afterwards is
// UB by [expr.prim.lambda.capture] even though every compiler resolves it
// through to the long-lived object.
//
// What this test is and is not: it drives the widget→state direction AFTER
// construction, so it covers the refactor (the closure still reaches the live
// tree, and reaches the RIGHT one). It cannot kill the pre-fix form — that is
// the nature of UB that happens to work, and pretending otherwise would be the
// kind of check that proves an impossible state. The value here is regression
// coverage for a direction the suite otherwise only drove state→widget.
// Round 46. A gesture-begin on one of the three §5.5 macros is not a neutral
// event: `audioProcessorParameterChangeGestureBegin` takes the macro branch,
// clears the WHOLE §5.3 detach mask and re-lands the curve. `ValueBox` opened
// its `ScopedDragNotification` on mouse-DOWN, so a plain click on the number
// under Loudness — to read it, or as the first half of a double-click to type
// into it — discarded every manual Advanced edit the user had made.
//
// The bracket now opens on the first movement instead. This drives the real
// ValueBox through synthesised events, because the whole point is which mouse
// event opens the gesture: a press-and-release must not re-engage, a drag must.
static void testAValueBoxClickIsNotAMacroGesture()
{
    AnabasisAudioProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
    auto* ed = dynamic_cast<AnabasisAudioProcessorEditor*> (base.get());
    check (ed != nullptr, "valueBoxGesture: (premise) the editor was created");
    if (ed == nullptr)
        return;

    // The macro knob by its registry name, and its text box — the LookAndFeel
    // gives every rotary one through `createSliderTextBox`.
    auto* macroParam = proc.apvts.getParameter (pid::loudness);
    check (macroParam != nullptr, "valueBoxGesture: (premise) the macro parameter exists");
    if (macroParam == nullptr)
        return;
    juce::Slider* knob = findSliderByTitle (*ed, macroParam->getName (24));
    check (knob != nullptr, "valueBoxGesture: (premise) the macro knob was found");
    if (knob == nullptr)
        return;
    juce::Label* box = findChildLabel (*knob);
    check (box != nullptr, "valueBoxGesture: (premise) its numeric readout was found");
    if (box == nullptr)
        return;

    auto detachOne = [&proc]
    {
        auto* lim = proc.apvts.getParameter (pid::limGain);
        lim->beginChangeGesture();
        lim->setValueNotifyingHost (lim->getNormalisableRange().convertTo0to1 (2.0f));
        lim->endChangeGesture();
        proc.flushPendingDetach();
    };
    auto ev = [box] (juce::Point<float> pos, juce::Point<float> downPos)
    {
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(),
                                 pos, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 box, box, juce::Time::getCurrentTime(), downPos,
                                 juce::Time::getCurrentTime(), 1, false);
    };

    // (1) PRESS AND RELEASE on the number: no movement, so no macro gesture.
    detachOne();
    check (proc.detachMask().contains (pid::limGain),
           "valueBoxGesture: (premise) a gestured edit detached limGain");
    juce::Component* boxComp = box;   // Label::mouseUp is protected; Component's is not
    boxComp->mouseDown (ev ({ 10.0f, 7.0f }, { 10.0f, 7.0f }));
    boxComp->mouseUp   (ev ({ 10.0f, 7.0f }, { 10.0f, 7.0f }));
    proc.getMacroEngine().flushPendingMapping();
    check (proc.detachMask().contains (pid::limGain),
           "valueBoxGesture: a click on the macro's readout does not re-engage the mask");

    // (2) DRAG on the number: a real macro move, so §5.3's rule applies exactly
    // as it does for the knob. Without this the first check is satisfied by a
    // readout that never gestures at all.
    boxComp->mouseDown (ev ({ 10.0f, 7.0f }, { 10.0f, 7.0f }));
    boxComp->mouseDrag (ev ({ 10.0f, -25.0f }, { 10.0f, 7.0f }));
    boxComp->mouseUp   (ev ({ 10.0f, -25.0f }, { 10.0f, 7.0f }));
    proc.getMacroEngine().flushPendingMapping();
    check (proc.detachMask().isEmpty(),
           "valueBoxGesture: …but dragging it re-engages, exactly as the knob does");
}

static void testTheSettingsCallbacksReachTheLiveTree()
{
    AnabasisAudioProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
    auto* ed = dynamic_cast<AnabasisAudioProcessorEditor*> (base.get());
    check (ed != nullptr, "settingsWrite: (premise) the editor was created");
    if (ed == nullptr)
        return;
    auto* scale = findComboByTitle (*ed, "UI Scale");
    auto* os    = findComboByTitle (*ed, "Oversampling");
    check (scale != nullptr && os != nullptr,
           "settingsWrite: (premise) both Settings combos were found");
    if (scale == nullptr || os == nullptr)
        return;

    // The hand-built combo (index ↔ PERCENT), whose closure re-fetches the tree.
    scale->setSelectedItemIndex (4, juce::sendNotificationSync);        // "XL" = 150 %
    check ((int) proc.internalState.state().getProperty (iid::uiScale, -1) == 150,
           "settingsWrite: the UI-scale closure writes the live InternalState tree");
    check (std::abs (ed->getTransform().mat00 - 1.50f) < 1.0e-4f,
           "settingsWrite: …and applies it, so the window follows the selection");

    // The helper-built combo (index ↔ 0-BASED value), whose closure captures the
    // widget by pointer. The encoding is contract — index 3 is "8x" and stores 3,
    // not the item ID 4.
    os->setSelectedItemIndex (3, juce::sendNotificationSync);
    check ((int) proc.internalState.state().getProperty (iid::oversample, -1) == 3,
           "settingsWrite: the helper-built closure stores the 0-based value it names");
}

// Round 50. A factory apply used to write every non-excluded parameter TWICE
// when the preset overrode it — once to its default in the first pass, once to
// the preset's value in the second — so the host was told about a value the
// preset never wanted, and the whole surface passed through a state no preset
// describes. The apply now computes each parameter's final value and writes it
// once.
//
// Measured on a parameter the preset overrides and the macro re-land does NOT
// touch: `lookahead` is not one of the nine §5.5 managed ids, so every
// notification it receives during an apply came from the preset path. A
// managed parameter would legitimately be notified again by `relandMacroCurve`
// afterwards, which is a different phase and not what this pins.
static void testAFactoryApplyWritesEachParameterOnce()
{
    struct CountingListener : juce::AudioProcessorListener
    {
        void audioProcessorParameterChanged (juce::AudioProcessor*, int i, float) override
        { if (i == watched) ++hits; }
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}
        int watched = -1, hits = 0;
    };

    AnabasisAudioProcessor proc;
    int lookaheadIndex = -1;
    for (int i = 0; i < proc.getParameters().size(); ++i)
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*> (proc.getParameters()[i]);
            p != nullptr && p->getParameterID() == pid::lookahead)
            lookaheadIndex = i;
    check (lookaheadIndex >= 0, "presetNotify: (premise) the probe parameter was found");
    if (lookaheadIndex < 0)
        return;

    // Factory index 1 ("Transparent Master") overrides `lookahead` — to 3.0 ms,
    // against the 2.0 ms registered default — and the probe is parked away from
    // BOTH first. That third position is what makes the count interesting: with
    // the parameter already at its default the old two-pass apply also wrote
    // once (the defaults pass had nothing to move), so the defect only shows
    // from a state a user actually reaches — a knob they moved before browsing
    // presets.
    //
    // The index is 1 and not 0 SINCE "Default" JOINED THE BANK AT INDEX 0
    // (2026-08-05): Default's override table is deliberately empty, so applying
    // it exercises the defaults pass alone and the two-pass defect this test
    // exists for has no override to duplicate. The check still passed there —
    // one write is one write — which is precisely why the stale index had to be
    // found by reading rather than by a red suite.
    auto* look = proc.apvts.getParameter (pid::lookahead);
    look->setValueNotifyingHost (look->getNormalisableRange().convertTo0to1 (7.5f));
    const float before = look->getValue();
    check (! juce::exactlyEqual (before, look->getDefaultValue()),
           "presetNotify: (premise) the probe starts away from its default");

    CountingListener counter;
    counter.watched = lookaheadIndex;
    proc.addListener (&counter);
    check (proc.applyFactoryPreset (1), "presetNotify: (premise) the apply succeeds");
    proc.removeListener (&counter);

    check (! juce::exactlyEqual (look->getValue(), before),
           "presetNotify: (premise) the preset really moves the probe parameter");
    check (counter.hits == 1,
           "presetNotify: an overridden parameter is announced ONCE, at its final value");
}

static void testAnOutOfListUiScaleClampsConsistently()
{
    AnabasisAudioProcessor proc;
    // 130 is not a step, and its nearest is 125 rather than the 100 a
    // "fall back to default" reading would give — so this separates clamping
    // from ignoring, which a value like 101 would not.
    proc.internalState.state().setProperty (iid::uiScale, 130, nullptr);

    std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
    auto* ed = dynamic_cast<AnabasisAudioProcessorEditor*> (base.get());
    check (ed != nullptr, "uiScaleClamp: (premise) the editor was created");
    if (ed == nullptr)
        return;
    auto* box = findComboByTitle (*ed, "UI Scale");
    check (box != nullptr, "uiScaleClamp: (premise) the UI-scale box was found");
    if (box == nullptr)
        return;

    // `getScaleFactor()` is the X scale of the transform applyUiScale set;
    // hostScale is 1 in the headless suite, so it IS the persisted step.
    // `mat00`, not the deprecated `getScaleFactor()`: JUCE deprecated the latter
    // because it is wrong for transforms carrying a rotation, and this build is
    // warning-free under the recommended flags. `applyUiScale` sets a PURE
    // scale, so the X scale IS the term.
    auto rendered = [ed] { return ed->getTransform().mat00; };
    check (std::abs (rendered() - 1.25f) < 1.0e-4f,
           "uiScaleClamp: an out-of-list percent renders at the NEAREST step, not at 100 %");
    check (box->getText() == "L",
           "uiScaleClamp: …and the panel displays the same step it rendered");

    // The item LABELS are the XS..XL names (2026-08-05, the sibling's
    // display); each name is index-locked to `ui_scale::steps` by the
    // static_assert beside them, so the guard is that selecting item i really
    // renders at steps[i] — the label→index→transform chain, with the percent
    // read from the ladder rather than parsed out of the label.
    bool labelsMatchTransform = true;
    for (int i = 0; i < box->getNumItems(); ++i)
    {
        box->setSelectedItemIndex (i, juce::sendNotificationSync);
        if (box->getItemText (i) != ui_scale::names[i]
            || std::abs (rendered() * 100.0f - (float) ui_scale::steps[i]) > 0.5f)
            labelsMatchTransform = false;
    }
    check (labelsMatchTransform,
           "uiScaleClamp: every combo label names the step that item actually renders at");

    // The load direction, which is where the two used to part company: the box
    // holds a legal selection and the stored value changes to an illegal one.
    proc.internalState.state().setProperty (iid::uiScale, 150, nullptr);
    ed->refreshInternalSettingsBoxes();
    check (box->getText() == "XL" && std::abs (rendered() - 1.50f) < 1.0e-4f,
           "uiScaleClamp: (premise) a legal stored step reaches both halves");
    proc.internalState.state().setProperty (iid::uiScale, 130, nullptr);
    ed->refreshInternalSettingsBoxes();
    check (box->getText() == "L",
           "uiScaleClamp: an illegal value arriving by LOAD moves the box off the stale step");
    check (std::abs (rendered() - 1.25f) < 1.0e-4f,
           "uiScaleClamp: …to the same step the window renders at");

    // CONVERGENCE, and it happens where the state is ADOPTED rather than on the
    // display poll. Clamping on read alone left an illegal value in
    // `InternalState` for ever — every save re-serialised it, so the session
    // never healed — but correcting it from `refreshInternalSettingsBoxes` made
    // a 24 Hz display timer a writer of the wrapper's tree, opposite
    // `setStateInformation` → `replaceFrom`, which VST3 does not promise on the
    // message thread (KI-003). `replaceFrom` applies the ladder read rule now,
    // beside every other field's, so the poll only reads. The stimulus is
    // therefore a session LOAD, not a poll.
    auto loadWithScale = [&proc] (int pct)
    {
        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        auto root = juce::ValueTree::fromXml (
            *juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize()));
        root.getChildWithName ("ANABASIS_INTERNAL").setProperty (iid::uiScale, pct, nullptr);
        juce::MemoryBlock in;
        juce::AudioProcessor::copyXmlToBinary (*root.createXml(), in);
        proc.setStateInformation (in.getData(), (int) in.getSize());
    };
    auto storedScale = [&proc] { return (int) proc.internalState.state().getProperty (iid::uiScale, -1); };

    loadWithScale (130);
    check (storedScale() == 125,
           "uiScaleClamp: an illegal stored value is normalised at adoption, not just clamped on read");
    ed->refreshInternalSettingsBoxes();
    check (box->getText() == "L" && std::abs (rendered() - 1.25f) < 1.0e-4f,
           "uiScaleClamp: …and the editor shows the step it converged on");

    // A LEGAL value is never rewritten — the convergence must not disturb a
    // scale the user actually chose.
    loadWithScale (85);
    check (storedScale() == 85, "uiScaleClamp: a legal stored step is left exactly as the user set it");
    ed->refreshInternalSettingsBoxes();
    check (std::abs (rendered() - 0.85f) < 1.0e-4f, "uiScaleClamp: …and renders at it");

    // THE CASE A BRANCH-OWNED CONVERGENCE MISSED, kept because it is the one
    // that made the old defect invisible: an illegal value whose nearest step is
    // the one ALREADY DISPLAYED. 92 sits nearer 85 than 100 on the 2026-08-05
    // ladder (unambiguously — a midpoint would be a tie the ladder resolves by
    // order, a different thing to test), so the re-seed's "has the selection
    // changed?" branch is false, and while that branch owned the write-back
    // nothing converged. Adoption does not consult the display at all, so the
    // case is no longer special.
    loadWithScale (92);
    check (storedScale() == 85,
           "uiScaleClamp: an illegal value converges even when the DISPLAYED step does not move");
    ed->refreshInternalSettingsBoxes();
    check (box->getText() == "S" && std::abs (rendered() - 0.85f) < 1.0e-4f,
           "uiScaleClamp: …and the box and the transform agree with the converged value");

    // MISSING FIELD → THE DEFAULT, which is §4.4's read rule and the one case
    // the ladder can get wrong in a way that looks plausible: an absent property
    // reads as `var()`, `var()` converts to 0, and 0's NEAREST step is 75 — so a
    // read without a stated default turns "not present" into the smallest legal
    // scale rather than the default one. Reachable from a hand-edited session or
    // one written before the field existed.
    {
        AnabasisAudioProcessor bare;
        juce::MemoryBlock mb;
        bare.getStateInformation (mb);
        auto root = juce::ValueTree::fromXml (
            *juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize()));
        auto internal = root.getChildWithName ("ANABASIS_INTERNAL");
        check (internal.isValid() && internal.hasProperty (iid::uiScale),
               "uiScaleClamp: (premise) a written session carries the field");
        internal.removeProperty (iid::uiScale, nullptr);
        juce::MemoryBlock in;
        juce::AudioProcessor::copyXmlToBinary (*root.createXml(), in);
        bare.setStateInformation (in.getData(), (int) in.getSize());
        check ((int) bare.internalState.state().getProperty (iid::uiScale, -1) == 100,
               "uiScaleClamp: a session missing the field loads at the DEFAULT step, not the smallest");
    }

    // THE POLL WRITES NOTHING. The whole point of moving the correction: an
    // illegal percent written straight into the live tree (which only a test can
    // do — `replaceFrom` is the real entry) must survive the display refresh
    // untouched, while still being clamped on read.
    proc.internalState.state().setProperty (iid::uiScale, 130, nullptr);
    ed->refreshInternalSettingsBoxes();
    check (storedScale() == 130,
           "uiScaleClamp: the display poll does not write the InternalState tree");
    check (box->getText() == "L" && std::abs (rendered() - 1.25f) < 1.0e-4f,
           "uiScaleClamp: …and still renders and displays the clamped step");
}

// Round 42. `CurveView::paint` rebuilt its curve on every repaint — a full
// `MasteringEQ::prepare` plus one `magnitudeDbAt` per pixel column — although
// `refresh()` already gates the repaint on a fingerprint of the parameters and
// the sample rate. Caching against THAT fingerprint is only safe if the cached
// frame is pixel-identical to the rebuilt one, which is what this asserts: the
// cache is invisible, or it is a bug.
// Round 49. `AnabasisEngine::prepare` rewinds the spectrum rings so frames
// captured at the previous sample rate become unreachable (round 39). That was
// only half of it: `SpectrumView::analyse` returns immediately when
// `readLatest` yields nothing — exactly the post-rewind state — so the view
// kept its previous EMA and went on drawing the OLD analysis against the NEW
// rate's bin mapping, which is the artefact the rewind was added to remove.
//
// This is the LIFECYCLE edge only. The "should an idle analyser decay?"
// question is a different branch and a listening-pass call (KI-007 item 6); it
// is untouched, and this test says nothing about it.
static void testARewoundSpectrumRingDropsThePreviousTrace()
{
    AnabasisAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    SpectrumView view (proc);

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, 512);
    auto feed = [&] (int blocks, float amp)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                const float v = amp * std::sin (2.0f * juce::MathConstants<float>::pi
                                                * 1000.0f * (float) (b * 512 + n) / 48000.0f);
                buf.setSample (0, n, v);
                buf.setSample (1, n, v);
            }
            proc.processBlock (buf, midi);
        }
    };

    // Enough frames to fill the 4096-point window and pull the EMA well off the
    // floor, then let the view analyse them.
    feed (32, 0.5f);
    view.tick (0.05);
    const auto& inDb = view.analysedInDb();
    const float loudest = *std::max_element (inDb.begin(), inDb.end());
    check (loudest > -100.0f,
           "spectrumReset: (premise) the analyser has a real trace to lose");

    // A FORWARD count change must NOT clear. Asserting only "still above the
    // floor" cannot see the difference — a clear followed by `analyse` in the
    // same tick rebuilds instantly, because the EMA's attack is a straight
    // assignment (`db > s ? db : …`). What a spurious clear DOES destroy is the
    // decay: feed material 40 dB quieter and the trace must be found part-way
    // down from the loud reading, not sitting at the quiet one.
    feed (16, 0.005f);
    view.tick (0.05);
    const float decaying = *std::max_element (inDb.begin(), inDb.end());
    check (decaying > loudest - 30.0f,
           "spectrumReset: a forward count change decays the trace, it does not reset it");

    // The lifecycle transition. `prepareToPlay` rewinds both rings; the very
    // next tick must not still be showing the pre-prepare analysis.
    proc.prepareToPlay (96000.0, 512);
    view.tick (0.05);
    const float afterReset = *std::max_element (inDb.begin(), inDb.end());
    check (afterReset <= -120.0f,
           "spectrumReset: a re-prepare drops the trace the rewound ring can no longer justify");

    // THE CASE A BACKWARDS-COUNT PREDICATE CANNOT SEE, and the reason the rings
    // carry a reset generation. `ci < shownInCount` is true only while the
    // observed count is still below the one the last tick stored; feed enough
    // audio between the reset and the next tick and the count is LARGER again,
    // so the rewind is missed outright and no later tick can notice, because
    // every count from then on is larger still. One delayed tick is all it
    // takes — the message thread suspended, a debugger stop, a host that
    // batches redraws — and the symptom is the pre-reset EMA drawn against the
    // new rate's bin mapping, silently, for the rest of the session.
    //
    // Reproduced exactly: build a real trace, re-prepare, then push MORE frames
    // than the pre-reset count before ticking, so the count the tick observes is
    // LARGER than the one it stored. Only the generation says a reset happened.
    //
    // The post-reset material is 40 dB QUIETER, for the round-49 reason the
    // forward-change check above spells out: "still above the floor" cannot
    // separate the two outcomes, because a clear followed by `analyse` in the
    // same tick rebuilds instantly (the EMA's attack is a straight assignment).
    // What separates them is where the trace LANDS — cleared, it takes the
    // quiet material's level directly; missed, it decays one step from the loud
    // reading and stays near it.
    feed (32, 0.5f);
    view.tick (0.05);
    const float before = *std::max_element (inDb.begin(), inDb.end());
    check (before > -100.0f, "spectrumReset: (premise) a trace to lose again");
    const auto countBefore = proc.spectrumInRing().writeCount();

    proc.prepareToPlay (48000.0, 512);
    feed (64, 0.005f);     // > the pre-reset frame count: the counter never dips
    check (proc.spectrumInRing().writeCount() > countBefore,
           "spectrumReset: (premise) the observed count is HIGHER than before the reset");
    view.tick (0.05);
    check (*std::max_element (inDb.begin(), inDb.end()) < before - 30.0f,
           "spectrumReset: a reset is caught even when the count never goes backwards");
}

// The graph-well views are interactive over their corner mode chips and INERT
// everywhere else. The spectrum view used to leave `setInterceptsMouseClicks`
// at JUCE's default, so it hit-tested true across its whole area and consumed
// every click in the metering strip with no affordance and no effect — the one
// region of the editor that took a click and did nothing; it opts out per-pixel
// through `hitTest`, and since the combined well (2026-08-05) `GrHistoryView`
// carries the mirrored "SPEC" chip and does the same (`CurveView` still opts
// out wholesale). Both halves are checked for BOTH views: a click elsewhere
// must not be claimed, and the chip must flip `int_spectrumOn` its way.
// The About overlay opened BLANK: `Backdrop::aboutText` had one reader — the
// dismiss-anywhere branch in `mouseDown` — so the flag named a panel whose copy
// nothing painted, and `ANABASIS_VERSION_STRING`/`ANABASIS_BUILD_NUMBER` had no
// consumer in the tree at all while `CI_CD.md` still described the run number as
// "the About-box build number".
//
// Asserted as HORIZONTAL VARIATION rather than a pixel count or a glyph match,
// which is what makes it font-independent: the empty panel is a vertical glass
// gradient, so every row is near-constant across x, while any rendered copy
// breaks that. A blank panel therefore fails no matter which font the host
// machine resolves, and the check needs no threshold tuned to one of them.
// Saving a preset over a FACTORY name used to leave the remembered source
// pointing at the factory entry, and the unchanged display name then CONFIRMED
// that stale hint — so the ‹ › arrows walked the factory ring from an entry the
// user had just replaced, silently changing the sound. Names cannot resolve this
// (they are not unique across the two collections, which is why the remembered
// source exists at all), so the check distinguishes the two by CONTENT: the
// saved preset carries a parameter value the factory table does not name, and
// therefore leaves at its default.
//
// This is the one test that writes into the REAL user preset directory, because
// the save button and `stepPreset` both resolve it themselves and neither takes
// an injected path. It restores whatever it displaced.
static void testSavingOverAFactoryNameKeepsTheArrowsOnTheUserPreset()
{
    int factoryCount = 0;
    const auto* factory = PresetManager::factoryPresets (factoryCount);
    check (factoryCount > 3, "saveSource: (premise) the factory bank has room to step");
    if (factoryCount <= 3)
        return;

    const juce::String clashName (factory[2].name);
    const auto dir  = PresetManager::userPresetDirectory();
    const auto file = dir.getChildFile (clashName + ".anabasis");

    // Displace nothing: a developer machine may already hold a preset of this
    // name, and the whole scenario requires that exact filename.
    const auto backup = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("anabasis-savesource-backup.anabasis");
    const bool hadFile = file.existsAsFile();
    if (hadFile)
        file.copyFileTo (backup);
    struct Restore
    {
        juce::File f, b; bool had;
        ~Restore() { f.deleteFile(); if (had) { b.copyFileTo (f); b.deleteFile(); } }
    } restore { file, backup, hadFile };

    AnabasisAudioProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
    auto* ed = dynamic_cast<AnabasisAudioProcessorEditor*> (base.get());
    check (ed != nullptr, "saveSource: (premise) the editor was created");
    if (ed == nullptr)
        return;

    auto* prev = findButtonByText (*ed, juce::String::charToString ((juce::juce_wchar) 0x2039));
    auto* next = findButtonByText (*ed, juce::String::charToString ((juce::juce_wchar) 0x203A));
    auto* save = findButtonByText (*ed, "Save");
    auto* nameBox = findTextEditor (*ed);
    check (prev != nullptr && next != nullptr && save != nullptr && nameBox != nullptr,
           "saveSource: (premise) the ring arrows and the save controls were found");
    if (prev == nullptr || next == nullptr || save == nullptr || nameBox == nullptr)
        return;

    // Land on the factory entry whose name we are about to take, so the live
    // identity really is the factory row when the save moves it (ADR-0022 —
    // this used to stage a stale editor-local hint; the identity replaced it).
    check (proc.applyFactoryPreset (2), "saveSource: (premise) the factory preset applies");

    // A value the table does NOT name — a factory apply therefore parks it at
    // its default, and only the SAVED preset carries this number.
    auto* knee = proc.apvts.getParameter (pid::compKnee);
    const float mark = knee->getNormalisableRange().convertTo0to1 (1.0f);
    knee->setValueNotifyingHost (mark);
    check (! juce::exactlyEqual (knee->getValue(), knee->getDefaultValue()),
           "saveSource: (premise) the marker value is not the default");

    nameBox->setText (clashName, juce::dontSendNotification);
    save->onClick();
    check (file.existsAsFile(), "saveSource: (premise) the preset saved under the factory's name");
    check (proc.currentPresetName() == clashName,
           "saveSource: (premise) the live name now collides with the factory entry");

    // Next then previous is exactly reversible through the ring, so a correctly
    // resolved position returns to the SAVED preset. Under name resolution it
    // returns to the factory entry instead — same name, different content.
    next->onClick();
    prev->onClick();
    check (std::abs (knee->getValue() - mark) < 1.0e-6f,
           "saveSource: the arrows step from the saved USER preset, not the same-named factory one");
}

// ---------------------------------------------------------------------------
// A preset file that cannot be applied must not TRAP the ‹ › ring. Resolving
// the ring's position from the ADR-0022 identity makes this a live hazard
// rather than a hypothetical: the identity moves only on a SUCCESSFUL apply,
// so a step that lands on a corrupt file leaves the position where it was and
// the next press recomputes the same row — one bad `.anabasis` in the folder
// would wall the arrows off in that direction for ever. The ring therefore
// keeps walking until an entry actually loads, which is the ungated-hint
// rationale the identity replaced, carried forward.
static void testTheRingWalksPastAnUnreadablePreset()
{
    int factoryCount = 0;
    const auto* factory = PresetManager::factoryPresets (factoryCount);
    check (factoryCount >= 2, "ring: (premise) factory presets ship");
    if (factoryCount < 2)
        return;

    // Adjacent by construction: one prefix, "-1-" before "-2-", so the sorted
    // list cannot put anything between them and the corrupt row is exactly the
    // one a ‹ press from the readable row must step over.
    const auto dir = PresetManager::userPresetDirectory();
    dir.createDirectory();
    const auto corrupt  = dir.getChildFile ("AnabasisRingHarness-1-corrupt.anabasis");
    const auto readable = dir.getChildFile ("AnabasisRingHarness-2-good.anabasis");
    struct RemoveRingFiles      // real preset folder, so RAII (see the identity tests)
    {
        juce::File a, b;
        ~RemoveRingFiles() { a.deleteFile(); b.deleteFile(); }
    } removeRingFiles { corrupt, readable };

    AnabasisAudioProcessor proc;
    check (proc.savePresetFile (readable), "ring: (premise) the readable harness preset saved");
    // Refused by `parsePresetFile` (foreign root), which is the reachable
    // failure `applyPresetFile` returns false on.
    check (corrupt.replaceWithText ("<NotAnAnabasisPreset/>"),
           "ring: (premise) the unreadable harness preset staged");
    check (PresetManager::parsePresetFile (corrupt) == nullptr,
           "ring: (premise) the harness file really is unreadable");

    auto rows = []
    {
        auto files = PresetManager::userPresetDirectory()
                         .findChildFiles (juce::File::findFiles, false, "*.anabasis");
        files.sort();
        return files;
    };
    auto rowOf = [&] (const juce::File& f)
    {
        const auto files = rows();
        for (int i = 0; i < files.size(); ++i)
            if (files.getReference (i) == f)
                return factoryCount + i;
        return -1;
    };
    auto row = [&]
    {
        return PresetManager::selectedPresetRow (proc.currentPresetSelection(),
                                                 proc.currentPresetName(),
                                                 factory, factoryCount, rows());
    };

    const int corruptRow  = rowOf (corrupt);
    const int readableRow = rowOf (readable);
    check (corruptRow >= factoryCount && readableRow == corruptRow + 1,
           "ring: (premise) the unreadable row sits immediately before the readable one");
    if (corruptRow < factoryCount || readableRow != corruptRow + 1)
        return;

    std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
    auto* ed = dynamic_cast<AnabasisAudioProcessorEditor*> (base.get());
    check (ed != nullptr, "ring: (premise) the editor was created");
    if (ed == nullptr)
        return;
    auto* prev = findButtonByText (*ed, juce::String::charToString ((juce::juce_wchar) 0x2039));
    check (prev != nullptr, "ring: (premise) the ‹ arrow was found");
    if (prev == nullptr)
        return;

    check (proc.applyPresetFile (readable), "ring: (premise) the readable preset applies");
    check (row() == readableRow, "ring: (premise) the readable preset is the selected row");

    // ONE press. The row below is the corrupt file: the ring must step over it
    // and land on whatever precedes it, not stall on the row it started from.
    prev->onClick();
    const int afterOne = row();
    check (afterOne != readableRow,
           "ring: a ‹ press onto an unreadable preset does not leave the arrows where they were");
    check (afterOne != corruptRow,
           "ring: ...and does not select the unreadable entry either");

    // ...and the ring is still moving afterwards, in both directions.
    prev->onClick();
    check (row() != afterOne, "ring: the next ‹ press keeps moving");
    auto* next = findButtonByText (*ed, juce::String::charToString ((juce::juce_wchar) 0x203A));
    check (next != nullptr, "ring: (premise) the › arrow was found");
    if (next != nullptr)
    {
        const int beforeNext = row();
        next->onClick();
        check (row() != beforeNext, "ring: › moves too");
    }
}

// ---------------------------------------------------------------------------
// ADR-0022, live behaviour: identity — a factory preset's immutable id vs a
// user preset's FILE — decides the selected row, so a user preset saved under
// a factory preset's name is the one selected, both rows stay individually
// selectable, undo and A/B carry the identity, and a known identity that is on
// no row (an outside-folder file, a deleted preset) selects NOTHING — never a
// same-named substitute. The restore matrix lives in
// testPresetIdentityAcrossRestore; the arrows-through-the-editor half is
// testSavingOverAFactoryNameKeepsTheArrowsOnTheUserPreset.
static void testPresetIdentitySharedName()
{
    int factoryCount = 0;
    const auto* factory = PresetManager::factoryPresets (factoryCount);
    const int factoryIdx = 3;   // "EDM Club"
    // The suite's own precedent (testSavingOverAFactoryNameKeepsTheArrows…):
    // a guarded premise, not an unchecked read — the bank's contents are ⊕
    // for the fine review, and a shrunk table must fail loudly, not read OOB.
    check (factoryCount > factoryIdx, "identity: (premise) the factory bank reaches the shared-name row");
    if (factoryCount <= factoryIdx)
        return;
    const juce::String shared (factory[factoryIdx].name);

    const auto dir  = PresetManager::userPresetDirectory();
    dir.createDirectory();
    const auto file = dir.getChildFile (shared + ".anabasis");
    const auto backup = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("anabasis-identity-backup.anabasis");
    const bool hadFile = file.existsAsFile();
    if (hadFile)
        file.copyFileTo (backup);
    struct Restore
    {
        juce::File f, b; bool had;
        ~Restore() { f.deleteFile(); if (had) { b.copyFileTo (f); b.deleteFile(); } }
    } restore { file, backup, hadFile };
    file.deleteFile();   // start from "no user preset of this name"

    AnabasisAudioProcessor proc;
    // The row the resolver answers for the CURRENT state, against the same
    // ordered list the menu and the ‹ › ring build (non-recursive scan,
    // sorted) — recomputed per call because the cases below create and delete
    // files. One int, so "exactly one row is marked" holds by construction;
    // what the cases prove is that it is the RIGHT row, or none.
    auto row = [&]
    {
        auto files = PresetManager::userPresetDirectory()
                         .findChildFiles (juce::File::findFiles, false, "*.anabasis");
        files.sort();
        return PresetManager::selectedPresetRow (proc.currentPresetSelection(),
                                                 proc.currentPresetName(),
                                                 factory, factoryCount, files);
    };
    auto userRowOf = [&] (const juce::File& f)
    {
        auto files = PresetManager::userPresetDirectory()
                         .findChildFiles (juce::File::findFiles, false, "*.anabasis");
        files.sort();
        for (int i = 0; i < files.size(); ++i)
            if (files.getReference (i) == f)
                return factoryCount + i;
        return -1;
    };

    check (row() == 0, "identity: a fresh instance selects the factory Default row");

    // The constructor SEEDS the identity with the name, and this is the case
    // that can tell: with a user preset called "Default" on disk, a fresh
    // instance must still select the FACTORY row — an unseeded (unknown)
    // identity would name-scan and land there too, but only because the
    // factory block is list-front; the seed makes it identity, not luck, and
    // saving/loading that user file must still be able to select the USER row.
    {
        const auto userDefault = dir.getChildFile ("Default.anabasis");
        const auto defBackup = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                   .getChildFile ("anabasis-identity-default-backup.anabasis");
        const bool hadDefault = userDefault.existsAsFile();
        if (hadDefault)
            userDefault.copyFileTo (defBackup);
        // RAII, like the outer harness's `Restore`: this writes into the
        // developer's REAL preset folder, so the un-staging cannot sit on the
        // success path — an early return from a failed premise would leave a
        // stray "Default" user preset that the next run sees as an extra row.
        struct RestoreDefault
        {
            juce::File f, b; bool had;
            ~RestoreDefault() { f.deleteFile(); if (had) { b.copyFileTo (f); b.deleteFile(); } }
        } restoreDefault { userDefault, defBackup, hadDefault };
        const bool staged = [&]
        {
            AnabasisAudioProcessor writer;
            return writer.savePresetFile (userDefault);
        }();
        check (staged, "identity: (premise) a user preset named Default staged");
        if (staged)
        {
            AnabasisAudioProcessor fresh;
            auto files = PresetManager::userPresetDirectory()
                             .findChildFiles (juce::File::findFiles, false, "*.anabasis");
            files.sort();
            check (PresetManager::selectedPresetRow (fresh.currentPresetSelection(),
                                                     fresh.currentPresetName(),
                                                     factory, factoryCount, files) == 0,
                   "identity: a fresh instance selects the factory Default even with a user Default on disk");
        }
    }

    check (proc.applyFactoryPreset (factoryIdx), "identity: (premise) the factory preset applies");
    check (row() == factoryIdx, "identity: the factory preset is selected before any user file exists");

    // The case the split exists for: save a user preset under the factory name.
    check (proc.savePresetFile (file), "identity: saveUser succeeds under a factory preset's name");
    check (proc.currentPresetName() == shared, "identity: the shared name is still what is DISPLAYED");
    const int userIdx = userRowOf (file);
    check (userIdx >= factoryCount, "identity: (premise) the saved file is a user row");
    check (row() == userIdx, "identity: the save SELECTS the user row, not the same-named factory one");

    // The user preset FILE gained nothing from this change: the identity trio
    // lives in the session blob only, so a `.anabasis` written by this build
    // parses to PARAM + DETACH_MASK children and no identity attribute —
    // byte-compatible with what earlier versions wrote (ADR-0022 §Decision 7).
    {
        const auto xml = juce::XmlDocument::parse (file);
        check (xml != nullptr, "identity: (premise) the saved preset file parses");
        bool onlyKnownChildren = xml != nullptr;
        if (xml != nullptr)
        {
            check (! xml->hasAttribute ("presetSource") && ! xml->hasAttribute ("presetFactoryId")
                       && ! xml->hasAttribute ("presetUserFile"),
                   "identity: the preset FILE carries no identity fields");
            for (auto* c : xml->getChildIterator())
                onlyKnownChildren = onlyKnownChildren
                    && (c->hasTagName ("PARAM") || c->hasTagName ("DETACH_MASK"));
        }
        check (onlyKnownChildren, "identity: the preset FILE's children are PARAM/DETACH_MASK only");
    }

    // Both rows remain individually selectable, in both directions.
    check (proc.applyFactoryPreset (factoryIdx), "identity: (premise) re-select the factory row");
    check (row() == factoryIdx, "identity: selecting the factory row returns the mark to it");
    check (proc.applyPresetFile (file), "identity: (premise) re-select the user row");
    check (row() == userIdx, "identity: selecting the user row moves the mark back to it");

    // A/B carries the identity inside the SLOT tree, so a switch away and
    // back does not snap the selection onto the same-named factory row.
    proc.switchToSlot (1);
    proc.switchToSlot (0);
    check (row() == userIdx, "identity: an A/B switch away and back preserves the user identity");

    // Undo after a save keeps the saved preset's identity: the gesture's
    // pre-state snapshot is taken AFTER the save, so the entry it pushes
    // already carries the user-file identity (the pre-state-snapshot shape —
    // Anamorph needed an onSaved re-baseline hook here; this build does not).
    check (proc.applyFactoryPreset (factoryIdx), "identity: (premise) back to the factory row");
    check (proc.savePresetFile (file), "identity: (premise) re-save under the shared name");
    check (row() == userIdx, "identity: the re-save selects the user row");
    {
        auto* limGain = proc.apvts.getParameter (pid::limGain);
        limGain->beginChangeGesture();
        limGain->setValueNotifyingHost (limGain->getNormalisableRange().convertTo0to1 (2.0f));
        limGain->endChangeGesture();
        proc.flushPendingDetach();
    }
    check (proc.canUndo(), "identity: (premise) the knob edit after the save is undoable");
    proc.undo();
    check (row() == userIdx, "identity: undo after a save keeps the saved preset's identity");

    // A `.anabasis` file from OUTSIDE the preset folder is on no row: nothing
    // is selected — it must NOT fall back to the same-named factory row.
    {
        const auto outside = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile (shared + ".anabasis");
        outside.deleteFile();
        const bool staged = file.copyFileTo (outside);
        check (staged, "identity: (premise) outside-folder copy staged");
        if (staged)
        {
            check (proc.applyPresetFile (outside), "identity: an outside file applies");
            check (proc.currentPresetName() == shared, "identity: an outside file still displays its name");
            check (row() < 0, "identity: an outside file selects nothing, not the same-named factory row");
            outside.deleteFile();
        }
    }

    // Same rule when the selected user preset disappears from disk.
    check (proc.applyPresetFile (file), "identity: (premise) back on the user row");
    check (file.deleteFile(), "identity: (premise) the user preset file removed while selected");
    check (row() < 0, "identity: a deleted user preset selects nothing, not the same-named factory row");
}

// ---------------------------------------------------------------------------
// The factory ids are the identity half of ADR-0022. Nothing in the type
// system stops a duplicated, empty or edited id, and the failure would be
// quiet: a duplicate makes an earlier row answer for a later one, and an
// edited id silently unhooks every saved session that stored the old one.
// These checks make that loud.
static void testFactoryPresetIdIntegrity()
{
    int factoryCount = 0;
    const auto* factory = PresetManager::factoryPresets (factoryCount);

    bool everyIdIsSet = true;
    juce::StringArray ids;
    for (int i = 0; i < factoryCount; ++i)
    {
        everyIdIsSet = everyIdIsSet && factory[i].id != nullptr
                           && juce::String (factory[i].id).isNotEmpty();
        ids.add (factory[i].id);
    }
    check (factoryCount >= 2, "factoryId: (premise) factory presets ship");
    check (everyIdIsSet, "factoryId: every factory row carries a non-empty id");

    juce::StringArray uniqueIds (ids);
    uniqueIds.removeDuplicates (false);   // case-SENSITIVE: the ids are exact tokens
    check (uniqueIds.size() == ids.size(), "factoryId: the ids are unique");

    // Every id RESOLVES to its own row: apply row i, and the identity the
    // wrapper recorded must select row i again through the resolver. A
    // duplicated or mistyped id would land on the wrong row here, positionally
    // visible. And the same loop carries Anamorph's cardinality check in
    // Anabasis-observable form: every applied preset's FULL raw snapshot is
    // compared against a fresh instance's, and exactly ONE row — index 0, the
    // empty override table — may land on the all-defaults sound. A second one
    // would mean an override table that silently applied nothing.
    const auto defaultsRaw = [&]
    {
        AnabasisAudioProcessor pristine;
        std::vector<float> v;
        for (auto* param : pristine.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
                v.push_back (rp->getValue());
        return v;
    }();
    AnabasisAudioProcessor proc;
    const juce::Array<juce::File> noFiles;   // factory resolution needs no user list
    int atDefaults = 0;
    for (int i = 0; i < factoryCount; ++i)
    {
        proc.applyFactoryPreset (i);
        const juce::String m = "factoryId: id round-trips to its own row ("
                                   + juce::String (factory[i].id) + ")";
        check (PresetManager::selectedPresetRow (proc.currentPresetSelection(),
                                                 proc.currentPresetName(),
                                                 factory, factoryCount, noFiles) == i,
               m.toRawUTF8());

        size_t k = 0;
        bool same = true;
        for (auto* param : proc.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
                same = same && k < defaultsRaw.size()
                            && juce::exactlyEqual (rp->getValue(), defaultsRaw[k++]);
        if (same && k == defaultsRaw.size())
            ++atDefaults;
    }
    check (atDefaults == 1,
           "factoryId: exactly one factory preset is the all-defaults one — every other table applies");
}

// ---------------------------------------------------------------------------
// ADR-0022's restore matrix: the identity is carried in the session (three
// additive strings per SLOT), so reopening a project puts the selection back
// on the row that produced the sound — even against a same-named factory
// preset — and anything the stored identity cannot resolve selects NOTHING.
// Every path asserts the restored PARAMETERS are bit-identical, because the
// identity is metadata and must never influence the sound, including when it
// fails to resolve.
static void testPresetIdentityAcrossRestore()
{
    int factoryCount = 0;
    const auto* factory = PresetManager::factoryPresets (factoryCount);
    const int factoryIdx = 3;   // "EDM Club"
    check (factoryCount > factoryIdx, "restoreId: (premise) the factory bank reaches the shared-name row");
    if (factoryCount <= factoryIdx)
        return;                 // same guarded-premise rule as the live test
    const juce::String shared (factory[factoryIdx].name);

    auto rawSnapshot = [] (AnabasisAudioProcessor& p)
    {
        std::vector<float> v;
        for (auto* param : p.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
                v.push_back (rp->getValue());
        return v;
    };
    auto sameRaw = [] (const std::vector<float>& a, const std::vector<float>& b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (! juce::exactlyEqual (a[i], b[i]))
                return false;
        return true;
    };
    auto filesNow = []
    {
        auto files = PresetManager::userPresetDirectory()
                         .findChildFiles (juce::File::findFiles, false, "*.anabasis");
        files.sort();
        return files;
    };
    // Restore `blob` into a NEW processor and report (row, name, params-match).
    struct Restored { int row; juce::String name; bool paramsMatch; };
    auto restoreInto = [&] (const juce::MemoryBlock& blob, const std::vector<float>& expectRaw)
    {
        AnabasisAudioProcessor q;
        q.setStateInformation (blob.getData(), (int) blob.getSize());
        return Restored { PresetManager::selectedPresetRow (q.currentPresetSelection(),
                                                            q.currentPresetName(),
                                                            factory, factoryCount, filesNow()),
                          q.currentPresetName(),
                          sameRaw (rawSnapshot (q), expectRaw) };
    };
    auto unwrap = [] (const juce::MemoryBlock& blob)
    {
        const auto xml = juce::AudioProcessor::getXmlFromBinary (blob.getData(), (int) blob.getSize());
        return xml != nullptr ? juce::ValueTree::fromXml (*xml) : juce::ValueTree();
    };
    auto wrap = [] (const juce::ValueTree& root)
    {
        juce::MemoryBlock mb;
        if (const auto xml = root.createXml())
            juce::AudioProcessor::copyXmlToBinary (*xml, mb);
        return mb;
    };
    auto slotOf = [] (const juce::ValueTree& root, int slotIndex)
    {
        juce::Array<juce::ValueTree> slots;
        const auto ab = root.getChildWithName ("AB");
        for (int i = 0; i < ab.getNumChildren(); ++i)
            if (ab.getChild (i).hasType ("SLOT"))
                slots.add (ab.getChild (i));
        return slotIndex < slots.size() ? slots[slotIndex] : juce::ValueTree();
    };

    const auto dir  = PresetManager::userPresetDirectory();
    dir.createDirectory();
    const auto file = dir.getChildFile (shared + ".anabasis");
    const auto backup = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("anabasis-identity-restore-backup.anabasis");
    const bool hadFile = file.existsAsFile();
    if (hadFile)
        file.copyFileTo (backup);
    struct Restore
    {
        juce::File f, b; bool had;
        ~Restore() { f.deleteFile(); if (had) { b.copyFileTo (f); b.deleteFile(); } }
    } restoreFiles { file, backup, hadFile };
    file.deleteFile();

    AnabasisAudioProcessor p;

    // Schema pin (SERIALIZATION_REGISTRY §1.2): both SLOTs carry the trio.
    {
        juce::MemoryBlock pristine;
        p.getStateInformation (pristine);
        const auto root = unwrap (pristine);
        for (int s = 0; s < 2; ++s)
        {
            const auto slot = slotOf (root, s);
            const juce::String m = "restoreId: SLOT " + juce::String (s)
                                       + " carries presetSource/presetFactoryId/presetUserFile";
            check (slot.hasProperty ("presetSource") && slot.hasProperty ("presetFactoryId")
                       && slot.hasProperty ("presetUserFile"),
                   m.toRawUTF8());
            // ...and a FRESH instance's slots record the factory Default by
            // ID, not an empty identity: the constructor seeds the selection
            // with the name, so the saved session says WHICH row produced the
            // untouched state — the name fallback happens to give the same
            // row today only because the factory block is list-front, which
            // is luck, not a record (ADR-0022; the ctor comment).
            const juce::String mSeed = "restoreId: SLOT " + juce::String (s)
                                           + " of a fresh session records the factory Default id";
            check (slot.getProperty ("presetSource").toString() == "factory"
                       && slot.getProperty ("presetFactoryId").toString() == "default",
                   mSeed.toRawUTF8());
        }
    }

    // --- Case 1: a FACTORY preset is current --------------------------------
    check (p.applyFactoryPreset (factoryIdx), "restoreId: (premise) the factory preset applies");
    const auto factoryRaw = rawSnapshot (p);
    juce::MemoryBlock factoryBlob;
    p.getStateInformation (factoryBlob);
    {
        const auto r = restoreInto (factoryBlob, factoryRaw);
        check (r.row == factoryIdx, "restoreId: a restored session selects the factory preset it was on");
        check (r.name == shared, "restoreId: the displayed name restores");
        check (r.paramsMatch, "restoreId: factory case restores parameters bit-identically");
    }

    // --- Case 1 fallback: the stored factory id no longer exists ------------
    {
        auto root = unwrap (factoryBlob);
        slotOf (root, 0).setProperty ("presetFactoryId", "aPresetThatWasRemoved", nullptr);
        const auto r = restoreInto (wrap (root), factoryRaw);
        check (r.row < 0, "restoreId: an unresolvable factory id selects NOTHING");
        check (r.paramsMatch, "restoreId: unresolvable-id case still restores parameters bit-identically");
    }

    // --- Case 2: a USER preset sharing the factory name is current ----------
    {
        auto* knee = p.apvts.getParameter (pid::compKnee);
        knee->setValueNotifyingHost (knee->getNormalisableRange().convertTo0to1 (1.0f));
    }
    check (p.savePresetFile (file), "restoreId: (premise) a user preset saved under the factory name");
    const auto userRaw = rawSnapshot (p);
    juce::MemoryBlock userBlob;
    p.getStateInformation (userBlob);
    const int userIdx = [&]
    {
        const auto files = filesNow();
        for (int i = 0; i < files.size(); ++i)
            if (files.getReference (i) == file)
                return factoryCount + i;
        return -1;
    }();
    check (userIdx >= factoryCount, "restoreId: (premise) the saved file is a user row");
    {
        const auto r = restoreInto (userBlob, userRaw);
        check (r.row == userIdx, "restoreId: a restored session selects the USER row, not the same-named factory one");
        check (r.name == shared, "restoreId: the shared display name restores");
        check (r.paramsMatch, "restoreId: user case restores parameters bit-identically");
    }

    // --- Case 2, nested: a preset in a SUB-folder of the preset folder ------
    // Every list this build shows is a non-recursive scan, so a nested file is
    // on no row and must select nothing — before AND after a reload. This is
    // the case where encoding by bare file NAME would silently re-point the
    // identity at the same-named preset sitting directly in the folder, which
    // still exists at this point (the direct-child-not-isAChildOf condition).
    {
        auto nestedDir = dir.getChildFile ("AnabasisHarnessNested");
        auto nested    = nestedDir.getChildFile (shared + ".anabasis");
        nested.deleteFile();
        // RAII for the same reason the file harnesses use it: the sub-folder
        // is created inside the developer's REAL preset folder, so removing it
        // on the success path only would leave an empty directory behind after
        // an early return or a failed staging.
        struct RemoveNested
        {
            juce::File d;
            ~RemoveNested() { d.deleteRecursively(); }
        } removeNested { nestedDir };
        const bool staged = nestedDir.createDirectory() && file.copyFileTo (nested);
        check (staged, "restoreId: (premise) nested sub-folder copy staged");
        if (staged)
        {
            check (p.applyPresetFile (nested), "restoreId: (premise) a nested preset applies");
            const auto nestedRaw = rawSnapshot (p);
            juce::MemoryBlock nestedBlob;
            p.getStateInformation (nestedBlob);
            const auto r = restoreInto (nestedBlob, nestedRaw);
            check (r.row < 0,
                   "restoreId: a reloaded nested preset selects nothing, not the same-named flat row");
            check (r.paramsMatch, "restoreId: nested case restores parameters bit-identically");
            nested.deleteFile();
        }
    }

    // --- Case 2 fallback: the user preset file is gone ----------------------
    {
        check (file.deleteFile(), "restoreId: (premise) the user preset file removed");
        const auto r = restoreInto (userBlob, userRaw);
        check (r.row < 0, "restoreId: a missing user preset selects NOTHING, not the same-named factory row");
        check (r.name == shared, "restoreId: the display name still restores for a missing file");
        check (r.paramsMatch, "restoreId: missing-file case still restores parameters bit-identically");
    }

    // --- Case 3: a pre-ADR-0022 session, no identity stored ------------------
    {
        auto root = unwrap (userBlob);
        for (int s = 0; s < 2; ++s)
        {
            auto slot = slotOf (root, s);
            slot.removeProperty ("presetSource",    nullptr);
            slot.removeProperty ("presetFactoryId", nullptr);
            slot.removeProperty ("presetUserFile",  nullptr);
        }
        check (! slotOf (root, 0).hasProperty ("presetSource"),
               "restoreId: (premise) the pre-ADR-0022 fixture really has no identity");
        // The file is still deleted, so the ONLY thing the name can resolve to
        // is the factory row — exactly the documented pre-ADR-0022 answer.
        const auto r = restoreInto (wrap (root), userRaw);
        check (r.row == factoryIdx, "restoreId: a session with no stored identity falls back to the name");
        check (r.paramsMatch, "restoreId: the name-fallback case still restores parameters bit-identically");
    }

    // --- A/B: each slot carries its own identity across the reload ----------
    {
        check (p.savePresetFile (file), "restoreId: (premise) the user preset re-created for the A/B check");
        p.switchToSlot (1);
        check (p.applyFactoryPreset (factoryIdx), "restoreId: (premise) slot B takes the factory preset");
        const auto abRaw = rawSnapshot (p);
        juce::MemoryBlock abBlob;
        p.getStateInformation (abBlob);

        AnabasisAudioProcessor q;
        q.setStateInformation (abBlob.getData(), (int) abBlob.getSize());
        auto qRow = [&]
        {
            return PresetManager::selectedPresetRow (q.currentPresetSelection(),
                                                     q.currentPresetName(),
                                                     factory, factoryCount, filesNow());
        };
        check (qRow() == factoryIdx, "restoreId: the restored session lands on slot B's factory identity");
        check (sameRaw (rawSnapshot (q), abRaw), "restoreId: slot B restores parameters bit-identically");
        q.switchToSlot (0);
        check (qRow() == userIdx, "restoreId: slot A's USER identity restores independently of slot B's");
        p.switchToSlot (0);   // leave p back on slot A for the cases below
    }

    auto pRow = [&]
    {
        return PresetManager::selectedPresetRow (p.currentPresetSelection(),
                                                 p.currentPresetName(),
                                                 factory, factoryCount, filesNow());
    };

    // --- Copy carries the identity, and ONLY a real move mints a step -------
    // Copy is a named ADR-0022 carrier: the destination inherits the live
    // SLOT tree, trio included. Three cases, one per way the guard can err:
    // the identity must TRAVEL; an identity-ONLY difference (same name, same
    // sound — the saved-under-a-factory-name shape) is a REAL change and must
    // push a step; and a pre-ADR-0022 stored slot (no trio at all) compared
    // against a fresh save (empty trio) is NOT a change — both decode to
    // `unknown`, and minting a step there is the dead-undo class the guard's
    // own comment forbids.
    {
        // live (slot A) is the user preset from the A/B block above.
        p.copySlotToOther();
        p.switchToSlot (1);
        check (pRow() == userIdx, "restoreId: Copy carries the USER identity to the destination slot");

        // Stage "identical in everything but identity": factory-apply on B,
        // copy it to A, then SAVE on B — the save moves only the identity.
        check (p.applyFactoryPreset (factoryIdx), "restoreId: (premise) slot B takes the factory preset again");
        p.copySlotToOther();                       // A := factory state, identity included
        check (p.savePresetFile (file), "restoreId: (premise) the save that moves only the identity");
        p.copySlotToOther();                       // A: factory identity vs live: user identity
        p.switchToSlot (0);
        check (pRow() == userIdx, "restoreId: an identity-only Copy still lands the user identity");
        check (p.canUndo(), "restoreId: ...and it minted an undo step — an identity move is a real change");
        p.undo();
        check (pRow() == factoryIdx, "restoreId: undoing that Copy restores the factory identity");

        // The phantom case: a pre-ADR-0022 session's stored slot carries no
        // trio; the first Copy after loading it must be the no-op it is.
        AnabasisAudioProcessor q2;
        juce::MemoryBlock pristine;
        q2.getStateInformation (pristine);
        auto root = unwrap (pristine);
        for (int s = 0; s < 2; ++s)
        {
            auto slot = slotOf (root, s);
            slot.removeProperty ("presetSource",    nullptr);
            slot.removeProperty ("presetFactoryId", nullptr);
            slot.removeProperty ("presetUserFile",  nullptr);
        }
        const auto blob = wrap (root);
        q2.setStateInformation (blob.getData(), (int) blob.getSize());
        q2.copySlotToOther();
        q2.switchToSlot (1);
        check (! q2.canUndo(),
               "restoreId: a Copy straight after a pre-ADR-0022 load mints no phantom undo step");

        // …and the OTHER direction on that same pre-ADR-0022 footing, which is
        // the ONLY shape in which `presetName` is the sole discriminator. With
        // no trio on either slot the identity cannot tell them apart, so if the
        // NAME stopped travelling through `strippedForUndoCompare` this Copy
        // would compare equal to what it replaces and silently retract its own
        // step. Every other case is covered by the trio — distinct presets carry
        // distinct identities — so without this leg the name could be stripped
        // and the whole suite would still pass.
        //
        // Staged through the blob rather than a setter: the two slots differ by
        // NAME ALONE, which no public API can produce (`savePresetFile` moves the
        // identity with the name, and `applyFactoryPreset` moves the surface too).
        AnabasisAudioProcessor q3;
        juce::MemoryBlock nameOnly;
        q3.getStateInformation (nameOnly);
        auto nroot = unwrap (nameOnly);
        for (int s = 0; s < 2; ++s)
        {
            auto slot = slotOf (nroot, s);
            slot.removeProperty ("presetSource",    nullptr);
            slot.removeProperty ("presetFactoryId", nullptr);
            slot.removeProperty ("presetUserFile",  nullptr);
            slot.setProperty ("presetName", s == 0 ? "Alpha" : "Beta", nullptr);
        }
        const auto nblob = wrap (nroot);
        q3.setStateInformation (nblob.getData(), (int) nblob.getSize());
        check (q3.currentPresetName() == "Alpha",
               "restoreId: (premise) the active slot loaded the name it was given");
        q3.copySlotToOther();            // "Alpha" over "Beta" — nothing else differs
        q3.switchToSlot (1);
        check (q3.canUndo(),
               "restoreId: with no trio on either slot, a NAME-only Copy is still a real change");
        q3.undo();
        check (q3.currentPresetName() == "Beta",
               "restoreId: ...and undoing it puts the destination's own name back");
    }

    // --- A no-AB restore resets the identity WITH the other slot fields -----
    // `resetSlotFieldsToDefaults` seeds the identity beside the name; without
    // that seed a valid root lacking an AB child would keep the PREVIOUS
    // session's selection under a freshly defaulted name — the chimera the
    // function exists to prevent, landing the mark on a stale user row.
    {
        AnabasisAudioProcessor r2;
        check (r2.applyPresetFile (file), "restoreId: (premise) the chimera setup is on the user preset");
        const int r2UserRow = PresetManager::selectedPresetRow (r2.currentPresetSelection(),
                                                                r2.currentPresetName(),
                                                                factory, factoryCount, filesNow());
        check (r2UserRow >= factoryCount, "restoreId: (premise) the user row is selected before the restore");
        juce::MemoryBlock own;
        r2.getStateInformation (own);
        auto root = unwrap (own);
        root.removeChild (root.getChildWithName ("AB"), nullptr);
        const auto blob = wrap (root);
        r2.setStateInformation (blob.getData(), (int) blob.getSize());
        check (r2.currentPresetName() == "Default",
               "restoreId: a no-AB restore defaults the preset name (the existing read rule)");
        check (PresetManager::selectedPresetRow (r2.currentPresetSelection(),
                                                 r2.currentPresetName(),
                                                 factory, factoryCount, filesNow()) == 0,
               "restoreId: ...and the identity resets WITH it — the factory Default row, not the stale user row");
    }

    // --- A user preset whose FILE NAME looks like an absolute path ----------
    // Nothing stops a user dropping `~foo.anabasis` into the preset folder by
    // hand, and `juce::File::isAbsolutePath` accepts a leading `~` on POSIX.
    // Encoding such a direct child by BARE NAME would come back from the
    // decoder as the literal relative string and the row would lose its
    // selection on reload — the encoder must fall back to the absolute path.
    {
        // Built from a full path string on purpose: getChildFile would
        // short-circuit on the very ambiguity under test.
        auto tilde = juce::File (dir.getFullPathName() + juce::File::getSeparatorString()
                                     + "~AnabasisTildeHarness.anabasis");
        tilde.deleteFile();
        struct RemoveTilde        // as above: real preset folder, so RAII
        {
            juce::File f;
            ~RemoveTilde() { f.deleteFile(); }
        } removeTilde { tilde };
        const bool staged = file.copyFileTo (tilde);
        check (staged, "restoreId: (premise) tilde-named preset staged");
        if (staged)
        {
            check (p.applyPresetFile (tilde), "restoreId: (premise) the tilde-named preset applies");
            const int tildeIdx = [&]
            {
                const auto files = filesNow();
                for (int i = 0; i < files.size(); ++i)
                    if (files.getReference (i) == tilde)
                        return factoryCount + i;
                return -1;
            }();
            check (tildeIdx >= factoryCount, "restoreId: (premise) the tilde-named preset is a row");
            const auto tildeRaw = rawSnapshot (p);
            juce::MemoryBlock tildeBlob;
            p.getStateInformation (tildeBlob);
            const auto r = restoreInto (tildeBlob, tildeRaw);
            check (r.row == tildeIdx,
                   "restoreId: a tilde-named preset keeps its selection across a reload (encode round-trips)");
            check (r.paramsMatch, "restoreId: tilde case restores parameters bit-identically");
        }
    }

    // --- A repeated restore must not inherit the previous state's identity --
    // Hosts call setStateInformation on ONE live processor any number of
    // times. The slot overlay assigns the identity UNCONDITIONALLY (absent
    // trio → unknown), so a stripped blob restored over a live factory
    // selection must resolve from the blob alone: its name matches no row →
    // nothing selected — never the previous state's row.
    {
        check (p.applyFactoryPreset (1), "restoreId: (premise) the live instance sits on a factory row");
        auto root = unwrap (factoryBlob);
        auto slot = slotOf (root, 0);
        slot.removeProperty ("presetSource",    nullptr);
        slot.removeProperty ("presetFactoryId", nullptr);
        slot.removeProperty ("presetUserFile",  nullptr);
        slot.setProperty ("presetName", "NoSuchPresetAnywhere", nullptr);
        const auto blob = wrap (root);
        p.setStateInformation (blob.getData(), (int) blob.getSize());   // SAME live instance
        check (p.currentPresetName() == "NoSuchPresetAnywhere",
               "restoreId: a repeated restore takes the session's name, not the previous state's");
        check (PresetManager::selectedPresetRow (p.currentPresetSelection(),
                                                 p.currentPresetName(),
                                                 factory, factoryCount, filesNow()) < 0,
               "restoreId: ...and the previous state's identity does not survive it");
    }
}

static void testTheAboutPanelShowsTheBuildItIsRunning()
{
    AnabasisAudioProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
    auto* ed = dynamic_cast<AnabasisAudioProcessorEditor*> (base.get());
    check (ed != nullptr, "about: (premise) the editor was created");
    if (ed == nullptr)
        return;

    // Through the real path: the wordmark's invisible hit-area button, found by
    // the component ID the LookAndFeel already keys on.
    auto* title = findButtonById (*ed, "ghost");
    check (title != nullptr && title->onClick != nullptr,
           "about: (premise) the wordmark opens the panel");
    if (title == nullptr || title->onClick == nullptr)
        return;
    title->onClick();

    // The same expression `resized()` centres the panel with — 440×290 since
    // the About layout took the sibling's geometry (2026-08-05). It read
    // 400×232 for a commit after that, so the sampled band no longer lined up
    // with the content inset and the heuristic was measuring a different strip
    // than it claimed to (it still passed, which is why it needed reading).
    //
    // The copy area is the panel's content inset, `reduced (30, 26)` — the same
    // expression `Backdrop::paint` uses — trimmed to the height the FLOW copy
    // stack actually occupies: 38 title + 20 subtitle + 14 + 18 version
    // + 18 vendor + 10 + 60 description = 178 px, of the 238 the inset leaves
    // (the copyright is bottom-ANCHORED since 0.1.1 — the sibling's geometry —
    // so it no longer belongs to this stack). Sampling a 200 px band keeps the
    // "textured rows" count about the copy rather than the glass beneath it.
    const auto panel = ed->getLocalBounds().withSizeKeepingCentre (440, 290);
    const auto copyArea = panel.reduced (30, 26).withHeight (200);
    const auto shot = ed->createComponentSnapshot (copyArea, false);
    check (shot.getWidth() == copyArea.getWidth() && shot.getHeight() > 0,
           "about: (premise) the panel's copy area rendered");

    int texturedRows = 0;
    for (int y = 0; y < shot.getHeight(); ++y)
    {
        int lo = 255, hi = 0;
        for (int x = 0; x < shot.getWidth(); ++x)
        {
            const int b = shot.getPixelAt (x, y).getBrightness() > 0.0f
                            ? (int) (shot.getPixelAt (x, y).getBrightness() * 255.0f) : 0;
            lo = juce::jmin (lo, b);
            hi = juce::jmax (hi, b);
        }
        if (hi - lo > 30)
            ++texturedRows;
    }
    check (texturedRows >= 20,
           "about: the panel paints product copy, not an empty glass rectangle");
}

// 0.1.1 owner directive: frequency text entry speaks mastering shorthand,
// classed by knob range. The owner's examples pin BOTH sides of the bells'
// pivot — "19" (below the 20 Hz floor, illegal as Hz) reads 19 kHz while
// "20" (the floor itself, legal) reads 20 Hz — and the all-kHz high shelf
// takes the sibling's `khzFrom` rule verbatim (bare ≤ 20 → kHz). Each case
// goes text → parser → normalise → denormalise through the REAL parameter,
// so the range's snap/clamp participates exactly as it does in a host or
// the value box.
static void testFrequencyTextEntrySpeaksMasteringShorthand()
{
    AnabasisAudioProcessor proc;
    auto hz = [&] (const char* id, const char* text) -> float
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (id));
        return p->convertFrom0to1 (p->getValueForText (text));
    };
    auto near = [] (float got, float want) { return std::abs (got - want) < 0.51f; };

    // HS Freq (1000–20000, all-kHz knob): bare ≤ 20 is kHz.
    check (near (hz (pid::eqHighShelfFreq, "8"),      8000.0f), "freqText: HS '8' lands 8 kHz");
    check (near (hz (pid::eqHighShelfFreq, "8000"),   8000.0f), "freqText: HS '8000' lands 8 kHz");
    check (near (hz (pid::eqHighShelfFreq, "8k"),     8000.0f), "freqText: HS '8k' lands 8 kHz");
    check (near (hz (pid::eqHighShelfFreq, "8 kHz"),  8000.0f), "freqText: HS '8 kHz' lands 8 kHz");
    check (near (hz (pid::eqHighShelfFreq, "5570"),   5570.0f), "freqText: HS '5570' stays Hz");
    check (near (hz (pid::eqHighShelfFreq, "2.38"),   2380.0f), "freqText: HS '2.38' lands 2.38 kHz");

    // Bells (20–20000, full-range knob): the pivot is the knob's own 20 Hz
    // floor, EXCLUSIVE — every legal Hz value stays Hz.
    check (near (hz (pid::eqBell1Freq, "19"),   19000.0f), "freqText: bell '19' (below the floor) lands 19 kHz");
    check (near (hz (pid::eqBell1Freq, "20"),      20.0f), "freqText: bell '20' (the floor) stays 20 Hz");
    check (near (hz (pid::eqBell1Freq, "155"),    155.0f), "freqText: bell '155' stays Hz");
    check (near (hz (pid::eqBell1Freq, "2.38"),  2380.0f), "freqText: bell '2.38' lands 2.38 kHz");
    check (near (hz (pid::eqBell2Freq, "5570"),  5570.0f), "freqText: bell '5570' stays Hz (shown 5.57 kHz)");
    check (near (hz (pid::eqBell2Freq, "8k"),    8000.0f), "freqText: bell '8k' lands 8 kHz");

    // Sub-kHz knobs keep the plain Hz parser (the sibling's convention for
    // its 20–500 Hz knob): a bare number is Hz, k-suffix still honoured.
    check (near (hz (pid::scHpfFreq,      "150"), 150.0f), "freqText: SC HPF '150' stays Hz");
    check (near (hz (pid::eqLowShelfFreq, "0.1k"), 100.0f), "freqText: LS '0.1k' lands 100 Hz");
}

// R2 item 11: every parameter control carries a hover hint. The wording lives
// in ONE table (`tipFor`, file-static in PluginEditor.cpp) which this suite
// cannot reach — what it CAN pin is the outcome: no slider or combo in the
// editor is hoverless, and neither are the named toggles. Toggles are not
// swept wholesale because `bypass` is DELIBERATELY tipless (the red pill
// labels itself — the sibling's rule), so a sweep would either fail on it or
// need the exemption this list states explicitly.
static void collectTooltipless (juce::Component& root, juce::StringArray& names)
{
    for (auto* c : root.getChildren())
    {
        if (auto* sl = dynamic_cast<juce::Slider*> (c); sl != nullptr && sl->getTooltip().isEmpty())
            names.add ("slider \"" + sl->getTitle() + "\"");
        if (auto* bx = dynamic_cast<juce::ComboBox*> (c); bx != nullptr && bx->getTooltip().isEmpty())
            names.add ("combo \"" + bx->getTitle() + "\"");
        collectTooltipless (*c, names);
    }
}

// The §8 focus sweep's collector — same shape as `collectTooltipless`, and
// deliberately a SECOND walk rather than a second condition inside the first:
// the two answer different questions and a combined failure message could not
// say which one failed.
//
// WHAT THIS SWEEP ACTUALLY GUARDS, stated because the obvious reading is
// wrong: only the SLIDERS. JUCE's per-class defaults were read from the
// vendored source — `Slider` ends its constructor `setWantsKeyboardFocus
// (false)`, `Button` sets it true unconditionally, `ComboBox` sets
// `! isLabelEditable` — so combos and toggles were focusable all along and
// their explicit calls in the setup helpers are redundant-but-deliberate.
// Removing the SLIDER call fails this check and names all forty knobs;
// removing the combo call changes nothing observable, which a mutation run
// confirmed. Both facts are recorded so a future reader does not mistake the
// sweep for broader cover than it has.
static void collectUnfocusable (juce::Component& root, juce::StringArray& names)
{
    for (auto* c : root.getChildren())
    {
        if (auto* sl = dynamic_cast<juce::Slider*> (c); sl != nullptr && ! sl->getWantsKeyboardFocus())
            names.add ("slider \"" + sl->getTitle() + "\"");
        if (auto* bx = dynamic_cast<juce::ComboBox*> (c); bx != nullptr && ! bx->getWantsKeyboardFocus())
            names.add ("combo \"" + bx->getTitle() + "\"");
        collectUnfocusable (*c, names);
    }
}

static void testEveryKnobAndComboCarriesATooltip()
{
    AnabasisAudioProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
    auto* ed = dynamic_cast<AnabasisAudioProcessorEditor*> (base.get());
    check (ed != nullptr, "tooltips: (premise) the editor was created");
    if (ed == nullptr)
        return;

    juce::StringArray hoverless;
    collectTooltipless (*ed, hoverless);
    // §8 keyboard operability, landed 0.1.1: every slider and combo ACCEPTS
    // keyboard focus, which is what lets tab traversal reach it and JUCE's own
    // `Slider::keyPressed` arrow handling fire. Swept here rather than in its
    // own test because it is the same walk over the same set — and asserted as
    // "accepts", never "takes": `EDITOR_WANTS_KEYBOARD_FOCUS` stays FALSE so
    // the plugin never steals the host's transport keys.
    juce::StringArray unfocusable;
    collectUnfocusable (*ed, unfocusable);
    if (! unfocusable.isEmpty())
        std::printf ("  unfocusable: %s\n", unfocusable.joinIntoString (", ").toRawUTF8());
    check (unfocusable.isEmpty(),
           "keyboard: every slider and combo accepts keyboard focus (brief section 8)");
    if (! hoverless.isEmpty())
        std::printf ("  tooltipless: %s\n", hoverless.joinIntoString (", ").toRawUTF8());
    check (hoverless.isEmpty(), "tooltips: every slider and combo carries a hover hint");

    // "True-Peak Meter" left this list in 0.1.1 with the toggle and the field
    // behind it (ADR-0020) — the statistics panel shows the true peak
    // unconditionally, so there is no longer a toggle to carry a hint.
    // "COMP" became "MATCH" at 0.1.3 (item 2): the loudness-compensation
    // toggle's caption, renamed so it stops reading as a compressor switch.
    for (auto* text : { "FREEZE", "MATCH", "DELTA", "LOCK", "AUTO", "TP", "SHAPE", "ADV",
                        "UI Animations", "Tooltips" })
    {
        auto* b = findButtonByText (*ed, text);
        // The toggle's NAME in the message, not a shared sentence: this check
        // ran eleven times under one wording, so a failure said only that one
        // of eleven toggles was hoverless and cost a bisect to localise.
        juce::String msg;
        msg << "tooltips: the \"" << text << "\" toggle carries a hint";
        check (b != nullptr && b->getTooltip().isNotEmpty(), msg.toRawUTF8());
    }
}

static void testTheGraphWellViewsOnlyClaimTheirModeChips()
{
    AnabasisAudioProcessor proc;
    auto& ist = proc.internalState.state();

    auto eventFor = [] (juce::Component& c, juce::Point<float> pos)
    {
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(),
                                 pos, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &c, &c, juce::Time::getCurrentTime(), pos,
                                 juce::Time::getCurrentTime(), 1, false);
    };

    // The switch is a two-segment GR|SPEC pill shared by both views
    // (`abgui::graph_switch`) — BOTTOM-LEFT and a WHOLE-PILL TOGGLE since
    // 0.1.2 (items 4+5): any press inside the pill switches the well to the
    // other mode, so no press on it is ever a silent no-op (the 0.1.1
    // side-of-divider semantics made "clicking the active SPEC segment" do
    // nothing, the owner-reported stuck control). Probe points, derived from
    // the one geometry both views key on: the 78×18 pill sits at inset (6, 4)
    // off the bottom-left of a 300×120 view — x 6..84, y 98..116 — so (20,107)
    // lands in the GR (left) half, (70,107) in the SPEC (right) half, and the
    // OLD top-right corner (W−10, 10) must no longer be claimed.

    // Spectrum view: any pill press switches to GR; the trace stays inert.
    {
        SpectrumView view (proc);
        view.setBounds (0, 0, 300, 120);

        check (view.hitTest (20, 107) && view.hitTest (70, 107),
               "spectrumClicks: (premise) both pill segments are hit-tested at the bottom-left");
        check (! view.hitTest (view.getWidth() - 10, 10),
               "spectrumClicks: the old top-right corner is no longer claimed");
        check (! view.hitTest (150, 60),
               "spectrumClicks: a click over the trace is not claimed by the overlay");

        ist.setProperty (iid::spectrumOn, true, nullptr);
        view.mouseDown (eventFor (view, { 150.0f, 60.0f }));
        check ((bool) ist.getProperty (iid::spectrumOn, false),
               "spectrumClicks: a press over the trace does not switch the mode");
        view.mouseDown (eventFor (view, { 70.0f, 107.0f }));
        check (! (bool) ist.getProperty (iid::spectrumOn, true),
               "spectrumClicks: pressing the pill on the active SPEC segment toggles to GR");
        ist.setProperty (iid::spectrumOn, true, nullptr);
        view.mouseDown (eventFor (view, { 20.0f, 107.0f }));
        check (! (bool) ist.getProperty (iid::spectrumOn, true),
               "spectrumClicks: …and pressing its GR segment switches to GR too");
    }

    // GR view: any pill press switches to the spectrum; the trace stays inert.
    {
        GrHistoryView view (proc);
        view.setBounds (0, 0, 300, 120);

        check (view.hitTest (20, 107) && view.hitTest (70, 107),
               "grChip: (premise) both pill segments are hit-tested at the bottom-left");
        check (! view.hitTest (view.getWidth() - 10, 10),
               "grChip: the old top-right corner is no longer claimed");
        check (! view.hitTest (150, 60),
               "grChip: a click over the history trace is not claimed");

        ist.setProperty (iid::spectrumOn, false, nullptr);
        view.mouseDown (eventFor (view, { 150.0f, 60.0f }));
        check (! (bool) ist.getProperty (iid::spectrumOn, true),
               "grChip: a press over the trace does not switch the mode");
        view.mouseDown (eventFor (view, { 20.0f, 107.0f }));
        check ((bool) ist.getProperty (iid::spectrumOn, false),
               "grChip: pressing the pill on the active GR segment toggles to the spectrum");
        ist.setProperty (iid::spectrumOn, false, nullptr);
        view.mouseDown (eventFor (view, { 70.0f, 107.0f }));
        check ((bool) ist.getProperty (iid::spectrumOn, false),
               "grChip: …and pressing its SPEC segment switches to the spectrum too");
    }
}

static void testTheCurveWellCachesWithoutChangingWhatItDraws()
{
    AnabasisAudioProcessor proc;
    // Constructed DIRECTLY rather than fished out of the editor, for two
    // reasons the first version of this test got wrong: the Advanced panel is
    // not laid out in the headless editor, so the well had zero bounds and the
    // test returned before asserting anything (an early return with no check —
    // it reported nothing and proved nothing); and `findChildOfType` returns
    // whichever well comes first, so a stimulus aimed at the EQ parameters
    // might have been driving the clip-transfer curve. Both disappear when the
    // test owns the component and names the mode.
    CurveView curve (proc, CurveView::Mode::eqResponse);
    curve.setBounds (0, 0, 220, 90);

    auto snapshot = [&curve]
    {
        juce::Image img (juce::Image::ARGB, curve.getWidth(), curve.getHeight(), true);
        juce::Graphics g (img);
        curve.paint (g);
        return img;
    };
    auto identical = [] (const juce::Image& a, const juce::Image& b)
    {
        for (int y = 0; y < a.getHeight(); ++y)
            for (int x = 0; x < a.getWidth(); ++x)
                if (a.getPixelAt (x, y) != b.getPixelAt (x, y))
                    return false;
        return true;
    };

    curve.refresh();                          // fingerprint the current state
    const auto first  = snapshot();           // builds
    const auto second = snapshot();           // must come from the cache
    check (identical (first, second),
           "curveCache: a repaint with nothing moved draws exactly what it drew before");

    // …and the cache is not stale: a parameter move plus the refresh that
    // fingerprints it must change the picture. Without this the previous check
    // is satisfied by a cache that never updates at all.
    auto* tilt = proc.apvts.getParameter (pid::eqTilt);
    tilt->setValueNotifyingHost (tilt->getNormalisableRange().convertTo0to1 (4.0f));
    curve.refresh();
    const auto moved = snapshot();
    check (! identical (first, moved),
           "curveCache: a parameter move rebuilds it — the cache follows the fingerprint");

    // THE LABEL IS THE READ. `refresh()` is what the editor timer calls, but it
    // is not what paints: a host expose, an overlay dismissal or a `resized()`
    // reaches `paint()` with no `refresh()` in front of it. When the cache key
    // was the fingerprint `refresh()` last stored, such a repaint compared the
    // new state against a label from the old one, matched, and served geometry
    // built from parameters that had since moved — a stale curve, self-correcting
    // only because the 24 Hz tick came round again. So: move a parameter and
    // paint WITHOUT refreshing.
    auto* bell = proc.apvts.getParameter (pid::eqBell1Gain);
    bell->setValueNotifyingHost (bell->getNormalisableRange().convertTo0to1 (10.0f));
    const auto unrefreshed = snapshot();      // deliberately no curve.refresh()
    check (! identical (moved, unrefreshed),
           "curveCache: a repaint with no preceding refresh draws the values it hashed");

    // The bounds are the OTHER half of the cache key — the path is in component
    // coordinates, so a resize must rebuild it even though no DSP input moved.
    // Deliberately NOT asserted here: every stimulus I could write for it is
    // satisfied by a cache that ignores bounds too (the drawn curve stays inside
    // the region both geometries share), and a check that cannot fail is worse
    // than none. The term is defensive and cheap; a resize test belongs with a
    // laid-out editor, which the headless suite does not have.
}

static void testTheSettingsPanelFollowsAProjectLoad()
{
    AnabasisAudioProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
    auto* ed = dynamic_cast<AnabasisAudioProcessorEditor*> (base.get());
    check (ed != nullptr, "settingsFollow: (premise) the editor was created");
    if (ed == nullptr)
        return;

    auto& tree = proc.internalState.state();
    tree.setProperty (iid::oversample, 3, nullptr);            // 8×
    tree.setProperty (iid::osPhase, 1, nullptr);               // linear
    ed->refreshInternalSettingsBoxes();
    // The round-26 half, now guarded rather than assumed: a combo whose index
    // is not its value follows the same load.
    if (auto* box = findComboByTitle (*ed, "Oversampling"))
        check (box->getSelectedItemIndex() == 3 && box->getText() == "8x",
               "settingsFollow: the combos still follow too");
    else
        check (false, "settingsFollow: (premise) the oversampling combo was found by title");

    // The micro-animation seed. `stepMicroAnims` eases `vpos` toward each
    // slider's real proportion and `drawRotarySlider` prefers `vpos` whenever
    // the control is not being dragged, so an unseeded editor opens with every
    // knob sweeping up from its minimum. The seed HAS to run after the APVTS
    // attachments: the setup helpers register the widget first, and at that
    // point the slider still carries JUCE's default 0..10 range and value 0 —
    // which is how the first attempt at this stored exactly the minimum it was
    // meant to stop showing. Asserted over EVERY registered slider rather than
    // a chosen one, with a guard that at least one sits off its minimum so the
    // sweep would actually be visible.
    int seeded = 0, offMinimum = 0;
    std::function<void (juce::Component&)> walk = [&] (juce::Component& root)
    {
        for (auto* c : root.getChildren())
        {
            if (auto* sl = dynamic_cast<juce::Slider*> (c))
            {
                const auto want = (double) sl->valueToProportionOfLength (sl->getValue());
                const auto got  = (double) sl->getProperties()["vpos"];
                if (std::abs (got - want) < 1.0e-6) ++seeded;
                if (want > 1.0e-3) ++offMinimum;
            }
            walk (*c);
        }
    };
    walk (*ed);
    check (offMinimum > 0,
           "settingsFollow: (premise) some knob's default sits off its minimum");
    check (seeded > 0 && seeded == countSliders (*ed),
           "settingsFollow: every knob's animated position starts where the value already is");

    // The streaming-target table and its tooltip checks stood here until the
    // 2026-08-05 removal (owner directive; the view's header carries the why).
}

// ---------------------------------------------------------------------------
// The reader's other half: the WINDOW CLAMP. `peek` masks the absolute index,
// so `head - kSize` aliases the slot the producer is filling at that instant —
// a frame asking for the full capacity reads a half-written entry as its
// oldest. Asserted through the pure bound rather than through a rendered
// frame, because the tear is only OBSERVABLE with a concurrent producer, which
// no deterministic test can stage: the guard has to be the bound itself.
static void testGrHistoryWindowNeverAsksForTheHeadSlot()
{
    using Ring = anabasis::GrHistoryBuffer;
    check (GrHistoryView::windowEntries (48000.0, 64) == Ring::kSize - 1,
           "grWindow: a 64-sample block saturates at kSize - 1, never kSize");
    check (GrHistoryView::windowEntries (192000.0, 32) == Ring::kSize - 1,
           "grWindow: …and so does every rate/block that would overflow the ring");
    check (GrHistoryView::windowEntries (48000.0, 512)
               == (int64_t) std::ceil (GrHistoryView::kWindowSeconds * 48000.0 / 512.0),
           "grWindow: below the clamp the window is the whole 20 s");
    check (GrHistoryView::windowEntries (0.0, 512) == GrHistoryView::windowEntries (48000.0, 512),
           "grWindow: an unprepared processor reads as 48 kHz, not as a divide by zero");

    // THE DECIMATION GEOMETRY — FIXED SCALE, RIGHT-ANCHORED (0.1.2 item 3).
    // This section previously pinned the 0.1.1 stretch-to-fill, which spread
    // however many buckets existed across the whole width — so a filling ring
    // rendered zoomed and re-spaced as it grew, the startup behaviour the
    // 0.1.2 directive removes. What is pinned now:
    //   • the pitch between adjacent buckets is a constant of (want, cols),
    //     identical while the ring fills and after it has wrapped;
    //   • the newest bucket sits on the right edge in every fill state;
    //   • a SETTLED window still spans the panel to within one truncated
    //     bucket of the left edge (the 0.1.1 blank-strip fix's property,
    //     carried over through `kFull` being derived from the same
    //     want/stride pair);
    //   • a filling ring occupies only the right portion at that same pitch —
    //     the left remainder is the unmeasured region `paintHistory` draws as
    //     ZERO data (level 0, GR 0), never as a stretched trace.
    {
        struct Case { double sr; int bs; int cols; const char* what; };
        const Case cases[] = {
            { 48000.0,  512, 904, "Simple well, 48 kHz / 512" },
            { 48000.0, 1024, 904, "Simple well, 48 kHz / 1024" },
            { 48000.0, 2048, 904, "…and a block big enough that entries are SCARCER than columns" },
            { 48000.0,  512, 604, "Advanced well, 48 kHz / 512" },
            { 192000.0,  32, 604, "192 kHz / 32 — the window saturates at the ring clamp" },
        };
        for (const auto& c : cases)
        {
            const auto want = GrHistoryView::windowEntries (c.sr, c.bs);
            const auto head = want * 4;                        // long settled, ring wrapped
            const auto b    = GrHistoryView::buckets (head, want, c.cols);
            const auto say  = [&c] (const char* what)
            { return juce::String ("grBuckets: ") + what + " — " + c.what; };
            const float pitch = ((float) c.cols - 1.0f) / (float) (b.kFull - 1);

            const auto m1 = say ("2..cols buckets");
            check (b.count >= 2 && b.count <= (int64_t) c.cols && b.kFull <= (int64_t) c.cols,
                   m1.toRawUTF8());
            // Settled: newest on the right edge, oldest within one pitch of
            // the left edge (exactly on it when the stride divides the window;
            // one truncated bucket short at the other boundary phase).
            const auto m2 = say ("a settled window spans the panel at the fixed pitch");
            check (std::abs (GrHistoryView::bucketX (b, b.kHead, 0.0f, (float) c.cols)
                             - (float) (c.cols - 1)) < 1.0e-3f
                   && GrHistoryView::bucketX (b, b.kFirst, 0.0f, (float) c.cols) > -1.0e-3f
                   && GrHistoryView::bucketX (b, b.kFirst, 0.0f, (float) c.cols) < pitch + 1.0e-3f,
                   m2.toRawUTF8());
            // …carrying one window of entries to within the truncation the
            // fixed pitch imposes (never MORE time than the window).
            const auto drawn = head - b.kFirst * b.stride;
            const auto m3 = say ("…carrying one window of entries, no more");
            check (drawn <= want && drawn >= want - 2 * b.stride, m3.toRawUTF8());
            // Every drawn bucket is non-empty: the oldest lies wholly inside
            // the window, and the newest holds entry `head - 1` by keying on
            // `head - 1` rather than `head` (which left it empty whenever the
            // head landed on a stride boundary).
            const auto m4 = say ("the oldest drawn bucket is inside the window");
            check (b.kFirst * b.stride >= head - want, m4.toRawUTF8());
            const auto m5 = say ("…and the newest holds the newest entry");
            check (b.kHead * b.stride <= head - 1 && (b.kHead + 1) * b.stride > head - 1,
                   m5.toRawUTF8());

            // FIXED SCALE across fill states — the item-3 property itself. A
            // quarter-full ring derives the same stride, the same kFull and
            // therefore the same pitch, keeps its newest bucket on the right
            // edge, and does NOT reach back to the left edge: the old stretch
            // put kFirst at x = 0 in exactly this state, which is the
            // behaviour this assertion exists to keep out.
            const auto q = GrHistoryView::buckets (juce::jmax ((int64_t) 2, want / 4),
                                                   want, c.cols);
            const auto m6 = say ("a quarter-full ring draws at the settled pitch, right-anchored");
            check (q.stride == b.stride && q.kFull == b.kFull
                   && std::abs (GrHistoryView::bucketX (q, q.kHead, 0.0f, (float) c.cols)
                                - (float) (c.cols - 1)) < 1.0e-3f
                   && std::abs ((GrHistoryView::bucketX (q, q.kHead, 0.0f, (float) c.cols)
                                 - GrHistoryView::bucketX (q, q.kHead - 1, 0.0f, (float) c.cols))
                                - pitch) < 1.0e-3f,
                   m6.toRawUTF8());
            const auto m7 = say ("…and leaves the unmeasured left region empty, not stretched");
            check (GrHistoryView::bucketX (q, q.kFirst, 0.0f, (float) c.cols)
                       > 0.5f * (float) c.cols,
                   m7.toRawUTF8());
        }

        // A ring with a handful of entries is a short stub at the RIGHT edge
        // at the same fixed pitch — the zero region covers the rest of the
        // panel. (Under the stretch this drew from the left edge across the
        // full width.)
        const auto want  = GrHistoryView::windowEntries (48000.0, 512);
        const auto small = GrHistoryView::buckets (12, want, 904);
        check (small.kFirst == 0 && small.count == 11 / small.stride + 1,
               "grBuckets: a barely-filled ring starts at bucket 0 and draws only what it has");
        check (std::abs (GrHistoryView::bucketX (small, small.kHead, 0.0f, 904.0f) - 903.0f) < 1.0e-3f
                   && GrHistoryView::bucketX (small, small.kFirst, 0.0f, 904.0f) > 890.0f,
               "grBuckets: …as a stub on the right edge, at the fixed pitch");

        // The DEGENERATE case: ONE bucket, reachable for the first few blocks
        // after every reset. Right-anchoring puts it on the right edge (the
        // stretch draw put it at the LEFT edge and needed a both-edges special
        // case in the paint); the zero region to its left is what the paint
        // now draws unconditionally whenever the first bucket is off the left
        // edge, so no special case remains.
        const auto one = GrHistoryView::buckets (1, want, 904);
        check (one.count == 1 && one.kFirst == one.kHead,
               "grBuckets: a just-reset ring yields exactly one bucket");
        check (std::abs (GrHistoryView::bucketX (one, one.kHead, 0.0f, 904.0f) - 903.0f) < 1.0e-3f,
               "grBuckets: …anchored on the right edge, zero region to its left");

        // THE LEFT-EDGE RULE (0.1.3 item 4). The zero region is honest only
        // while the ring is still FILLING; once a full window is scrolling,
        // the sub-pitch strip left of the truncated oldest bucket holds
        // EXPIRED history, and drawing the zero line across it — then dropping
        // vertically into the trace at the same x — is the flashing accent bar
        // the owner reported. The predicate is pinned here rather than left
        // inline in `paintHistory`, which is what let the defect ship.
        check (GrHistoryView::drawsZeroRegion (1, want),
               "grZeroRegion: a just-reset ring draws the unmeasured region");
        check (GrHistoryView::drawsZeroRegion (want - 1, want),
               "grZeroRegion: …still, one entry short of a full window");
        check (GrHistoryView::drawsZeroRegion (want, want),
               "grZeroRegion: …and at the exact changeover, where the oldest bucket lands on the edge");
        check (! GrHistoryView::drawsZeroRegion (want + 1, want),
               "grZeroRegion: a scrolling window does NOT — that strip is expired history, not unmeasured");
        check (! GrHistoryView::drawsZeroRegion (want * 97 + 3, want),
               "grZeroRegion: …however long it has been scrolling, at any bucket-expiry phase");
    }
}

// ---------------------------------------------------------------------------
// §2.9 vs §2.7: the meters report the RENDER, not the listening path. With
// Delta or Loudness Comp engaged the buffer handed to the host is the monitor
// signal — metering it showed the difference signal's loudness (Delta) or the
// attenuated level (Comp), and permanently biased the session-cumulative
// integrated LUFS and dBTP hold. The published readings must be identical
// whether the monitor functions are on or off, while the audible output
// plainly differs (the guard that keeps this from passing vacuously).
static void testMetersReadTheRenderNotTheMonitor()
{
    auto run = [] (bool comp, bool delta)
    {
        AnabasisAudioProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        auto set = [&] (const char* id, float denorm)
        {
            auto* p = proc.apvts.getParameter (id);
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (denorm));
        };
        set (pid::limGain, 12.0f);                     // wet well above dry: comp acts
        if (comp)  set (pid::loudnessComp, 1.0f);
        if (delta) set (pid::deltaMonitor, 1.0f);

        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buf (2, 512);
        double tailSq = 0.0;
        for (int b = 0; b < 200; ++b)                  // ~2.1 s: measure + smoothers settle
        {
            for (int n = 0; n < 512; ++n)
            {
                const float v = 0.1f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                 * 997.0f * (float) (b * 512 + n) / 48000.0f);
                buf.setSample (0, n, v);
                buf.setSample (1, n, v);
            }
            proc.processBlock (buf, midi);
            if (b >= 150)
                for (int n = 0; n < 512; ++n)
                    tailSq += (double) buf.getSample (0, n) * buf.getSample (0, n);
        }
        struct R { float lufsS, lufsM, tpMax, plr; double tailRms; };
        return R { proc.meterLufsS(), proc.meterLufsM(), proc.meterDbTpMax(),
                   proc.meterPlr(), std::sqrt (tailSq / (50.0 * 512.0)) };
    };

    const auto plain = run (false, false);
    const auto comp  = run (true,  false);
    const auto delta = run (false, true);

    // The render is bit-identical across the three runs, so the published
    // readings are too — exact, not approximate: same samples, same meters.
    check (juce::exactlyEqual (plain.lufsS, comp.lufsS)
             && juce::exactlyEqual (plain.lufsM, comp.lufsM)
             && juce::exactlyEqual (plain.tpMax, comp.tpMax)
             && juce::exactlyEqual (plain.plr,  comp.plr),
           "meters/monitor: loudness comp does not move a single published reading");
    check (juce::exactlyEqual (plain.lufsS, delta.lufsS)
             && juce::exactlyEqual (plain.tpMax, delta.tpMax),
           "meters/monitor: delta does not move the published readings either");

    // Not vacuous: the LISTENING path really was altered in both runs.
    check (comp.tailRms < plain.tailRms * 0.7,
           "meters/monitor: comp audibly attenuates the monitor (the runs are not identical)");
    // Delta's tail is the dry-minus-wet residue (~0.75× here, direction not
    // guaranteed), so the guard is a relative difference, not an ordering.
    check (std::abs (delta.tailRms - plain.tailRms) > 0.1 * plain.tailRms,
           "meters/monitor: delta audibly replaces the monitor with the difference signal");
}

// ---------------------------------------------------------------------------
// MODE_AND_ADAPTATION_POLICY invariant 2's named guard: switching Simple ⇄
// Advanced changes NOTHING about the rendered sound — not approximately,
// sample-identically. Two processors, identical input and settings; one
// toggles advancedMode repeatedly mid-stream (at macro positions and after a
// manual Advanced edit), the other never does. Byte-compare the outputs.
// Structurally the switch cannot reach the DSP (advancedMode is not in the
// cache order), and this test is what keeps that structural fact true.
static void testModeSwitchIsSoundNeutral()
{
    auto renderWithToggles = [] (bool toggle) -> std::vector<float>
    {
        AnabasisAudioProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        // Same non-default context on both: a macro position and a manual
        // Advanced edit (which detaches limGain from the macro).
        proc.apvts.getParameter (pid::loudness)->setValueNotifyingHost (0.4f);
        proc.getMacroEngine().flushPendingMapping();
        proc.apvts.getParameter (pid::limGain)->setValueNotifyingHost (0.6f);

        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buf (2, 512);
        std::vector<float> out;
        for (int b = 0; b < 60; ++b)
        {
            if (toggle && b % 7 == 3)
                proc.apvts.getParameter (pid::advancedMode)
                    ->setValueNotifyingHost (b % 14 == 3 ? 1.0f : 0.0f);
            for (int n = 0; n < 512; ++n)
            {
                const float v = 0.4f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                 * 330.0f * (float) (b * 512 + n) / 48000.0f);
                buf.setSample (0, n, v);
                buf.setSample (1, n, v);
            }
            proc.processBlock (buf, midi);
            for (int n = 0; n < 512; ++n)
                out.push_back (buf.getSample (0, n));
        }
        return out;
    };

    const auto still = renderWithToggles (false);
    const auto moved = renderWithToggles (true);
    bool identical = still.size() == moved.size();
    for (size_t n = 0; identical && n < still.size(); ++n)
        if (! juce::exactlyEqual (still[n], moved[n]))
            identical = false;
    check (identical, "modeSwitch: Simple/Advanced toggling is sample-identical to not toggling");
}

// ---------------------------------------------------------------------------
// §5.4 Learn end to end: analyse a passage → commit → the reference targets
// move; the session then carries an ADAPTIVE child that restores them; a
// session WITHOUT the child restores "never learned" (§4.4's discriminator).
static void testLearnCommitAndAdaptiveRoundTrip()
{
    AnabasisAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, 512);

    const float refOnset0 = proc.adaptiveReadout().publishedRefOnset();
    check (! proc.adaptiveReadout().hasLearned(), "learn: factory state has never learned");

    proc.startLearn();
    for (int b = 0; b < 500; ++b)                    // ~5 s of transient-dense material
    {
        for (int n = 0; n < 512; ++n)
        {
            const int t = b * 512 + n;
            float v = 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi
                                       * 220.0f * (float) t / 48000.0f);
            if ((t % 4800) < 96) v += 0.6f;          // 10 clicks/s: far off the default ref
            buf.setSample (0, n, v);
            buf.setSample (1, n, v);
        }
        proc.processBlock (buf, midi);
    }
    proc.stopLearn();
    for (int b = 0; b < 2; ++b) proc.processBlock (buf, midi);   // commit consumed at block top

    check (proc.adaptiveReadout().hasLearned(), "learn: commit latches the learned state");
    const float refOnsetLearned = proc.adaptiveReadout().publishedRefOnset();
    check (refOnsetLearned > refOnset0 + 2.0f,
           "learn: the onset reference moved to the analysed passage's density");

    // Round trip: the ADAPTIVE child restores the targets...
    juce::MemoryBlock state;
    proc.getStateInformation (state);
    AnabasisAudioProcessor restored;
    restored.prepareToPlay (48000.0, 512);
    restored.setStateInformation (state.getData(), (int) state.getSize());
    for (int b = 0; b < 2; ++b) restored.processBlock (buf, midi);   // mirror consumed
    check (std::abs (restored.adaptiveReadout().publishedRefOnset() - refOnsetLearned) < 1.0e-4f,
           "learn: the ADAPTIVE child restores the learned targets");

    // ...byte-identity still holds with the child present...
    juce::MemoryBlock again;
    restored.getStateInformation (again);
    check (state == again, "learn: save → load → save stays byte-identical with ADAPTIVE present");

    // ...and a session WITHOUT the child restores never-learned defaults.
    juce::MemoryBlock blank;
    AnabasisAudioProcessor().getStateInformation (blank);
    restored.setStateInformation (blank.getData(), (int) blank.getSize());
    for (int b = 0; b < 2; ++b) restored.processBlock (buf, midi);
    check (! restored.adaptiveReadout().hasLearned()
             && std::abs (restored.adaptiveReadout().publishedRefOnset() - refOnset0) < 1.0e-4f,
           "learn: absent ADAPTIVE means never learned — defaults restored");

    // Load → save with NO audio in between. The engine only adopts a staged
    // restore at a block top, so a host that duplicates a track, copies plugin
    // state, or saves a freshly opened project without transport would have
    // serialized the engine's one-session-stale answer: the ADAPTIVE child
    // omitted entirely (Learn silently lost), and in the mirror case an old
    // learned child resurrected over an un-learned session.
    {
        AnabasisAudioProcessor noAudio;
        noAudio.prepareToPlay (48000.0, 512);
        noAudio.setStateInformation (state.getData(), (int) state.getSize());
        juce::MemoryBlock resaved;
        noAudio.getStateInformation (resaved);          // deliberately no processBlock

        auto xml = juce::AudioProcessor::getXmlFromBinary (resaved.getData(), (int) resaved.getSize());
        check (xml != nullptr, "learn/noAudio: the immediate re-save parses");
        const auto reloaded = juce::ValueTree::fromXml (*xml);
        const auto child    = reloaded.getChildWithName ("ADAPTIVE");
        check (child.isValid(),
               "learn/noAudio: loading and re-saving without audio keeps the ADAPTIVE child");
        check (std::abs ((float) (double) child.getProperty ("refOnsetRate") - refOnsetLearned)
                 < 1.0e-4f,
               "learn/noAudio: ...with the loaded session's references, not the engine's stale ones");

        // Mirror: a LEARNED engine loaded with an un-learned session and
        // re-saved before the next block must not resurrect the old child.
        AnabasisAudioProcessor wasLearned;
        wasLearned.prepareToPlay (48000.0, 512);
        wasLearned.setStateInformation (state.getData(), (int) state.getSize());
        for (int b = 0; b < 2; ++b) wasLearned.processBlock (buf, midi);   // adopt it
        check (wasLearned.adaptiveReadout().hasLearned(), "learn/noAudio: the mirror run did learn");
        juce::MemoryBlock blank2;
        AnabasisAudioProcessor().getStateInformation (blank2);
        wasLearned.setStateInformation (blank2.getData(), (int) blank2.getSize());
        juce::MemoryBlock afterBlank;
        wasLearned.getStateInformation (afterBlank);    // again no processBlock
        auto xml2 = juce::AudioProcessor::getXmlFromBinary (afterBlank.getData(),
                                                            (int) afterBlank.getSize());
        check (xml2 != nullptr && ! juce::ValueTree::fromXml (*xml2)
                                      .getChildWithName ("ADAPTIVE").isValid(),
               "learn/noAudio: an un-learned session re-saved without audio stays un-learned");
    }

    // A PRESENT child with the values MISSING is the §4.4 read rule's other
    // half: a missing field takes its default, and the default here is the
    // factory neutral reference — not var()'s 0.0, which would leave the
    // trims chasing a reference no programme material can produce.
    {
        auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
        check (xml != nullptr, "learn: the saved session parses back to XML (fixture precondition)");
        auto root = juce::ValueTree::fromXml (*xml);
        auto adaptive = root.getChildWithName ("ADAPTIVE");
        check (adaptive.isValid(), "learn: the learned session really does carry ADAPTIVE");
        adaptive.removeProperty ("refOnsetRate", nullptr);
        adaptive.removeProperty ("refTiltDb", nullptr);
        juce::MemoryBlock stripped;
        if (const auto out = root.createXml())
            juce::AudioProcessor::copyXmlToBinary (*out, stripped);

        AnabasisAudioProcessor partial;
        partial.prepareToPlay (48000.0, 512);
        partial.setStateInformation (stripped.getData(), (int) stripped.getSize());
        for (int b = 0; b < 2; ++b) partial.processBlock (buf, midi);
        check (partial.adaptiveReadout().hasLearned(),
               "learn: a present ADAPTIVE child still means learned, values or not");
        check (std::abs (partial.adaptiveReadout().publishedRefOnset()
                           - anabasis::AdaptiveEngine::kDefaultRefOnset) < 1.0e-4f
                 && std::abs (partial.adaptiveReadout().publishedRefTilt()
                           - anabasis::AdaptiveEngine::kDefaultRefTilt) < 1.0e-4f,
               "learn: missing ADAPTIVE fields fall back to the factory references, not zero");
    }
}

// ---------------------------------------------------------------------------
// ADR-0015: the Ceiling ADVERTISES THE UNIT IT ACTUALLY ENFORCES. `ceiling` is
// a dBTP limit only while true-peak mode is engaged (DSP_POLICY invariant 3,
// ADR-0006 item 3); with it off — the shipped default since ADR-0015 — the
// clamp decides on the sample peak, so the old unconditional " dBTP" suffix
// promised an inter-sample guarantee the default configuration does not make.
//
// Checked through the HOST-FACING path (`getCurrentValueAsText`, i.e. the same
// `getText` a generic editor and the value box call), in both modes, plus the
// round-trip that makes the change safe: `dbFrom` parses the leading float, so
// `getValueForText` is indifferent to which spelling it is handed. That last
// half is what pins the suffix as display-only — a reader who wonders whether
// a live-changing unit can disturb automation or state has the answer here.
static void testTheCeilingAdvertisesTheUnitItEnforces()
{
    AnabasisAudioProcessor proc;
    auto* ceil = proc.apvts.getParameter (pid::ceiling);
    auto* tp   = proc.apvts.getParameter (pid::truePeakMode);
    check (ceil != nullptr && tp != nullptr,
           "ceilingUnit: (premise) the ceiling and true-peak parameters exist");
    if (ceil == nullptr || tp == nullptr)
        return;

    check (tp->getValue() < 0.5f,
           "ceilingUnit: (premise) true-peak mode ships OFF (ADR-0015)");

    const auto offText = ceil->getCurrentValueAsText();
    check (offText.endsWith (" dB"),
           "ceilingUnit: with true-peak mode off the ceiling reads in plain dB");
    check (! offText.contains ("dBTP"),
           "ceilingUnit: …and does not claim dBTP the default cannot hold");

    tp->setValueNotifyingHost (1.0f);
    const auto onText = ceil->getCurrentValueAsText();
    check (onText.endsWith (" dBTP"),
           "ceilingUnit: engaging true-peak mode makes the ceiling read in dBTP");

    // Both spellings name the same number, and both parse back to it.
    check (std::abs (ceil->getValueForText (offText) - ceil->getValueForText (onText)) < 1.0e-6f,
           "ceilingUnit: the suffix is display-only — both spellings parse identically");
    check (std::abs (ceil->getValueForText (onText) - ceil->getValue()) < 1.0e-4f,
           "ceilingUnit: …and round-trip to the value they were printed from");

    tp->setValueNotifyingHost (0.0f);
    check (ceil->getCurrentValueAsText().endsWith (" dB")
             && ! ceil->getCurrentValueAsText().contains ("dBTP"),
           "ceilingUnit: turning true-peak mode back off restores the weaker claim");

    // ...AND THE READOUT THE USER ACTUALLY LOOKS AT, which the parameter half
    // above does not reach. A JUCE Slider caches its value-box label and
    // recomputes it only in `updateText()` — on a value change, a
    // `setTextBoxStyle`, a relayout or a LookAndFeel change, never on a
    // repaint. Flipping TP does none of those, so the box kept the previous
    // suffix while `getText` returned the right one: the host was told the
    // truth and the plugin's own panel was not. Checked on the LABEL rather
    // than through `getTextFromValue`, because the cached string is the defect
    // — a live re-computation would pass either way.
    {
        std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
        auto* ed = dynamic_cast<AnabasisAudioProcessorEditor*> (base.get());
        check (ed != nullptr, "ceilingUnit: (premise) the editor was created");
        if (ed == nullptr)
            return;

        auto* knob = findSliderByTitle (*ed, ceil->getName (24));
        check (knob != nullptr, "ceilingUnit: (premise) a Ceiling knob was found");
        if (knob == nullptr)
            return;
        auto* box = findChildLabel (*knob);
        check (box != nullptr, "ceilingUnit: (premise) the knob has a value box");
        if (box == nullptr)
            return;

        check (box->getText().endsWith (" dB") && ! box->getText().contains ("dBTP"),
               "ceilingUnit: the value box opens in the shipped default's unit");

        tp->setValueNotifyingHost (1.0f);
        ed->refreshCeilingUnit();          // the 24 Hz tick's edge, driven directly
        check (box->getText().endsWith (" dBTP"),
               "ceilingUnit: engaging true-peak mode refreshes the value box, not just getText");

        tp->setValueNotifyingHost (0.0f);
        ed->refreshCeilingUnit();
        check (box->getText().endsWith (" dB") && ! box->getText().contains ("dBTP"),
               "ceilingUnit: …and disengaging it refreshes back");
    }

    // AN EDITOR OPENED ON A TP-ON SESSION, then switched off before the first
    // refresh. This is the case a hard-coded `shownTpMode = false` seed got
    // wrong: the attachment renders " dBTP" at construction, the cache claims
    // off, and the edge gate then sees `tp == shownTpMode == false` and skips
    // the refresh — leaving " dBTP" over a sample-peak ceiling. Reachable in
    // ~42 ms of real time (a host write, a state load, the other TP toggle),
    // and permanent until something else forces a recompute.
    {
        tp->setValueNotifyingHost (1.0f);
        std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
        auto* ed = dynamic_cast<AnabasisAudioProcessorEditor*> (base.get());
        check (ed != nullptr, "ceilingUnit: (premise) the TP-on editor was created");
        if (ed == nullptr)
            return;

        auto* knob = findSliderByTitle (*ed, ceil->getName (24));
        auto* box  = knob != nullptr ? findChildLabel (*knob) : nullptr;
        check (box != nullptr, "ceilingUnit: (premise) the TP-on editor's value box was found");
        if (box == nullptr)
            return;
        check (box->getText().endsWith (" dBTP"),
               "ceilingUnit: (premise) an editor opened on TP-on shows dBTP straight away");

        // The flip the seed has to have recorded, before any tick runs.
        tp->setValueNotifyingHost (0.0f);
        ed->refreshCeilingUnit();
        check (box->getText().endsWith (" dB") && ! box->getText().contains ("dBTP"),
               "ceilingUnit: TP off right after opening on TP-on still refreshes the box");
    }
}

// ---------------------------------------------------------------------------
// kCacheOrder and CachedParams::toEngine are coupled POSITIONALLY: inserting a
// row in one without the matching line in the other silently shifts every
// later field, and the static_assert only catches a length change. Distinct
// values per parameter, checked field by field, catch a shift of any size.
// TWO DECIMAL PLACES ON THE CEILING, checked where it has to be true: the
// VALUE, not the label. The display was already the easy half — the hard half
// is that a host writing a raw normalised number, a state restore, and a
// preset override all reach `AudioParameterFloat::setValue`, which does NOT
// snap, so an `interval` alone would leave the automation lane writing
// -0.123456 into a knob that reads -0.12. The quantisation therefore lives in
// the range's `convertFrom0to1` (see `twoDecimalRange`), and these assertions
// exercise each entry point separately rather than trusting one of them to
// stand for the rest.
static void testCeilingIsQuantisedToTwoDecimals()
{
    AnabasisAudioProcessor proc;
    auto* ceil = proc.apvts.getParameter (pid::ceiling);
    check (ceil != nullptr, "ceiling2dp: (premise) the ceiling parameter exists");
    if (ceil == nullptr)
        return;

    // "Two decimal places" in binary floating point means the nearest float to
    // a two-decimal decimal, so the predicate is on the SCALED value: v·100
    // must be a whole number to well inside a float ULP at this magnitude.
    auto isTwoDecimal = [] (float v)
    {
        const double scaled = (double) v * 100.0;
        return std::abs (scaled - std::round (scaled)) < 1.0e-3;
    };

    // 1. HOST AUTOMATION. A sweep of raw normalised values, deliberately on
    //    irrational-ish steps so the sweep cannot accidentally land on the grid
    //    by construction — 997 is coprime with everything the range divides by.
    bool everyStepOnGrid = true;
    float worst = 0.0f;
    for (int i = 0; i <= 997; ++i)
    {
        const float norm = (float) i / 997.0f;
        ceil->setValueNotifyingHost (norm);
        const float v = proc.apvts.getRawParameterValue (pid::ceiling)->load();
        if (! isTwoDecimal (v))
        {
            everyStepOnGrid = false;
            worst = v;
        }
    }
    const auto sweepMsg = juce::String ("ceiling2dp: every raw normalised host write lands on a two-decimal value")
                        + (everyStepOnGrid ? juce::String() : " (worst: " + juce::String (worst, 6) + ")");
    check (everyStepOnGrid, sweepMsg.toRawUTF8());

    // 2. THE VALUE THE DSP READS. `AudioProcessorValueTreeState` publishes
    //    `convertFrom0to1 (normalised)` into the atomic that `CachedParams`
    //    resolves and `EngineParameters::ceilingDbTp` carries, so this is the
    //    same number `CeilingClamp` compares against — not a rounded display of
    //    an unrounded one.
    ceil->setValueNotifyingHost (ceil->getNormalisableRange().convertTo0to1 (-0.123456f));
    const float dspValue = proc.apvts.getRawParameterValue (pid::ceiling)->load();
    check (isTwoDecimal (dspValue) && std::abs (dspValue - (-0.12f)) < 1.0e-4f,
           "ceiling2dp: -0.123456 reaches the DSP as -0.12");

    // 3. TEXT ENTRY, both rounding directions and the trailing zero the owner
    //    asked for. `getText` formats `convertFrom0to1 (v)`, so a text that
    //    round-trips proves the label and the value agree by construction.
    struct TextCase { const char* typed; const char* shown; float value; };
    const TextCase cases[] = {
        { "-0.1",      "-0.10", -0.10f },
        { "-0.123",    "-0.12", -0.12f },
        { "-0.129",    "-0.13", -0.13f },
        { "-0.123456", "-0.12", -0.12f },
        { "-3",        "-3.00", -3.00f },
    };
    for (const auto& c : cases)
    {
        ceil->setValueNotifyingHost (ceil->getValueForText (c.typed));
        const float v = proc.apvts.getRawParameterValue (pid::ceiling)->load();
        const auto storeMsg = juce::String ("ceiling2dp: typing ") + c.typed + " stores " + juce::String (c.value, 2);
        check (std::abs (v - c.value) < 1.0e-4f, storeMsg.toRawUTF8());
        const auto showMsg = juce::String ("ceiling2dp: ...and displays ") + c.shown;
        check (ceil->getCurrentValueAsText().startsWith (c.shown), showMsg.toRawUTF8());
    }

    // 4. STATE RESTORE. The stored value must come back identical rather than
    //    drifting by a rounding step each save/load — the round trip
    //    normalise→denormalise has to be a fixed point on the grid.
    ceil->setValueNotifyingHost (ceil->getValueForText ("-0.13"));
    const float before = proc.apvts.getRawParameterValue (pid::ceiling)->load();
    juce::MemoryBlock blob;
    proc.getStateInformation (blob);
    ceil->setValueNotifyingHost (ceil->getValueForText ("-6.00"));
    proc.setStateInformation (blob.getData(), (int) blob.getSize());
    const float after = proc.apvts.getRawParameterValue (pid::ceiling)->load();
    check (juce::exactlyEqual (before, after) && isTwoDecimal (after),
           "ceiling2dp: a save/load round trip returns the same two-decimal value");

    // 5. THE DEFAULT IS ON THE GRID. If it were not, every fresh instance would
    //    start off-grid and the first knob touch would jump — and
    //    `testRegistrySnapshot` pins the default, so this is also the assertion
    //    that says the fixture and the range still agree.
    check (isTwoDecimal (ceil->getNormalisableRange().convertFrom0to1 (ceil->getDefaultValue())),
           "ceiling2dp: the shipped default is itself a two-decimal value");
}

static void testCachedParamsMapping()
{
    AnabasisAudioProcessor proc;
    auto set = [&] (const char* id, float denorm)
    {
        auto* p = proc.apvts.getParameter (id);
        p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (denorm));
    };
    set (pid::inputGain, 7.0f);
    set (pid::scHpfFreq, 120.0f);
    set (pid::compRatio, 3.0f);
    set (pid::compThreshold, -18.0f);
    set (pid::compKnee, 9.0f);
    set (pid::clipDrive, 5.0f);
    set (pid::colourBalance, 0.5f);
    set (pid::dynTilt, 1.25f);
    set (pid::limGain, 11.0f);
    set (pid::lookahead, 4.0f);
    set (pid::limRelease, 250.0f);
    set (pid::eqTilt, -2.0f);
    set (pid::eqLowShelfGain, 3.0f);
    set (pid::eqBell2Q, 2.0f);
    set (pid::ceiling, -3.0f);
    set (pid::compMix, 40.0f);          // percent in, 0..1 out
    set (pid::compStereoLink, 70.0f);   // ADR-0019 — distinct from the limiter's 80
    set (pid::stereoLink, 80.0f);
    set (pid::colourDepth, 60.0f);

    check (proc.cachedForTest().allResolved(),
           "cache: every cached id resolves to a live parameter (a null slot would feed 0)");

    anabasis::EngineParameters e;
    proc.cachedForTest().toEngine (e);

    auto near = [] (float a, float b) { return std::abs (a - b) < 1.0e-3f; };
    check (near (e.inputGainDb, 7.0f)        && near (e.scHpfFreqHz, 120.0f),   "cache: input/detector fields");
    check (near (e.compRatio, 3.0f)          && near (e.compThresholdDb, -18.0f)
            && near (e.compKneeDb, 9.0f)     && near (e.compMix, 0.40f)
            && near (e.compStereoLink, 0.70f),                                   "cache: compressor fields");
    check (near (e.clipDriveDb, 5.0f)        && near (e.colourBalance, 0.5f)
            && near (e.dynTiltDb, 1.25f)     && near (e.colourDepth, 0.60f),    "cache: clip/colour fields");
    check (near (e.limGainDb, 11.0f)         && near (e.lookaheadMs, 4.0f)
            && near (e.limReleaseMs, 250.0f) && near (e.stereoLink, 0.80f),     "cache: limiter fields");
    check (near (e.eqTiltDb, -2.0f)          && near (e.eqLowShelfGainDb, 3.0f)
            && near (e.eqBell2Q, 2.0f),                                          "cache: eq fields");
    check (near (e.ceilingDbTp, -3.0f),                                          "cache: shared/output fields");
}

// ============================================================================
//  Both channels carry audio through the REAL wrapper (processBlock), at
//  defaults and with the macro push engaged. Written to reproduce a field
//  report of a silent LEFT channel; the DSP suite proves the ENGINE stereo
//  path, but nothing before this drove the wrapper's processBlock and measured
//  BOTH output channels.
// ============================================================================
// KI-009's field scenario, run as the user runs it — and the reason this is a
// WRAPPER test rather than another stage-level property: the report is
// "load a preset, play, the left channel goes silent, Clip Mix = 0 brings it
// back", so the guard has to be the whole plugin under a real preset with real
// programme, not an invariant about one stage.
//
// It also PINS THE PROOF that redirected the investigation. At the factory
// Default the clipper is exact-skipped (`clipDrive == 0`), the colour stage
// contributes nothing (`colourDepth == 0`) and the dynamic tame is idle
// (`activityEnv` is bit-zero while nothing clips) — so ClipSat's wet value IS
// its input, and the mix blend, at ANY mix, lands on the same float:
//   mix == 1 → `chans = wet` (the same number);
//   mix == 0 → nothing written;
//   otherwise → `chans + (wet − chans)·mix`, and `wet − chans` is exactly 0.
// **At the Default preset Clip Mix therefore cannot change one sample**, which
// makes it impossible for Clip Mix to be the cause of anything there. The
// assertion below is that statement made mechanical: sweep the control across
// its range and the rendered output must be BIT-IDENTICAL every time.
//
// The mutant it kills is any future edit that gives the stage a mix-dependent
// contribution at the null settings — which is exactly the class of change that
// would make the field correlation real, and the class this test exists to stop
// re-entering the tree unnoticed.
static void testClipMixCannotChangeTheDefaultPresetsSound()
{
    auto render = [] (int factoryIndex, float clipMixPercent, std::vector<float>& outL,
                      std::vector<float>& outR)
    {
        AnabasisAudioProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        proc.applyFactoryPreset (factoryIndex);
        auto* q = proc.apvts.getParameter (pid::clipMix);
        q->setValueNotifyingHost (q->getNormalisableRange().convertTo0to1 (clipMixPercent));

        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        const int blocks = 90;
        outL.clear(); outR.clear();
        for (int b = 0; b < blocks; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                // Programme, not a test tone: bass, body, air and a periodic
                // transient, correlated across the pair the way a master is,
                // and hot enough that every stage a preset engages is working.
                const int   tt = b * 512 + n;
                const float ph = 2.0f * juce::MathConstants<float>::pi * (float) tt / 48000.0f;
                const float base = 0.50f * std::sin (ph * 55.0f)
                                 + 0.22f * std::sin (ph * 440.0f)
                                 + 0.12f * std::sin (ph * 5200.0f)
                                 + ((tt % 12000) < 40 ? 0.30f : 0.0f);
                buf.setSample (0, n, juce::jlimit (-0.99f, 0.99f, base));
                buf.setSample (1, n, juce::jlimit (-0.99f, 0.99f, base * 0.97f));
            }
            proc.processBlock (buf, midi);
            if (b >= blocks / 2)                     // the settled half
                for (int n = 0; n < 512; ++n)
                {
                    outL.push_back (buf.getSample (0, n));
                    outR.push_back (buf.getSample (1, n));
                }
        }
    };

    auto rms = [] (const std::vector<float>& v)
    {
        double s = 0.0;
        for (auto x : v) s += (double) x * x;
        return std::sqrt (s / (double) juce::jmax ((size_t) 1, v.size()));
    };

    // ---- (1) the Default preset: Clip Mix is provably inert ----------------
    std::vector<float> refL, refR, curL, curR;
    render (0, 100.0f, refL, refR);
    check (rms (refL) > 0.02 && rms (refR) > 0.02,
           "clipMixField: (premise) the Default preset passes programme on BOTH channels");

    for (const float mixPct : { 0.0f, 1.0f, 25.0f, 50.0f, 99.0f, 100.0f })
    {
        render (0, mixPct, curL, curR);
        size_t differing = 0;
        for (size_t n = 0; n < refL.size(); ++n)
            if (! juce::exactlyEqual (curL[n], refL[n]) || ! juce::exactlyEqual (curR[n], refR[n]))
                ++differing;
        juce::String msg;
        msg << "clipMixField: at the Default preset Clip Mix " << mixPct
            << " % renders BIT-IDENTICALLY to 100 % — the control cannot be causal here ("
            << (int) differing << " samples differ)";
        check (differing == 0, msg.toRawUTF8());
        msg.clear();
        msg << "clipMixField: …and both channels are alive at Clip Mix " << mixPct << " % (L="
            << (float) rms (curL) << " R=" << (float) rms (curR) << ")";
        check (rms (curL) > 0.02 && rms (curR) > 0.02, msg.toRawUTF8());
    }

    // ---- (2) EVERY factory preset, at both Clip Mix endpoints --------------
    // The field report says "all presets". Where a preset DOES engage the
    // clipper the two endpoints legitimately differ in sound — what may never
    // differ is whether a channel is there at all, and the two channels may
    // never diverge (the input pair is 0.26 dB apart by construction).
    int factoryCount = 0;
    PresetManager::factoryPresets (factoryCount);
    check (factoryCount > 1, "clipMixField: (premise) the factory bank has presets to sweep");
    for (int i = 0; i < factoryCount; ++i)
        for (const float mixPct : { 0.0f, 100.0f })
        {
            render (i, mixPct, curL, curR);
            const double l = rms (curL), r = rms (curR);
            juce::String msg;
            msg << "clipMixField: factory preset " << i << " at Clip Mix " << mixPct
                << " % keeps BOTH channels alive and matched (L=" << (float) l
                << " R=" << (float) r << ")";
            check (l > 0.01 && r > 0.01 && l < r * 2.0 && r < l * 2.0, msg.toRawUTF8());
        }
}

static void testBothChannelsCarryAudioThroughTheWrapper()
{
    // `clipMixPercent < 0` leaves Clip Mix at its default; a value in [0, 100]
    // sets it. The parameter is threaded through rather than set by the caller
    // because it must be written BEFORE the processing loop and AFTER the macro
    // engagement below, and because the premise assertion at the end of the
    // engagement block has to see both.
    auto run = [] (float loudness, const char* tag, float clipMixPercent = -1.0f)
    {
        AnabasisAudioProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        if (loudness > 0.0f)
        {
            auto* par = proc.apvts.getParameter (pid::loudness);
            par->setValueNotifyingHost (par->getNormalisableRange().convertTo0to1 (loudness));
            // …AND the parameters that knob drives, set directly.
            //
            // WITHOUT THIS THE CASE IS VACUOUS, and it was for its whole life
            // (found 2026-08-09 by instrumenting the chain and reading the
            // engaged values back: at "loudness 85" the clipper reported
            // `clipDrive == 0`). `MacroEngine` maps on a 30 ms `juce::Timer`,
            // and a headless console app runs no message loop, so the tick
            // never fires and the macro-managed parameters keep their
            // defaults. The knob moved; nothing downstream of it did — so
            // every "loudness N" case in this battery was re-running the
            // DEFAULTS case, with the clipper exact-skipped and the colour
            // stage contributing nothing.
            //
            // That matters well beyond a tidier test: the field report this
            // battery exists for (KI-009) is observed with the chain PUSHED,
            // and the pushed chain is exactly what was never being exercised.
            // The values below are representative of the §5.5 curves at this
            // knob position rather than derived from them — deriving them
            // would couple this test to the curve tables, and what it needs is
            // simply that the stages are WORKING.
            auto engage = [&proc] (const char* id, float denorm)
            {
                auto* q = proc.apvts.getParameter (id);
                q->setValueNotifyingHost (q->getNormalisableRange().convertTo0to1 (denorm));
            };
            engage (pid::clipDrive,   loudness * 0.22f);
            engage (pid::colourDepth, loudness);
            engage (pid::dynTilt,     loudness > 60.0f ? 2.0f : 0.8f);
            engage (pid::limGain,     loudness * 0.18f);

            // THE PREMISE, ASSERTED RATHER THAN ASSUMED. Round 6 found every
            // "loudness N" case in this battery had been vacuous for its whole
            // life — the knob moved and `clipDrive` stayed 0, so the clipper was
            // exact-skipped and the cases were re-running "defaults" under a
            // different name. The repair was to engage the parameters directly;
            // this check is what makes the repair self-guarding. Without it the
            // same silence returns the moment an id, a range or a curve moves,
            // and it returns GREEN.
            juce::String premise;
            premise << "stereoWrapper (" << tag << "): (premise) the clipper is actually engaged "
                    << "(clipDrive=" << proc.apvts.getRawParameterValue (pid::clipDrive)->load()
                    << " dB) — a vacuous case is what round 6 found here";
            check (proc.apvts.getRawParameterValue (pid::clipDrive)->load() > 0.0f,
                   premise.toRawUTF8());
        }

        if (clipMixPercent >= 0.0f)
        {
            auto* q = proc.apvts.getParameter (pid::clipMix);
            q->setValueNotifyingHost (q->getNormalisableRange().convertTo0to1 (clipMixPercent));
        }

        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        double sumSq[2] = { 0.0, 0.0 };
        const int blocks = 60;                      // ~640 ms: latency + smoothing well cleared
        for (int b = 0; b < blocks; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                const int t = b * 512 + n;
                // DIFFERENT frequencies per channel, so a channel swap or a
                // sum-into-one-channel defect cannot masquerade as health.
                buf.setSample (0, n, 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                       * 220.0f * (float) t / 48000.0f));
                buf.setSample (1, n, 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                       * 330.0f * (float) t / 48000.0f));
            }
            proc.processBlock (buf, midi);
            if (b >= blocks / 2)                    // measure the settled half only
                for (int n = 0; n < 512; ++n)
                {
                    sumSq[0] += (double) buf.getSample (0, n) * buf.getSample (0, n);
                    sumSq[1] += (double) buf.getSample (1, n) * buf.getSample (1, n);
                }
        }
        // DERIVED from the loop, not written out again: the divisor read
        // `256.0 * 512.0` against a settled half of 30 × 512 samples — an ~8.5×
        // over-division. It did not fail (0.25-amplitude sines settle near
        // 0.177 RMS, and even 0.061 clears the 0.05 bar), which is the whole
        // problem: a permanent guard was passing by 20 % instead of 3.5× and
        // printing a wrong number in its own failure message, so the next
        // ordinary level change would have turned it red for a reason that was
        // never about the channels. `blocks - blocks / 2` is the same count the
        // `b >= blocks / 2` gate admits, at any `blocks`.
        const double settledSamples = (double) (blocks - blocks / 2) * 512.0;
        const double rmsL = std::sqrt (sumSq[0] / settledSamples);
        const double rmsR = std::sqrt (sumSq[1] / settledSamples);
        juce::String msg;
        msg << "stereoWrapper (" << tag << "): LEFT carries audio (rmsL="
            << (float) rmsL << " rmsR=" << (float) rmsR << ")";
        check (rmsL > 0.05, msg.toRawUTF8());
        msg.clear();
        msg << "stereoWrapper (" << tag << "): RIGHT carries audio (rmsR=" << (float) rmsR << ")";
        check (rmsR > 0.05, msg.toRawUTF8());
        msg.clear();
        msg << "stereoWrapper (" << tag << "): channels within 6 dB of each other";
        check (rmsL < rmsR * 2.0 && rmsR < rmsL * 2.0, msg.toRawUTF8());
    };
    run (0.0f,  "defaults");
    run (50.0f, "loudness 50");

    // ---- THE FIELD CROSS-PRODUCT: a PUSHED chain x every non-zero Clip Mix --
    //
    // This is the reported scenario, and until now nothing ran it. The two
    // halves existed separately and neither one covers it:
    //
    //   * `testClipMixCannotChangeTheDefaultPresetsSound` sweeps Clip Mix, but
    //     at the DEFAULT preset — where `clipDrive == 0` exact-skips the clip
    //     sub-block, so every mix branch provably lands on the same float. It
    //     proves the mix is INERT there, which is the opposite of exercising it.
    //   * The `loudness N` cases push the chain, but leave Clip Mix at its
    //     default, so they sweep exactly one point of it.
    //
    // The owner's report is gated on the mix being NON-ZERO with the chain
    // working — the one configuration in which this stage's value reaches the
    // wet ring at all, and therefore the only one in which the ring's
    // per-channel zero substitution can be reached. Every value in the sweep is
    // non-zero on purpose: 0 is covered above as the inert case, and a mix of 0
    // is the setting the field report says makes the fault GO AWAY, so including
    // it here would add a row that passes for the wrong reason.
    //
    // 1 % is in the list because a barely-open mix is the hardest case for a
    // fault that scales with the mix, not the easiest: it is where a poisoned
    // wet value contributes least and a level assertion is closest to passing
    // anyway. Cheap: five renders of 60 blocks each.
    for (const float mixPct : { 1.0f, 25.0f, 50.0f, 99.0f, 100.0f })
    {
        juce::String tag;
        tag << "loudness 70, Clip Mix " << mixPct << " %";
        run (70.0f, tag.toRawUTF8(), mixPct);
    }

    // The configurations the plain run above does NOT cover, each a candidate
    // for a channel-asymmetric defect the DSP suite would miss: the editor
    // alive while audio runs, the oversampler engaged (its polyphase state is
    // per channel), a factory preset applied, and a session round-trip.
    // setup returns a keep-alive token so a configuration can hold an object
    // (the editor) LIVE across the processing loop, not merely construct it.
    auto runWith = [] (const char* tag,
                       std::function<std::shared_ptr<void> (AnabasisAudioProcessor&)> setup)
    {
        AnabasisAudioProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        auto keepAlive = setup (proc);

        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        double sumSq[2] = { 0.0, 0.0 };
        for (int b = 0; b < 60; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                const int t = b * 512 + n;
                buf.setSample (0, n, 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                       * 220.0f * (float) t / 48000.0f));
                buf.setSample (1, n, 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                       * 330.0f * (float) t / 48000.0f));
            }
            proc.processBlock (buf, midi);
            if (b >= 30)
                for (int n = 0; n < 512; ++n)
                {
                    sumSq[0] += (double) buf.getSample (0, n) * buf.getSample (0, n);
                    sumSq[1] += (double) buf.getSample (1, n) * buf.getSample (1, n);
                }
        }
        const double rmsL = std::sqrt (sumSq[0] / (30.0 * 512.0));
        const double rmsR = std::sqrt (sumSq[1] / (30.0 * 512.0));
        juce::String msg;
        msg << "stereoWrapper (" << tag << "): both channels carry audio (L="
            << (float) rmsL << " R=" << (float) rmsR << ")";
        check (rmsL > 0.05 && rmsR > 0.05, msg.toRawUTF8());
    };

    runWith ("editor attached", [] (AnabasisAudioProcessor& p) -> std::shared_ptr<void>
    {
        // The editor stays ALIVE through the whole processing loop via the
        // keep-alive token; the deleter runs editorBeingDeleted first, per the
        // JUCE ownership contract.
        auto* ed = p.createEditor();
        return std::shared_ptr<void> (ed, [&p, ed] (void*)
        {
            p.editorBeingDeleted (ed);
            delete ed;
        });
    });
    runWith ("oversampling 16x linear", [] (AnabasisAudioProcessor& p) -> std::shared_ptr<void>
    {
        p.internalState.state().setProperty (iid::oversample, 4, nullptr);
        p.internalState.state().setProperty (iid::osPhase, 1, nullptr);
        p.prepareToPlay (48000.0, 512);      // re-prepare builds the OS chain
        return nullptr;
    });
    runWith ("factory preset applied", [] (AnabasisAudioProcessor& p) -> std::shared_ptr<void>
    {
        // Index 3 = "EDM Club", the bank's LOUDEST table (loudness 80) — the
        // point of this configuration is maximum macro push through the chain.
        // It read `1` with the comment "Loud Pop" until 2026-08-05, when
        // "Default" took index 0 and shifted every entry by one: index 1 is
        // "Transparent Master" (loudness 25), so the case had quietly become
        // the gentlest preset in the bank rather than the hardest-driven one.
        p.applyFactoryPreset (3);
        return nullptr;
    });
    runWith ("after a session round-trip", [] (AnabasisAudioProcessor& p) -> std::shared_ptr<void>
    {
        juce::MemoryBlock blob;
        p.getStateInformation (blob);
        p.setStateInformation (blob.getData(), (int) blob.getSize());
        return nullptr;
    });

    // mono → stereo (KI-009): the layout Anabasis REFUSED until 0.1.1. A host
    // with a mono source then negotiated stereo→stereo and fed the signal on
    // whichever single input pin its convention chose — and this chain is
    // strictly dual-mono, so the other output channel mastered silence. The
    // wrapper now accepts mono in and duplicates it; both output channels must
    // carry the (same) programme.
    {
        AnabasisAudioProcessor proc;
        check (proc.checkBusesLayoutSupported (
                   juce::AudioProcessor::BusesLayout { { juce::AudioChannelSet::mono() },
                                                       { juce::AudioChannelSet::stereo() } }),
               "stereoWrapper (mono in): the mono->stereo layout is accepted");
        const bool applied = proc.setBusesLayout (
            juce::AudioProcessor::BusesLayout { { juce::AudioChannelSet::mono() },
                                                { juce::AudioChannelSet::stereo() } });
        check (applied, "stereoWrapper (mono in): the mono->stereo layout applies");
        proc.prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        double sumSq[2] = { 0.0, 0.0 };
        for (int b = 0; b < 60; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                const int t = b * 512 + n;
                // The mono programme arrives on channel 0 ONLY — channel 1 is
                // the host-convention silence the field report describes.
                buf.setSample (0, n, 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                       * 220.0f * (float) t / 48000.0f));
                buf.setSample (1, n, 0.0f);
            }
            proc.processBlock (buf, midi);
            if (b >= 30)
                for (int n = 0; n < 512; ++n)
                {
                    sumSq[0] += (double) buf.getSample (0, n) * buf.getSample (0, n);
                    sumSq[1] += (double) buf.getSample (1, n) * buf.getSample (1, n);
                }
        }
        const double rmsL = std::sqrt (sumSq[0] / (30.0 * 512.0));
        const double rmsR = std::sqrt (sumSq[1] / (30.0 * 512.0));
        juce::String msg;
        msg << "stereoWrapper (mono in): BOTH output channels carry the duplicated programme (L="
            << (float) rmsL << " R=" << (float) rmsR << ")";
        check (rmsL > 0.05 && rmsR > 0.05, msg.toRawUTF8());
    }

    // mono → mono (0.1.2 item 13, ADR-0023): the same plugin at nCh = 1 — the
    // layout dual-mono/multi-mono host racks need. One channel in, one out,
    // the engine prepared mono; the programme must come through.
    {
        AnabasisAudioProcessor proc;
        const juce::AudioProcessor::BusesLayout monoMono { { juce::AudioChannelSet::mono() },
                                                           { juce::AudioChannelSet::mono() } };
        check (proc.checkBusesLayoutSupported (monoMono),
               "stereoWrapper (mono->mono): the layout is accepted");
        check (! proc.checkBusesLayoutSupported (
                   juce::AudioProcessor::BusesLayout { { juce::AudioChannelSet::stereo() },
                                                       { juce::AudioChannelSet::mono() } }),
               "stereoWrapper (mono->mono): stereo->mono stays refused (no downmix rule)");
        check (proc.setBusesLayout (monoMono),
               "stereoWrapper (mono->mono): the layout applies");
        proc.prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> buf (1, 512);
        juce::MidiBuffer midi;
        double sumSq = 0.0;
        for (int b = 0; b < 60; ++b)
        {
            for (int n = 0; n < 512; ++n)
                buf.setSample (0, n, 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                       * 220.0f * (float) (b * 512 + n) / 48000.0f));
            proc.processBlock (buf, midi);
            if (b >= 30)
                for (int n = 0; n < 512; ++n)
                    sumSq += (double) buf.getSample (0, n) * buf.getSample (0, n);
        }
        const double rms = std::sqrt (sumSq / (30.0 * 512.0));
        juce::String msg;
        msg << "stereoWrapper (mono->mono): the one channel masters the programme (rms="
            << (float) rms << ")";
        check (rms > 0.05, msg.toRawUTF8());
    }

    // The DIAGNOSTIC configurations of the KI-009 0.1.2 field report — the
    // modes the owner had engaged when observing the asymmetry, none of which
    // the battery covered: Delta (the observation instrument itself), the
    // loudness-comp monitor, an unlinked limiter, dither with shaping, true
    // peak, and a non-48k rate. Each feeds per-channel-distinct sines and
    // asserts BOTH channels alive; Delta additionally pins the channels
    // within 6 dB of each other, since "one delta channel loud, the other
    // quiet" was the report's fingerprint.
    {
        AnabasisAudioProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        // Drive the limiter hard so the difference signal EXISTS (+18 dB push
        // into the ceiling), then listen to Delta.
        auto set = [&proc] (const char* id, float denorm)
        {
            auto* par = proc.apvts.getParameter (id);
            par->setValueNotifyingHost (par->getNormalisableRange().convertTo0to1 (denorm));
        };
        set (pid::limGain, 18.0f);
        set (pid::deltaMonitor, 1.0f);

        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        double sumSq[2] = { 0.0, 0.0 };
        for (int b = 0; b < 60; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                const int t = b * 512 + n;
                buf.setSample (0, n, 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                       * 220.0f * (float) t / 48000.0f));
                buf.setSample (1, n, 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                       * 330.0f * (float) t / 48000.0f));
            }
            proc.processBlock (buf, midi);
            if (b >= 30)
                for (int n = 0; n < 512; ++n)
                {
                    sumSq[0] += (double) buf.getSample (0, n) * buf.getSample (0, n);
                    sumSq[1] += (double) buf.getSample (1, n) * buf.getSample (1, n);
                }
        }
        const double rmsL = std::sqrt (sumSq[0] / (30.0 * 512.0));
        const double rmsR = std::sqrt (sumSq[1] / (30.0 * 512.0));
        juce::String msg;
        msg << "stereoWrapper (delta engaged): both delta channels carry the difference (L="
            << (float) rmsL << " R=" << (float) rmsR << ")";
        check (rmsL > 0.01 && rmsR > 0.01, msg.toRawUTF8());
        msg.clear();
        msg << "stereoWrapper (delta engaged): the delta channels sit within 6 dB of each other";
        check (rmsL < rmsR * 2.0 && rmsR < rmsL * 2.0, msg.toRawUTF8());
    }
    runWith ("loudness comp monitor on", [] (AnabasisAudioProcessor& p) -> std::shared_ptr<void>
    {
        auto* par = p.apvts.getParameter (pid::loudnessComp);
        par->setValueNotifyingHost (1.0f);
        return nullptr;
    });
    runWith ("limiter link 0%", [] (AnabasisAudioProcessor& p) -> std::shared_ptr<void>
    {
        p.apvts.getParameter (pid::stereoLink)->setValueNotifyingHost (0.0f);
        return nullptr;
    });
    runWith ("dither 16-bit shaped", [] (AnabasisAudioProcessor& p) -> std::shared_ptr<void>
    {
        auto* d = p.apvts.getParameter (pid::dither);
        d->setValueNotifyingHost (d->getNormalisableRange().convertTo0to1 (1.0f));
        p.apvts.getParameter (pid::ditherShaping)->setValueNotifyingHost (1.0f);
        return nullptr;
    });
    runWith ("true peak on", [] (AnabasisAudioProcessor& p) -> std::shared_ptr<void>
    {
        p.apvts.getParameter (pid::truePeakMode)->setValueNotifyingHost (1.0f);
        return nullptr;
    });
    runWith ("44.1 kHz", [] (AnabasisAudioProcessor& p) -> std::shared_ptr<void>
    {
        p.prepareToPlay (44100.0, 512);      // the sines shift ~8 %; still programme
        return nullptr;
    });

    // The KI-009 0.1.3 field configuration, pinned EXACTLY as the owner ran
    // it: both stereo links at 0 (per-channel detectors and envelopes), the
    // comp threshold pulled low and the limiter gain pushed high so BOTH
    // stages draw reduction on BOTH channels. The field observation was comp
    // GR on both lanes but limiter GR on the RIGHT lane only, with the left
    // output silent — which localises the field kill to the span between the
    // comp's output and the limiter's detector tap (the wet ring): comp GR on
    // L proves the left channel alive INTO the comp, zero limiter GR on L
    // proves it dead AT the tap. This case asserts the whole fingerprint
    // headlessly — per-channel comp GR, per-channel limiter GR, both outputs
    // alive — at BOTH oversampling extremes of that span: OS Off (the
    // shipped default), where the span holds NO per-channel recursive state
    // at all (ClipSat at any setting is channel-symmetric and self-healing,
    // the push is one shared scalar, the ring indices are shared), and 4×,
    // where JUCE's per-channel polyphase oversampler is the one stateful
    // occupant. The pair is the decisive field experiment in headless form:
    // a field bug that follows the OS toggle names the oversampler; one that
    // survives OS Off has no in-plugin site left. See KNOWN_ISSUES KI-009,
    // 0.1.3 addendum.
    {
        auto runFieldConfig = [] (int osIdx, const char* tag)
        {
            AnabasisAudioProcessor proc;
            proc.internalState.state().setProperty (iid::oversample, osIdx, nullptr);
            proc.prepareToPlay (48000.0, 512);
            auto set = [&proc] (const char* id, float denorm)
            {
                auto* par = proc.apvts.getParameter (id);
                par->setValueNotifyingHost (par->getNormalisableRange().convertTo0to1 (denorm));
            };
            set (pid::compStereoLink, 0.0f);
            set (pid::stereoLink,     0.0f);
            set (pid::compThreshold,  -30.0f);
            // The FULL +18 dB push, not a midway value: the comp above takes
            // ~4 dB first (−15 dBFS RMS against the −30 threshold at 1.5:1),
            // so the −12 dBFS sines reach the limiter near −16 dBFS + push —
            // +12 left them ~4 dB UNDER the ceiling and the limiter idle,
            // which failed this case's own premise on first run.
            set (pid::limGain,        18.0f);

            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            double sumSq[2] = { 0.0, 0.0 };
            for (int b = 0; b < 60; ++b)
            {
                for (int n = 0; n < 512; ++n)
                {
                    const int t = b * 512 + n;
                    buf.setSample (0, n, 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                           * 220.0f * (float) t / 48000.0f));
                    buf.setSample (1, n, 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                           * 330.0f * (float) t / 48000.0f));
                }
                proc.processBlock (buf, midi);
                if (b >= 30)
                    for (int n = 0; n < 512; ++n)
                    {
                        sumSq[0] += (double) buf.getSample (0, n) * buf.getSample (0, n);
                        sumSq[1] += (double) buf.getSample (1, n) * buf.getSample (1, n);
                    }
            }
            const double rmsL = std::sqrt (sumSq[0] / (30.0 * 512.0));
            const double rmsR = std::sqrt (sumSq[1] / (30.0 * 512.0));
            juce::String msg;
            msg << "stereoWrapper (" << tag << "): both channels carry audio (L="
                << (float) rmsL << " R=" << (float) rmsR << ")";
            check (rmsL > 0.02 && rmsR > 0.02, msg.toRawUTF8());
            for (int ch = 0; ch < 2; ++ch)
            {
                msg.clear();
                msg << "stereoWrapper (" << tag << "): comp GR engaged on channel " << ch
                    << " (" << proc.meterCompGrDbCh (ch) << " dB)";
                check (proc.meterCompGrDbCh (ch) < -0.5f, msg.toRawUTF8());
                msg.clear();
                msg << "stereoWrapper (" << tag << "): limiter GR engaged on channel " << ch
                    << " (" << proc.meterLimGrDbCh (ch) << " dB)";
                check (proc.meterLimGrDbCh (ch) < -0.5f, msg.toRawUTF8());
            }
        };
        runFieldConfig (0, "field config, links 0, OS off");
        runFieldConfig (2, "field config, links 0, OS 4x");
    }
}

int main (int argc, char** argv)
{
    // Unbuffered stdout: CI pipes are fully buffered, so a crash mid-suite
    // used to eat every line printed before it — a failing run showed exit 1
    // and nothing else. Costs nothing measurable at this output volume.
    setvbuf (stdout, nullptr, _IONBF, 0);
    juce::ScopedJuceInitialiser_GUI juceInit;
    const bool writeSnapshot = argc > 1 && juce::String (argv[1]) == "--write-snapshot";

    testRegistrySnapshot (writeSnapshot);
    if (! writeSnapshot)
    {
        testStateRoundTrip();
        testCorruptAndForeignState();
        testMacroDefaultIsFixedPoint();
        testAbSlotsAndTiers();
        testMacroRestoreDoesNotClobber();
        testAbSwitchRequestsDuck();
        testUndoRequestsDuck();
        testMeterPublication();
        testTheWaveformStatisticsRowsReadTheirStandards();
        testAGestureEndWithoutACountedBeginIsIgnored();
        testAMacroGestureWinsADetachRacingItInOneDrain();
        testTeardownAndReengageInvariants();
        testStateReplacementAndHistoryConsistency();
        testPreparedStateAndSlotOwnership();
        testTheFrozenLatchNeedsNoThreadCrossing();
        testAFrozenLatchDoesNotFollowTheSlotSwitch();
        testHistoryOwnershipAcrossAStateLoad();
        testBothChannelsCarryAudioThroughTheWrapper();
        testClipMixCannotChangeTheDefaultPresetsSound();
        testARestoreDropsStagedDetachBits();
        testTheDrainTickReEngagesBeforeItMaps();
        testThePostedDrainAlsoTakesTheWrapperBitsFirst();
        testDetachAndReengageGrammar();
        testUndoIsPerSlotGestureCoalescedAndMaskWide();
        testANoOpPresetApplyIsNotAUserAction();
        testANoOpPresetApplyIsNotAUserActionAfterASessionRestore();
        testANoOpPresetApplyDoesNotEatTheOldestUndoStep();
        testThePopupShieldActuallyCoversTheEditor();
        testAPopupRowKeepsItsLabelOutOfTheShortcutStrip();
        testTheResizableFrameOverrideDiscriminatesItsCallers();
        testEveryComboMenuFitsItsControl();
        testAShortcutRowIsMeasuredWideEnoughForItsOwnLabel();
        testAMalformedStoredSlotCannotSplitSoundFromMetadata();
        testARootlessSurfaceDropsTheActiveSlotsMetadataToo();
        testFactoryPresets();
        testALockedCeilingSurvivesAPresetThatNamesIt();
        testMeterResetClearsSessionHolds();
        testGrRingResetEpoch();
        testTheSettingsPanelFollowsAProjectLoad();
        testAValueBoxClickIsNotAMacroGesture();
        testTheSettingsCallbacksReachTheLiveTree();
        testAFactoryApplyWritesEachParameterOnce();
        testTheSavePresetNameFieldIsTaggedForItsFocusGlow();
        testAnOutOfListUiScaleClampsConsistently();
        testTheCurveWellCachesWithoutChangingWhatItDraws();
        testARewoundSpectrumRingDropsThePreviousTrace();
        testSavingOverAFactoryNameKeepsTheArrowsOnTheUserPreset();
        testTheRingWalksPastAnUnreadablePreset();
        testPresetIdentitySharedName();
        testFactoryPresetIdIntegrity();
        testPresetIdentityAcrossRestore();
        testTheAboutPanelShowsTheBuildItIsRunning();
        testFrequencyTextEntrySpeaksMasteringShorthand();
        testTheGraphWellViewsOnlyClaimTheirModeChips();
        testEveryKnobAndComboCarriesATooltip();
        testGrHistoryWindowNeverAsksForTheHeadSlot();
        testMetersReadTheRenderNotTheMonitor();
        testModeSwitchIsSoundNeutral();
        testLearnCommitAndAdaptiveRoundTrip();
        testDrainInsideRestoreIsSuppressed();
        testTheWholeTickIsSuppressedInsideARestore();
        testAbRawExact();
        testFrozenSlotRoundTrip();
        testFrozenTrimRestore();
        testAbToleranceRules();
        testPresetContract();
        testTheDirtyMarkerMeasuresOnlyWhatAPresetCanCarry();
        testThePresetWriterAndTheDirtyMarkerCoverTheSameParameters();
        testAPresetApplyKeepsTheFrozenLatchItDidNotChange();
        testAPresetApplyDropsTheMacroBaselineOnBothPaths();
        testMissingChildrenReadAsDefaults();
        testLatencyNotifyIsBatchedAcrossARead();
        testRawRoundTripIsIdempotent();
        testTheCeilingAdvertisesTheUnitItEnforces();
        testCeilingIsQuantisedToTwoDecimals();
        testCachedParamsMapping();
    }

    std::printf ("%s: %d checks, %d failure(s)\n",
                 failures == 0 ? "PASS" : "FAIL", checks, failures);
    return failures == 0 ? 0 : 1;
}
