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
    check (count == 49,  "registry: exactly 49 parameters (DESIGN §4.2)");
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
        buf.clear();
        fresh.processBlock (buf, midi);
        check (sameVector (trimsOf (fresh), saved),
               "frozenRestore: an unprimed session load restores the vector on the first block");
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
             && sAfter.getProperty ("presetName").toString().isEmpty(),
           "tolerance: a root without AB resets the slot fields to defaults");

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
}

// ---------------------------------------------------------------------------
// §7 factory presets: compiled-in override tables — defaults first, then the
// intents — through the SAME lock/exclusion semantics as file presets, with
// an empty detach mask (nothing loads pre-detached), one undo step, and the
// dirty marker clean right after an apply and set by the next edit.
static void testFactoryPresets()
{
    AnabasisAudioProcessor proc;
    auto& apvts = proc.apvts;

    int count = 0;
    const auto* table = PresetManager::factoryPresets (count);
    check (count == 12, "factory: the brief's >=12-preset bank is compiled in (5 named + 7 owner-approved 2026-08-02)");

    // Apply EDM Club (index 2): macros land, style lands, ceiling lands.
    check (proc.applyFactoryPreset (2), "factory: apply succeeds");
    check (proc.currentPresetName() == juce::String (table[2].name),
           "factory: the preset name is the table's");
    check (std::abs (apvts.getRawParameterValue (pid::loudness)->load() - 80.0f) < 0.5f,
           "factory: the loudness macro landed");
    check (std::abs (apvts.getRawParameterValue (pid::ceiling)->load() - (-0.5f)) < 0.01f,
           "factory: the ceiling override landed");
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
    feedTone (0.005f, 20, (3 * warm + 8) * 512);
    check (proc.meterDbTpMax() < -40.0f,
           "meterReset: a session load cleared the previous programme's holds");
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

    proc.prepareToPlay (48000.0, 512);                 // reaches GrHistoryBuffer::reset()
    check (ring.resetEpoch() == e0 + 2, "grEpoch: one reset moves the epoch by exactly 2");
    check ((ring.resetEpoch() & 1u) == 0u, "grEpoch: and lands even");
    check (ring.available() == 0,
           "grEpoch: the index rewound — which is why the epoch must exist");
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
// kCacheOrder and CachedParams::toEngine are coupled POSITIONALLY: inserting a
// row in one without the matching line in the other silently shifts every
// later field, and the static_assert only catches a length change. Distinct
// values per parameter, checked field by field, catch a shift of any size.
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
    set (pid::stereoLink, 80.0f);
    set (pid::colourDepth, 60.0f);

    check (proc.cachedForTest().allResolved(),
           "cache: every cached id resolves to a live parameter (a null slot would feed 0)");

    anabasis::EngineParameters e;
    proc.cachedForTest().toEngine (e);

    auto near = [] (float a, float b) { return std::abs (a - b) < 1.0e-3f; };
    check (near (e.inputGainDb, 7.0f)        && near (e.scHpfFreqHz, 120.0f),   "cache: input/detector fields");
    check (near (e.compRatio, 3.0f)          && near (e.compThresholdDb, -18.0f)
            && near (e.compKneeDb, 9.0f)     && near (e.compMix, 0.40f),        "cache: compressor fields");
    check (near (e.clipDriveDb, 5.0f)        && near (e.colourBalance, 0.5f)
            && near (e.dynTiltDb, 1.25f)     && near (e.colourDepth, 0.60f),    "cache: clip/colour fields");
    check (near (e.limGainDb, 11.0f)         && near (e.lookaheadMs, 4.0f)
            && near (e.limReleaseMs, 250.0f) && near (e.stereoLink, 0.80f),     "cache: limiter fields");
    check (near (e.eqTiltDb, -2.0f)          && near (e.eqLowShelfGainDb, 3.0f)
            && near (e.eqBell2Q, 2.0f),                                          "cache: eq fields");
    check (near (e.ceilingDbTp, -3.0f),                                          "cache: shared/output fields");
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
    testAGestureEndWithoutACountedBeginIsIgnored();
    testDetachAndReengageGrammar();
    testUndoIsPerSlotGestureCoalescedAndMaskWide();
    testFactoryPresets();
    testMeterResetClearsSessionHolds();
    testGrRingResetEpoch();
        testMetersReadTheRenderNotTheMonitor();
        testModeSwitchIsSoundNeutral();
        testLearnCommitAndAdaptiveRoundTrip();
        testDrainInsideRestoreIsSuppressed();
        testAbRawExact();
        testFrozenSlotRoundTrip();
        testFrozenTrimRestore();
        testAbToleranceRules();
        testPresetContract();
        testMissingChildrenReadAsDefaults();
        testLatencyNotifyIsBatchedAcrossARead();
        testRawRoundTripIsIdempotent();
        testCachedParamsMapping();
    }

    std::printf ("%s: %d checks, %d failure(s)\n",
                 failures == 0 ? "PASS" : "FAIL", checks, failures);
    return failures == 0 ? 0 : 1;
}
