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
#include <cstdio>

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
    trims.setProperty ("release",    0.25,  nullptr);
    trims.setProperty ("stereoLink", -0.1,  nullptr);
    trims.setProperty ("scHpf",      3.0,   nullptr);
    trims.setProperty ("dynTilt",    0.4,   nullptr);
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
             && juce::exactlyEqual ((double) s2.getChildWithName ("FROZEN_TRIMS").getProperty ("release"), 0.25),
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
        testMeterPublication();
        testModeSwitchIsSoundNeutral();
        testDrainInsideRestoreIsSuppressed();
        testAbRawExact();
        testFrozenSlotRoundTrip();
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
