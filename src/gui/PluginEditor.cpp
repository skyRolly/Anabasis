#include "PluginEditor.h"
#include "../PluginProcessor.h"
#include <cstring>

using namespace abgui;

// The C8 rule ("free tooltip prose is owner-supplied, not invented") was
// DISCHARGED for tooltips by the round-2 item-11 directive: the owner asked
// for a generated complete set, so the copy below ships ⊕ instead of waiting.
// Tooltips read terser without a trailing full stop (the sibling's rule,
// ADR-0009). Applied centrally so every control set up through the helpers is
// covered. This was a stub (`trim()` only) while the tips themselves were just
// the registry names; both halves became real with the R2 item-11 tooltip set.
static juce::String tidyTip (const juce::String& tip)
{
    auto t = tip.trim();
    while (t.endsWithChar ('.')) t = t.dropLastCharacters (1).trimEnd();
    return t;
}

// ----------------------------------------------------------------------------
//  The parameter tooltip set (R2 item 11, 2026-08-05 owner directive) — one
//  descriptive line per parameter, in the sibling's tooltip voice (terse,
//  plain " - " dash, no trailing period; `tidyTip` enforces the last rule).
//  Held in ONE table so a knob and its Simple-view twin (ceiling, truePeakMode)
//  can never drift apart. Accessibility titles deliberately do NOT read these:
//  they stay the registry names (brief §8), so a screen reader announces what
//  the automation lane shows, and the hover hint stays free to explain.
//  Wording ⊕ for the fine review, like all product copy taken under the
//  standing approval.
// ----------------------------------------------------------------------------
static juce::String tipFor (const char* id)
{
    struct Row { const char* id; const char* tip; };
    static constexpr Row rows[] = {
        { pid::bypass,            "" },   // the red pill labels itself (sibling: no tip)
        { pid::advancedMode,      "Per-stage control over the same sound - switching views never changes it" },
        { pid::loudness,          "How hard the adaptive chain pushes - 0 adds no push, but the Ceiling still holds" },
        { pid::character,         "Clean to Colour - how much of the push comes from saturation rather than clean limiting" },
        { pid::tone,              "Dark to bright tilt of the overall result" },
        { pid::ceiling,           "The output limit - nothing leaves the plugin above it. Sample peak by default; engage TP to hold it in dBTP" },
        { pid::freeze,            "Hold the adaptive trims exactly where they are now" },
        { pid::loudnessComp,      "Listen at matched loudness, so louder can't pass for better" },
        { pid::deltaMonitor,      "Solo the difference - hear exactly what the processing removes" },
        { pid::inputGain,         "Level into the chain, before any processing" },
        { pid::scHpfFreq,         "Sidechain high-pass for both detectors - keeps low end from pumping the compressor and limiter" },
        { pid::compRatio,         "How strongly the glue compressor reduces past the threshold" },
        { pid::compThreshold,     "Where the glue compressor starts to work" },
        { pid::compAttack,        "How quickly the compressor responds to a level rise" },
        { pid::compRelease,       "How quickly the compressor recovers" },
        { pid::compAutoRelease,   "Programme-dependent release - two stages follow the material" },
        { pid::compKnee,          "Width of the soft knee around the threshold" },
        { pid::compDetector,      "RMS averages the level, Peak follows every transient" },
        { pid::compMix,           "Parallel compression - blend compressed with dry" },
        { pid::compStereoLink,    "How much both channels share one compressor gain - lower lets each channel breathe on its own" },
        { pid::clipShape,         "Hard to soft clipping curve - shown live in the display" },
        { pid::clipDrive,         "Push into the clipper, level-compensated - adds density, not volume" },
        { pid::clipMix,           "Blend clipped with dry" },
        { pid::colourModel,       "The saturation voicing - Clean, Tape, Tube or Transistor" },
        { pid::colourBalance,     "Odd to even harmonic balance of the colour" },
        { pid::colourTone,        "Dark to bright voicing of the added harmonics" },
        { pid::colourDepth,       "How much colour the stage adds" },
        { pid::dynTilt,           "Dynamic Tame - softens harsh highs only when the material turns aggressive" },
        { pid::limGain,           "The push into the limiter" },
        { pid::lookahead,         "How far ahead the limiter sees - longer catches transients more cleanly" },
        { pid::limRelease,        "How quickly the limiter recovers" },
        { pid::limAutoRelease,    "Programme-dependent release - two stages follow the material" },
        { pid::limStyle,          "Release voicing - Transparent, Punchy or Loud" },
        { pid::stereoLink,        "How much both channels share one gain - full link keeps the image stable" },
        { pid::transientPreserve, "Keeps attack transients alive through heavy limiting" },
        { pid::truePeakMode,      "Catch inter-sample peaks - the Ceiling then holds in dBTP instead of sample peak" },
        { pid::eqTilt,            "Tilts the whole spectrum around ~700 Hz" },
        { pid::eqLowShelfFreq,    "Low shelf corner frequency" },
        { pid::eqLowShelfGain,    "Low shelf level" },
        { pid::eqHighShelfFreq,   "High shelf corner frequency" },
        { pid::eqHighShelfGain,   "High shelf level" },
        { pid::eqBell1Freq,       "Bell 1 centre frequency" },
        { pid::eqBell1Gain,       "Bell 1 level" },
        { pid::eqBell1Q,          "Bell 1 width - higher is narrower" },
        { pid::eqBell2Freq,       "Bell 2 centre frequency" },
        { pid::eqBell2Gain,       "Bell 2 level" },
        { pid::eqBell2Q,          "Bell 2 width - higher is narrower" },
        { pid::eqPosition,        "EQ before the compressor or after the limiter - the Ceiling holds either way" },
        { pid::dither,            "Bit-depth dither for the final export - Off, 16-bit or 24-bit TPDF" },
        { pid::ditherShaping,     "Shape the dither noise away from where the ear is most sensitive" },
    };
    for (const auto& r : rows)
        if (std::strcmp (r.id, id) == 0)
        {
            // §5.3/§6.3 detach-badge legend (0.1.1, owner item 9: the dot's
            // semantics read as arbitrary — "why only these knobs, and why
            // doesn't returning to the preset value clear it?"). The answer
            // lives ON the only controls that can ever show it — the nine
            // macro-managed knobs — appended here from the ONE managed list
            // rather than written into nine strings, so the badge set and
            // the legend set cannot drift apart. The dot marks detachment
            // FROM THE MACROS, not difference from the preset: it appears
            // when a manual edit takes the knob off its macro curve, and
            // clears when a macro gesture (or Simple's reset dot, or a
            // preset load's own mask) re-lands the curve — matching the
            // value alone never re-attaches, so it never clears the dot.
            for (int i = 0; i < managed_params::kCount; ++i)
                if (std::strcmp (managed_params::ids[i], id) == 0)
                    return juce::String (r.tip)
                         + ". A corner dot means this knob is detached from the macros"
                           " and holds your value - move a macro to re-land it";
            return r.tip;
        }
    // Reaching here means a parameter was added without a tooltip — make the
    // gap visible in a debug build instead of silently hoverless.
    jassertfalse;
    return {};
}

// The build system defines both (CMakeLists `target_compile_definitions`, for the
// plugin AND the state-test target that constructs this editor). The fallbacks
// are for a build that bypasses CMake, and are deliberately NOT a release number
// so a stale one cannot masquerade as shipped — the same guard the sibling
// product carries.
#ifndef ANABASIS_VERSION_STRING
 #define ANABASIS_VERSION_STRING "0.0.0-dev"
#endif
#ifndef ANABASIS_BUILD_NUMBER
 #define ANABASIS_BUILD_NUMBER "0"
#endif

// The UI-scale step list, in ONE place — and that place is `InternalState.h`,
// beside the identifier whose legal values it defines, because the state layer
// needs it too: `replaceFrom` normalises a persisted percent onto the ladder as
// part of the §4.4 read rules. These are aliases, not a second list. The combo
// builds its item list from them, `applyUiScale` maps the stored percent back
// through them, and the settings re-seed compares against them.
static constexpr auto& kScaleSteps  = ui_scale::steps;
static constexpr int   kNumScaleSteps = ui_scale::numSteps;

// The ONE reading of `iid::uiScale`, because three sites derived a step from it
// with three different fallbacks and could therefore disagree about the same
// stored number. A percent that is not a legal step — hand-edited state, a
// session written by a build whose list has since changed — used to be silently
// IGNORED rather than clamped: `applyUiScale` left `scale` at 1.0, the
// constructor showed 100 %, and `refreshInternalSettingsBoxes` kept whatever the
// box already had. That last one is the divergence that matters, because it is
// the path a project load takes: the window rendered at 100 % while the Settings
// panel went on displaying the previously selected step.
//
// Clamping to the NEAREST step is the read rule the rest of this tree already
// uses for persisted values (`adoptParamsTree`'s missing/aliased-field rules,
// `setupComboInternal`'s `jlimit`), and it is a strict generalisation of a range
// clamp: 110 → 100, 50 → 75, 300 → 150 (the XS..XL ladder in `ui_scale`; the
// examples read 80/200 from the seven-step ladder this replaced). Returning an INDEX rather than a scale
// is what makes the rendered transform and the displayed selection the same
// decision instead of two decisions that happen to agree.
static int nearestScaleIndex (int pct) noexcept { return ui_scale::nearestIndex (pct); }

// ============================================================================
//  Backdrop / ABControl paint — family grammar (provenance in the header).
// ============================================================================
void AnabasisAudioProcessorEditor::Backdrop::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0x06080b).withAlpha (0.80f));
    const auto pf = panel.toFloat();

    if (dropShadow)
        for (int i = 5; i >= 0; --i)
        {
            const float t  = (float) i / 5.0f;
            const float ex = 1.5f + t * 13.0f;
            g.setColour (juce::Colours::black.withAlpha (0.13f * (1.0f - t)));
            g.fillRoundedRectangle (pf.expanded (ex).translated (0.0f, 2.5f), 12.0f + ex);
        }

    glass::fillPanel (g, pf, 12.0f, colours::bgPanel, 1.0f);

    if (! aboutText)
        return;

    // THE ABOUT COPY. `aboutText` had exactly one reader — `mouseDown`, where it
    // makes a click anywhere dismiss — so the flag named a panel whose content
    // nothing drew: the overlay opened blank but for the hyperlink, and the
    // `ANABASIS_VERSION_STRING` / `ANABASIS_BUILD_NUMBER` definitions
    // `CMakeLists.txt` sets (and `CI_CD.md` describes as "the run number becomes
    // the About-box build number") had no consumer in the tree at all. Restored
    // here rather than in a new child component: `resized()` already reserves
    // this space — the panel is 440×290 with the link band at
    // `panel.getBottom() - 50` — and every other overlay's content is likewise
    // painted by its Backdrop.
    //
    // The strings are the ones this product already owns — plus, since
    // 2026-08-05, ONE flowing description sentence. C8 said product prose is
    // never invented in code; the owner's round-2 directive explicitly asked
    // for this copy to be generated here (⊕ — "I will let you know later if
    // it needs tweaking"), which is the owner supplying the authority if not
    // the words; the 0.1.1 directive then asked it shortened to the selling
    // points. Layout mirrors the sibling's About panel exactly (its #3: one
    // sentence that word-wraps naturally, drawFittedText over 4 lines, the
    // desc in the Version line's textDim, and the copyright ANCHORED to the
    // panel bottom BELOW the hyperlink — the flow-placed copyright this
    // replaces had inverted the sibling's order and opened a 14 px gap).
    const auto copyright = juce::String::charToString ((juce::juce_wchar) 0x00A9);  // © , not mojibake
    const auto emdash    = juce::String::charToString ((juce::juce_wchar) 0x2014);

    auto r = panel.reduced (30, 26);
    g.setColour (colours::text);
    g.setFont (juce::Font (juce::FontOptions (28.0f)).withExtraKerningFactor (0.16f));
    g.drawText ("ANABASIS", r.removeFromTop (38), juce::Justification::topLeft);

    // The sibling's subtitle tracking (12 pt / 0.22), so the two About
    // panels read as one family document with different names on it.
    g.setColour (colours::accent);
    g.setFont (juce::Font (juce::FontOptions (12.0f)).withExtraKerningFactor (0.22f));
    g.drawText ("MASTERING MAXIMIZER", r.removeFromTop (20), juce::Justification::topLeft);

    r.removeFromTop (14);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawText (juce::String ("Version ") + ANABASIS_VERSION_STRING
                    + "   build " + ANABASIS_BUILD_NUMBER,
                r.removeFromTop (18), juce::Justification::topLeft);
    g.drawText ("RollyTech", r.removeFromTop (18), juce::Justification::topLeft);

    r.removeFromTop (10);
    // One flowing sentence that word-wraps naturally (the sibling's #3) —
    // the family sentence-shape: what it is, the selling points, the
    // honest-monitoring hook after the dash. Shortened for 0.1.1 (owner:
    // concise, selling points only) and drawn in the SAME textDim as the
    // Version line above — the sibling never switches colour here, and the
    // bright `colours::text` this replaces made the paragraph shout over
    // the title. ⊕ owner-review wording.
    // "hold the ceiling", NOT "hold a true-peak ceiling": with `truePeakMode`
    // off — the shipped default since ADR-0015 — the clamp decides on the
    // sample peak, so the stronger phrasing was the same inter-sample
    // over-claim that ADR removed from the Ceiling readout one panel over.
    const juce::String desc =
        "A mastering loudness maximizer: an adaptive chain, a held ceiling, "
        "loudness-matched compare " + emdash + " louder, never just louder-sounding.";
    g.drawFittedText (desc, r.removeFromTop (60), juce::Justification::topLeft, 4);

    // Copyright anchored to the PANEL bottom, not the flow — the sibling's
    // geometry verbatim: the link band `resized()` places at bottom−50…−30
    // sits directly ABOVE this 16 px strip drawn bottomLeft at bottom−34…−18,
    // so the two read as one block with no gap.
    g.setColour (colours::textDim.withAlpha (0.7f));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (copyright + " 2026 RollyTech. All rights reserved.",
                panel.reduced (30, 18).removeFromBottom (16), juce::Justification::bottomLeft);
}

void AnabasisAudioProcessorEditor::ABControl::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const float rad = b.getHeight() * 0.5f;

    g.setColour (colours::accent.withAlpha (0.10f));
    g.fillRoundedRectangle (b.expanded (1.6f), rad + 1.6f);

    const float hovA = animOr (*this, "hovA", hovered);
    const float lift = 0.06f + 0.10f * hovA;
    juce::ColourGradient gr (colours::bgRaised.brighter (lift), b.getX(), b.getY(),
                             colours::bgRaised.darker (0.12f),  b.getX(), b.getBottom(), false);
    g.setGradientFill (gr);
    g.fillRoundedRectangle (b, rad);
    g.setColour (colours::outline.brighter (0.12f)
                     .interpolatedWith (colours::accent.withAlpha (0.6f), hovA));
    g.drawRoundedRectangle (b, rad, 1.0f);

    const int active = getActive ? getActive() : 0;
    auto inner = getLocalBounds();
    g.setFont (juce::Font (juce::FontOptions (14.0f)).withExtraKerningFactor (0.04f));

    auto aRect = inner.removeFromLeft (juce::roundToInt (inner.getWidth() * 0.40f));
    auto bRect = inner.removeFromRight (juce::roundToInt (getWidth() * 0.40f));

    g.setColour (colours::textDim.withAlpha (0.7f));
    g.drawText ("/", inner, juce::Justification::centred);
    g.setColour (active == 0 ? colours::accent : colours::textDim);
    g.drawText ("A", aRect, juce::Justification::centredRight);
    g.setColour (active == 1 ? colours::accent : colours::textDim);
    g.drawText ("B", bRect, juce::Justification::centredLeft);
}

// ============================================================================
//  Construction
// ============================================================================
AnabasisAudioProcessorEditor::AnabasisAudioProcessorEditor (AnabasisAudioProcessor& p)
    : juce::AudioProcessorEditor (p), processor (p)
{
    // `animVBlank` is deliberately NOT in the initialiser list — see the end of
    // this constructor.
    setLookAndFeel (&lnf);
    tooltips.setLookAndFeel (&lnf);   // desktop window: nothing to inherit from — see the member
   #if JUCE_MAC
    // ADAPTED from Anamorph `src/PluginEditor.cpp` (ADR-0009), and owed the
    // moment the line above made `drawTooltip` reachable at all: JUCE's
    // TooltipWindow declares itself OPAQUE (its constructor calls
    // `setOpaque (true)`) while the family's `drawTooltip` deliberately leaves
    // the pixels OUTSIDE the rounded capsule unpainted — undefined pixels in a
    // window that promised to fill its bounds. Apple-Silicon-native AppKit
    // initialises the opaque layer-backed window with its background colour
    // first, so those corners render as an opaque white rectangle around the
    // tooltip. Non-opaque makes the peer create a transparent NSWindow and
    // clear the backing to alpha-0 every paint. macOS-gated: uncomposited X11
    // keeps the corner PRE-FILL `drawTooltip` already does for it, which is the
    // same undefined-pixel class solved the other way round.
    tooltips.setOpaque (false);
   #endif
    setOpaque (true);

    auto& apvts = processor.apvts;
    auto paramName = [&apvts] (const char* id) -> juce::String
    {
        if (auto* pr = apvts.getParameter (id))
            return pr->getName (24);
        return {};
    };

    // -- top bar ------------------------------------------------------------
    titleButton.setButtonText ({});
    titleButton.setComponentID ("ghost");
    titleButton.onClick = [this] { showAbout (true); };
    addAndMakeVisible (titleButton);

    abControl.setTooltip ("A/B Compare");   // the sibling's exact wording (no period)
    abControl.getActive = [this] { return processor.activeSlotIndex(); };
    abControl.onToggle  = [this]
    {
        processor.switchToSlot (1 - processor.activeSlotIndex());
        abControl.repaint();
        refreshPresetDisplay (true);
    };
    addAndMakeVisible (abControl);
    registerAnimated (abControl);

    copyButton.setTooltip (tidyTip ("Copy the current settings into the other A/B slot."));
    copyButton.onClick = [this] { processor.copySlotToOther(); };
    addAndMakeVisible (copyButton);
    registerAnimated (copyButton);

    undoButton.setButtonText (juce::String::charToString ((juce::juce_wchar) 0x21BA)); // the sibling's circle arrow (#7)
    redoButton.setButtonText (juce::String::charToString ((juce::juce_wchar) 0x21BB));
    // THE ARMING SITE for the LookAndFeel's `"icon"` treatment, missing since
    // these buttons were added at P6. `drawButtonText`/`getTextButtonFont`
    // discriminate on this id to render a 21 px glyph instead of the generic
    // path's ~13 px, and the branch's own header names its owner: "Undo/Redo
    // glyphs". Unarmed, a 30 px glyph button drew a 13 px character — the
    // opposite of the sizing the branch exists to provide.
    //
    // Wired rather than deleted, unlike the three sibling ids removed beside
    // it, because the discriminator is between two kinds of button IN THIS
    // top bar (the glyph pair against `Copy`/`Settings`/`Save`), not a
    // description of a control this product lacks.
    //
    // ONE THING FOR THE BRAND PASS, flagged rather than decided here: the
    // treatment also rotates the glyph 180°, specified against the sibling's
    // U+21BA/U+21BB OPEN CIRCLE arrows, where a half-turn of a near-circular
    // shape is a subtle comfort tweak. These are U+21B6/U+21B7 SEMICIRCLE
    // arrows, so the same half-turn visibly moves the arc from the top to the
    // bottom. Direction is not at risk — a rotation preserves the curl, so
    // neither glyph can come to read as the other — but which glyph pair this
    // product ships is a Level-5 question the brand checklist still holds open,
    // and it is not one a cleanup pass should answer.
    undoButton.setComponentID ("icon");
    redoButton.setComponentID ("icon");
    undoButton.setTooltip ("Undo");
    redoButton.setTooltip ("Redo");
    undoButton.onClick = [this] { processor.undo(); refreshPresetDisplay (true); };
    redoButton.onClick = [this] { processor.redo(); refreshPresetDisplay (true); };
    addAndMakeVisible (undoButton);
    addAndMakeVisible (redoButton);
    registerAnimated (undoButton);
    registerAnimated (redoButton);

    settingsButton.onClick = [this] { showSettings (true); };
    addAndMakeVisible (settingsButton);
    registerAnimated (settingsButton);

    setupToggle (advancedToggle, pid::advancedMode, "ADV", tipFor (pid::advancedMode));
    setupToggle (bypassToggle, pid::bypass, "BYPASS", tipFor (pid::bypass));
    bypassToggle.setComponentID ("bypass");   // red-pill LookAndFeel variant

    presetPrev.setButtonText (juce::String::charToString ((juce::juce_wchar) 0x2039));
    presetNext.setButtonText (juce::String::charToString ((juce::juce_wchar) 0x203A));
    presetPrev.setComponentID ("presetnav");
    presetNext.setComponentID ("presetnav");
    presetName.setComponentID ("presetname");
    auto stepPreset = [this] (int dir)
    {
        // One ordered list: FACTORY first, then the user files — ‹ › walks it
        // as a ring, matching the menu's presentation.
        int factoryCount = 0;
        const auto* factory = PresetManager::factoryPresets (factoryCount);
        auto files = PresetManager::userPresetDirectory()
                         .findChildFiles (juce::File::findFiles, false, "*.anabasis");
        files.sort();
        const int total = factoryCount + files.size();
        if (total == 0)
            return;
        // WHERE ARE WE? The IDENTITY answers (ADR-0022): the wrapper records
        // which row produced the current sound — a factory id or a user file,
        // two namespaces that cannot collide — and it travels with undo, A/B
        // and the session, so it is right after every path that changes what
        // is showing. A known identity that is on no row (an outside-folder
        // file, a preset deleted on disk) answers -1 and the arrows start
        // from the list edge; the resolver's name fallback covers only state
        // that carries no identity at all. This replaced the editor-local
        // "remembered source" hint, which died with the window and needed the
        // NAME — the very thing that fails on a clash — to confirm it.
        const int here = PresetManager::selectedPresetRow (processor.currentPresetSelection(),
                                                           processor.currentPresetName(),
                                                           factory, factoryCount, files);
        // ...and the ring KEEPS WALKING past an entry it cannot apply, which
        // is a requirement of resolving the position from the identity rather
        // than a refinement of it. The identity is set only by a SUCCESSFUL
        // apply (`applyPresetFile` returns before it on an unreadable file),
        // so a single-shot step would re-derive the same starting row on the
        // next press and offer the same broken file for ever: one corrupt
        // `.anabasis` in the folder would wall the arrows off in that
        // direction permanently. The editor-local hint this replaced was
        // advanced UNCONDITIONALLY for exactly this reason; the loop carries
        // that rationale forward, and lands on the next preset that actually
        // loads instead of merely stepping over the broken one.
        //
        // Bounded by `total`, so the pass visits every entry at most once and
        // terminates. The last candidate that bound admits is the row the
        // press STARTED from, and re-applying it would mint an undo step for
        // a press that moved nothing — UNREACHABLE rather than tolerated,
        // which is worth writing down because it reads like a live edge case.
        // The walk only reaches it if every OTHER entry failed first, and it
        // cannot: the factory rows are a CONTIGUOUS PREFIX of the ring and a
        // factory apply cannot fail (`applyFactoryPreset` returns false only
        // for an out-of-range index, and both its range checks pass for one
        // this loop produces). So the longest run of failable candidates is
        // the user block — `files.size()` of them, NOT `factoryCount`, which
        // is the wrong way round and was written here once — and any walk
        // meets a factory row by step `files.size()`, which is short of the
        // `total - 1` step where the starting row sits whenever there are at
        // least TWO factory rows. A folder of nothing but unreadable USER
        // files therefore lands on a FACTORY preset: not a no-op, and not
        // back where it started.
        //
        // Both halves of that are premises, not laws. A single-row factory
        // bank would put the sole applyable row under the press itself and
        // re-apply it; a factory source that can fail removes the argument
        // outright. `testTheRingWalksPastAnUnreadablePreset` pins the first
        // (`factoryCount >= 2`); if either changes, this bound needs the
        // starting row excluded explicitly. Note the scope: the claim is that
        // the WALK cannot wrap onto its own starting row, not that a press
        // can never re-land the sound it started with — an identity the
        // resolver cannot place (-1) starts from the list edge by ADR-0022
        // §Decision 3, and that edge may hold the same preset under a name it
        // no longer matches.
        //
        // Retrying costs nothing either. Both reachable failures — a file
        // that vanished between the scan above and the press, and
        // `parsePresetFile` refusing a corrupt or foreign one — return BEFORE
        // the undo bracket and the §2.8 duck, and the post-bracket failure is
        // unreachable (`applyPreset` re-tests the root tag `parsePresetFile`
        // already enforced). So one press still mints exactly one undo step,
        // the one for the preset that landed.
        int idx = (here < 0 ? (dir > 0 ? 0 : total - 1) : (here + dir + total) % total);
        for (int tried = 0; tried < total; ++tried)
        {
            const bool applied = idx < factoryCount
                                     ? processor.applyFactoryPreset (idx)
                                     : processor.applyPresetFile (files.getReference (idx - factoryCount));
            if (applied)
                break;
            idx = (idx + dir + total) % total;
        }
        refreshPresetDisplay (true);
    };
    presetPrev.setTooltip ("Previous preset");
    presetNext.setTooltip ("Next preset");
    presetName.setTooltip ("Presets");   // short, no period — the sibling's rule
    presetPrev.onClick = [stepPreset] { stepPreset (-1); };
    presetNext.onClick = [stepPreset] { stepPreset (+1); };
    presetName.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (presetPrev);
    addAndMakeVisible (presetNext);
    addAndMakeVisible (presetName);
    // Hand-built, so they miss the setup helpers' `registerAnimated` tail call
    // — and the LookAndFeel's hover branch reads the animated value, falling
    // back to a binary is-the-mouse-over test without it. These three and the
    // two save buttons below were the only controls in the editor still
    // stepping between hover states while everything around them eased
    // (0.1.1 migration audit; the sibling registers exactly this set).
    for (auto* b : { (juce::Component*) &presetPrev, (juce::Component*) &presetNext,
                     (juce::Component*) &presetName })
        registerAnimated (*b);

    // -- COMP panel ----------------------------------------------------------
    // EDITOR captions vs AUTOMATION names (0.1.2 items 8–10): inside a
    // captioned panel the stage prefix is noise — the panel already says
    // COMP or LIMITER — so the knob captions drop it ("Ratio", "Threshold",
    // "Gain", "Release", both modules' links read "Stereo Link"). The
    // REGISTRY names keep the prefix: an automation lane floats free of any
    // panel, so there the prefix is the disambiguation ("Comp Ratio",
    // "Limiter Gain", "Comp Stereo Link" vs "Limiter Stereo Link").
    // Accessibility deliberately follows the REGISTRY name (`setupRotary`'s
    // title/description): a screen reader announces controls in a flat list,
    // the same no-panel context an automation lane has (brief §8's "the same
    // wording the automation lane shows" survives this split unchanged).
    // IDs are untouched — this table is display only.
    auto displayName = [&paramName] (const char* id) -> juce::String
    {
        static const std::pair<const char*, const char*> overrides[] = {
            { pid::compRatio,      "Ratio" },     { pid::compThreshold, "Threshold" },
            { pid::compAttack,     "Attack" },    { pid::compRelease,   "Release" },
            { pid::compKnee,       "Knee" },      { pid::compMix,       "Mix" },
            { pid::compStereoLink, "Stereo Link" },
            { pid::limGain,        "Gain" },      { pid::limRelease,    "Release" },
            { pid::stereoLink,     "Stereo Link" },
        };
        for (const auto& [pid_, caption] : overrides)
            if (std::strcmp (pid_, id) == 0)
                return caption;
        return paramName (id);
    };
    auto rotary = [this, &paramName, &displayName] (Knob& k, juce::Label& l, const char* id)
    {
        setupRotary (k, l, displayName (id), tipFor (id), paramName (id));
        attachSlider (k, id);
    };
    rotary (ratioK, ratioL, pid::compRatio);
    rotary (thresholdK, thresholdL, pid::compThreshold);
    rotary (attackK, attackL, pid::compAttack);
    rotary (releaseK, releaseL, pid::compRelease);
    rotary (kneeK, kneeL, pid::compKnee);
    rotary (compMixK, compMixL, pid::compMix);
    rotary (compLinkK, compLinkL, pid::compStereoLink);   // ADR-0019 (0.1.1)
    setupToggle (compAutoToggle, pid::compAutoRelease, "AUTO", tipFor (pid::compAutoRelease));
    setupCombo (detectorBox, pid::compDetector, tipFor (pid::compDetector));

    // -- CLIP / COLOUR panel -------------------------------------------------
    rotary (shapeK, shapeL, pid::clipShape);
    rotary (driveK, driveL, pid::clipDrive);
    rotary (clipMixK, clipMixL, pid::clipMix);
    rotary (balanceK, balanceL, pid::colourBalance);
    rotary (colToneK, colToneL, pid::colourTone);
    rotary (depthK, depthL, pid::colourDepth);
    rotary (dynTiltK, dynTiltL, pid::dynTilt);
    setupCombo (modelBox, pid::colourModel, tipFor (pid::colourModel));

    // -- LIMITER panel -------------------------------------------------------
    rotary (limGainK, limGainL, pid::limGain);
    rotary (ceilingK, ceilingL, pid::ceiling);
    rotary (lookaheadK, lookaheadL, pid::lookahead);
    rotary (limReleaseK, limReleaseL, pid::limRelease);
    rotary (linkK, linkL, pid::stereoLink);
    rotary (preserveK, preserveL, pid::transientPreserve);
    setupToggle (limAutoToggle, pid::limAutoRelease, "AUTO", tipFor (pid::limAutoRelease));
    setupToggle (tpToggle, pid::truePeakMode, "TP", tipFor (pid::truePeakMode));
    setupCombo (styleBox, pid::limStyle, tipFor (pid::limStyle));

    // -- EQ panel ------------------------------------------------------------
    rotary (eqTiltK, eqTiltL, pid::eqTilt);
    rotary (lsFreqK, lsFreqL, pid::eqLowShelfFreq);
    rotary (lsGainK, lsGainL, pid::eqLowShelfGain);
    rotary (hsFreqK, hsFreqL, pid::eqHighShelfFreq);
    rotary (hsGainK, hsGainL, pid::eqHighShelfGain);
    rotary (b1FreqK, b1FreqL, pid::eqBell1Freq);
    rotary (b1GainK, b1GainL, pid::eqBell1Gain);
    rotary (b1QK, b1QL, pid::eqBell1Q);
    rotary (b2FreqK, b2FreqL, pid::eqBell2Freq);
    rotary (b2GainK, b2GainL, pid::eqBell2Gain);
    rotary (b2QK, b2QL, pid::eqBell2Q);
    setupCombo (eqPosBox, pid::eqPosition, tipFor (pid::eqPosition));

    // -- utility row ---------------------------------------------------------
    rotary (inputGainK, inputGainL, pid::inputGain);
    rotary (scHpfK, scHpfL, pid::scHpfFreq);
    // R2 item 2: these two ride the utility STRIP, where a rotary renders as
    // the smallest knob on the page — a horizontal fader (the family's
    // `drawLinearSlider` language) fits the row's shape instead. Everything
    // else `setupRotary`/`attachSlider` wired — attachment, double-click
    // reset, value box, tooltip, title — is presentation-independent and
    // stays; only the slider style changes, and the value box moves beside
    // the track (a below-track box would eat the strip's height).
    for (auto* s : { &inputGainK, &scHpfK })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 14);
    }
    setupCombo (ditherBox, pid::dither, tipFor (pid::dither));
    ditherCaption.setText ("Dither", juce::dontSendNotification);
    ditherCaption.setJustificationType (juce::Justification::centred);
    ditherCaption.setColour (juce::Label::textColourId, colours::textDim);
    ditherCaption.setFont (juce::Font (juce::FontOptions (11.5f)));
    addAndMakeVisible (ditherCaption);
    setupToggle (shapingToggle, pid::ditherShaping, "SHAPE", tipFor (pid::ditherShaping));
    setupToggle (compToggle, pid::loudnessComp, "COMP", tipFor (pid::loudnessComp));
    setupToggle (deltaToggle, pid::deltaMonitor, "DELTA", tipFor (pid::deltaMonitor));
    setupToggle (freezeToggle, pid::freeze, "FREEZE", tipFor (pid::freeze));
    setupToggle (tpSimpleToggle, pid::truePeakMode, "TP", tipFor (pid::truePeakMode));

    // The Advanced macro row (read-only Loudness/Character/Tone mirrors,
    // §6.3) was REMOVED at 0.1.2 (item 11, ADR-0023): three non-adjustable
    // knobs bought 78 px of the window and answered no question the detach
    // badges (paint()) and the Simple view do not. The macros themselves are
    // untouched — mapping, detach and re-engage all live below the view
    // layer — and the Advanced height dropped to kAdvancedH (822).

    // -- Simple view (§6.2) --------------------------------------------------
    rotary (bigLoudnessK, bigLoudnessL, pid::loudness);
    rotary (simpleCharacterK, simpleCharacterL, pid::character);
    rotary (simpleToneK, simpleToneL, pid::tone);
    rotary (simpleCeilingK, simpleCeilingL, pid::ceiling);
    bigLoudnessL.setFont (juce::Font (juce::FontOptions (13.5f)).withExtraKerningFactor (0.2f));

    setupToggleInternal (ceilingLockToggle, "LOCK", "Ceiling lock",
                         "Keep the Ceiling where it is while you browse presets",
                         processor.internalState.state()
                             .getPropertyAsValue (iid::ceilingLock, nullptr));

    // §5.4 Learn — explicit start / explicit end, never background (MODE
    // inv 3). The button ignores a stop inside the MINIMUM PASS (the ~1.5 s
    // integrated features carry pre-start material, so an instant stop would
    // commit a biased reference — the P4-recorded grammar debt); while
    // learning it counts the remaining seconds down, wordlessly. An EMPTY
    // pass (nothing above the silence gate) flashes `warn`: the reference
    // did not move, which is the readout P4 owed the caller.
    learnButton.setClickingTogglesState (false);
    learnButton.onClick = [this]
    {
        const auto& a = processor.adaptiveReadout();
        const double nowMs = juce::Time::getMillisecondCounterHiRes();
        if (! a.isLearning())
        {
            processor.startLearn();
            learnStartedMs = nowMs;
            learnStopPending = false;
        }
        else if (nowMs - learnStartedMs >= kLearnMinPassMs)
        {
            hadLearnedAtStop = a.hasLearned();
            refOnsetAtStop   = a.publishedRefOnset();
            refTiltAtStop    = a.publishedRefTilt();
            processor.stopLearn();
            learnStopPending = true;
        }
    };
    learnButton.setTooltip (tidyTip (
        "Play the loudest section and Learn measures it as the adaptive reference - click again to stop"));
    addAndMakeVisible (learnButton);
    registerAnimated (learnButton);

    outLufsCaption.setText ("out LUFS", juce::dontSendNotification);   // §6.2 wireframe
    outLufsCaption.setColour (juce::Label::textColourId, colours::textDim);
    outLufsCaption.setFont (juce::Font (juce::FontOptions (11.5f)));
    outLufsCaption.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (outLufsCaption);
    outLufsValue.setColour (juce::Label::textColourId, colours::text);
    outLufsValue.setFont (juce::Font (juce::FontOptions (15.0f)));
    outLufsValue.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (outLufsValue);

    editedDot.setTooltip (tidyTip (
        "Advanced edits took knobs off the macros - click to return to the macro sound"));
    editedDot.onClick = [this] { processor.resetToMacro(); };
    addChildComponent (editedDot);

    meterView    = std::make_unique<LoudnessMeterView> (processor);
    grView       = std::make_unique<GrHistoryView> (processor);
    spectrumView = std::make_unique<SpectrumView> (processor);
    clipCurve = std::make_unique<CurveView> (processor, CurveView::Mode::clipTransfer);
    eqCurve   = std::make_unique<CurveView> (processor, CurveView::Mode::eqResponse);
    addChildComponent (*clipCurve);
    addChildComponent (*eqCurve);
    addChildComponent (compGrMeter);
    addChildComponent (limGrMeter);
    addAndMakeVisible (*meterView);
    // The two modes of the shared graph well (both views, both editor modes) —
    // `int_spectrumOn` picks one; every layout pass and the 24 Hz tick keep the
    // visibility pair in step, starting with the first `resized()`.
    addChildComponent (*grView);
    addChildComponent (*spectrumView);

    // -- overlays ------------------------------------------------------------
    addChildComponent (dimOverlay);
    dimOverlay.setInterceptsMouseClicks (false, false);
    dimOverlay.setAlwaysOnTop (true);

    aboutBackdrop.aboutText = true;
    aboutBackdrop.onDismiss = [this] { showAbout (false); };
    addChildComponent (aboutBackdrop);
    aboutBackdrop.setAlwaysOnTop (true);
    addChildComponent (aboutLink);
    aboutLink.setAlwaysOnTop (true);
    // The sibling's link styling, exactly: accent colour, 13 pt, NO underline
    // (the second argument), left-justified, and no tooltip on the URL (#2).
    aboutLink.setColour (juce::HyperlinkButton::textColourId, colours::accent);
    aboutLink.setFont (juce::Font (juce::FontOptions (13.0f)), false, juce::Justification::centredLeft);
    aboutLink.setJustificationType (juce::Justification::centredLeft);
    aboutLink.setTooltip (juce::String());

    settingsBackdrop.dropShadow = true;
    settingsBackdrop.onDismiss = [this] { showSettings (false); };
    addChildComponent (settingsBackdrop);
    settingsBackdrop.setAlwaysOnTop (true);

    savePresetBackdrop.dropShadow = true;
    savePresetBackdrop.onDismiss = [this] { showSavePreset (false); };
    addChildComponent (savePresetBackdrop);
    savePresetBackdrop.setAlwaysOnTop (true);

    // -- Settings rows (§6.4, all InternalState-bound) -----------------------
    auto& ist = processor.internalState.state();
    settingsTitle.setText ("SETTINGS", juce::dontSendNotification);
    settingsTitle.setFont (juce::Font (juce::FontOptions (13.0f)).withExtraKerningFactor (0.18f));
    settingsTitle.setColour (juce::Label::textColourId, colours::textDim);
    settingsBackdrop.addAndMakeVisible (settingsTitle);

    auto settingsRow = [this] (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setColour (juce::Label::textColourId, colours::text);
        l.setFont (juce::Font (juce::FontOptions (13.0f)));
        settingsBackdrop.addAndMakeVisible (l);
    };
    settingsRow (oversampleLabel, "Oversampling");
    settingsRow (phaseLabel,      "Phase");
    settingsRow (offlineLabel,    "Offline Render");
    settingsRow (integratedLabel, "Integrated");
    settingsRow (rmsRefLabel,     "RMS Reference");
    settingsRow (uiScaleLabel,    "UI Scale");

    setupComboInternal (oversampleBox, { "Off", "2x", "4x", "8x", "16x" },
                        "Oversampling",
                        "Cleaner nonlinear stages and true-peak accuracy - higher costs CPU and adds latency",
                        ist.getPropertyAsValue (iid::oversample, nullptr));
    // DESIGN §6.4 specifies the latency note lives in this control's tooltip.
    setupComboInternal (phaseBox, { "Minimum", "Linear" },
                        "Phase",
                        "Minimum keeps latency lowest - Linear rings symmetrically and adds latency (reported to the host)",
                        ist.getPropertyAsValue (iid::osPhase, nullptr));
    setupComboInternal (offlineBox, { "Follow Online", "Force Max" },
                        "Offline Render",
                        "Force Max bounces at maximum oversampling - Follow Online uses the live setting",
                        ist.getPropertyAsValue (iid::offlineQuality, nullptr));
    // ADR-0020: which STANDARD two of the statistics rows follow. Both are
    // display-side only — the processor publishes every reading and this
    // choice selects among them, so neither box touches the audio thread.
    // The item text names the revision rather than "gated"/"ungated" because
    // a delivery spec is written as a revision number.
    setupComboInternal (integratedBox, { "BS.1770-2+", "BS.1770-1" },
                        "Integrated",
                        "Which revision the INTEGRATED row follows - -2 onward gates quiet passages out, -1 does not",
                        ist.getPropertyAsValue (iid::integratedStd, nullptr));
    setupComboInternal (rmsRefBox, { "AES-17", "Mathematical" },
                        "RMS Reference",
                        "AES-17 reads a full-scale sine as 0 dBFS - Mathematical reads it as -3.01",
                        ist.getPropertyAsValue (iid::rmsRef, nullptr));
    // int_uiScale stores PERCENT; the combo maps index<->percent through the
    // step list, so the stored value stays meaningful outside this editor.
    // Built FROM `kScaleSteps`, not written out beside it. The literal list
    // that used to live here was the same seven values a second time: they
    // agreed, but adding or removing a step in the ladder without editing this
    // line would have left the displayed label describing a different scale
    // from the applied transform — the exact second-copy shape this file
    // removed for the meter target table and the badge ids.
    {
        // XS..XL — the sibling's display (owner directive 2026-08-05); the
        // stored value stays a percent, `ui_scale::names` is index-locked to
        // `steps` by its static_assert, so label and transform still cannot
        // disagree.
        juce::StringArray items;
        for (int i = 0; i < kNumScaleSteps; ++i)
            items.add (ui_scale::names[i]);
        uiScaleBox.addItemList (items, 1);
    }
    // Through `normalisedUiScale()` like every other read of this property, so
    // the initial seed converges an illegal stored percent too. `applyUiScale()`
    // at the end of the constructor would reach it anyway; going through the one
    // reader means no future ordering change can quietly take that away.
    uiScaleBox.setSelectedItemIndex (nearestScaleIndex (normalisedUiScale()),
                                     juce::dontSendNotification);
    // `[this]`, NOT `[this, &ist]`. `ist` is a local REFERENCE VARIABLE whose
    // lifetime ends when this constructor returns, and capturing a reference
    // variable by reference captures the variable, not the referent
    // ([expr.prim.lambda.capture]) — so invoking the closure afterwards is UB by
    // the letter of the standard, even though every compiler resolves it
    // through to the long-lived `InternalState` tree and it has always worked.
    // Re-fetching inside is the form the §6.4 toggle callback below already
    // uses; the two now agree, so a reader does not have to re-derive why one
    // of them is benign.
    uiScaleBox.onChange = [this]
    {
        processor.internalState.state().setProperty (iid::uiScale,
                         kScaleSteps[juce::jlimit (0, kNumScaleSteps - 1,
                                                   uiScaleBox.getSelectedItemIndex())],
                         nullptr);
        applyUiScale();
    };
    passComboHoverThrough (uiScaleBox);
    allCombos.add (&uiScaleBox);
    settingsBackdrop.addAndMakeVisible (uiScaleBox);
    // Hand-built rather than via setupComboInternal (it maps index↔percent, not
    // index↔value), which is exactly how it came to miss both of that helper's
    // tail calls: without registerAnimated its hover lift skipped the easing
    // every other combo has, and without a title it had no accessibility name.
    registerAnimated (uiScaleBox);
    uiScaleBox.setTitle ("UI Scale");
    uiScaleBox.setTooltip (tidyTip ("Window size - M is the original"));
    for (auto* b : { &oversampleBox, &phaseBox, &offlineBox, &integratedBox, &rmsRefBox })
    {
        removeChildComponent (b);
        settingsBackdrop.addAndMakeVisible (b);
    }

    // These four are bound by `getToggleStateValue().referTo (…)` inside
    // `setupToggleInternal` — a bool↔bool binding `juce::Value` CAN express, so
    // they follow a project load by themselves and are deliberately NOT in
    // `refreshInternalSettingsBoxes()`. The panel therefore has two refresh
    // mechanisms, which is worth naming rather than leaving to be discovered:
    // the re-seeded controls are the ones whose widget state is not the stored
    // value (index↔value for the combos, index↔percent for `uiScaleBox`, one
    // BIT of an int for each target checkbox). The trade is testability —
    // `juce::Value` delivers its change asynchronously through the message
    // loop, which the headless suite does not run, so
    // `testTheSettingsPanelFollowsAProjectLoad` covers the re-seeded half only.
    setupToggleInternal (animToggle, "UI Animations", "UI Animations",
                         "Smooth micro-animations on hovers, presses and switches",
                         ist.getPropertyAsValue (iid::uiAnimations, nullptr));
    setupToggleInternal (tooltipsToggle, "Tooltips", "Tooltips",
                         "Show these hover hints on every control",
                         ist.getPropertyAsValue (iid::tooltipsOn, nullptr));
    // The True-Peak Meter toggle left Settings in 0.1.1 with the field behind
    // it (ADR-0020): the statistics panel shows the true peak unconditionally,
    // beside the sample peak, so there was no row left to hide. Its slot is
    // taken by the two STANDARD selectors above, which choose how a shown
    // reading is computed rather than whether it appears — the distinction
    // the removed toggle failed to draw.
    // The Spectrum toggle left Settings 2026-08-05: the graph well itself
    // carries the GR/SPEC switch now (owner directive — the control lives on
    // the thing it controls).
    // The §6.4 streaming-target checkboxes stood here until 2026-08-05, when
    // the owner removed streaming-platform analysis outright (platforms
    // normalise; a modern master is pushed against the ceiling, not a
    // platform figure). `int_meterTargets` left the schema with them — an
    // old session carrying it is ignored by the §4.4 unknown-field rule.
    for (auto* t : { &animToggle, &tooltipsToggle })
    {
        removeChildComponent (t);
        settingsBackdrop.addAndMakeVisible (t);
    }
    animToggle.onStateChange = [this]
    { uiAnimOn = animToggle.getToggleState(); };
    tooltipsToggle.onStateChange = [this] { applyTooltipsEnabled(); };

    // -- save-preset overlay -------------------------------------------------
    saveTitle.setText ("SAVE PRESET", juce::dontSendNotification);
    saveTitle.setFont (juce::Font (juce::FontOptions (13.0f)).withExtraKerningFactor (0.18f));
    saveTitle.setColour (juce::Label::textColourId, colours::textDim);
    savePresetBackdrop.addAndMakeVisible (saveTitle);
    saveNameEditor.setSelectAllWhenFocused (true);
    // THE ARMING SITE the LookAndFeel's focus-glow branch was written for, and
    // never had. `fillTextEditorBackground`/`drawTextEditorOutline` both key on
    // this property and fall through to the JUCE default without it, so the
    // rounded accent-lit border the family design language specifies (#11) was
    // unreachable and this field rendered as a stock rectangle. The property is
    // the right mechanism rather than styling every `TextEditor`: the OTHER
    // editors in this tree are the ValueBox labels' edit fields, which the
    // LookAndFeel deliberately leaves at the default outline — the branch is a
    // choice between two kinds of field in THIS editor, not migrated Anamorph
    // state, so it is wired rather than deleted.
    saveNameEditor.getProperties().set ("glow", true);
    // The family TextEditor palette. The `"glow"` property above only arms the
    // LookAndFeel's focus-ring BRANCH; the branch draws against these colour
    // ids, and without them the field rendered in JUCE's stock scheme — a
    // light box in a dark panel, the one control in the product not wearing
    // the palette. Ported from the sibling's own save field.
    saveNameEditor.setFont (juce::Font (juce::FontOptions (14.0f)));
    saveNameEditor.setColour (juce::TextEditor::backgroundColourId,     colours::bg);
    saveNameEditor.setColour (juce::TextEditor::textColourId,           colours::text);
    saveNameEditor.setColour (juce::TextEditor::outlineColourId,        colours::outline);
    saveNameEditor.setColour (juce::TextEditor::focusedOutlineColourId, colours::accent.withAlpha (0.60f));
    saveNameEditor.setColour (juce::TextEditor::highlightColourId,      colours::accent.withAlpha (0.30f));
    // RETURN COMMITS, ESCAPE DISMISSES — the sibling's two lines, and the half
    // of this overlay that was never ported. `juce::TextEditor::returnPressed`
    // posts a command whose ONLY consumers are a registered Listener and these
    // two std::functions; Anabasis registers neither, so both keys were inert
    // and the dialog could be left only with the mouse. The knob value boxes
    // never showed the defect because they are `juce::Label` editors, and
    // Label implements the two callbacks itself.
    saveNameEditor.onReturnKey = [this] { saveOkButton.triggerClick(); };
    saveNameEditor.onEscapeKey = [this] { showSavePreset (false); };
    savePresetBackdrop.addAndMakeVisible (saveNameEditor);
    for (auto* b : { (juce::Component*) &saveOkButton, (juce::Component*) &saveCancelButton })
        registerAnimated (*b);      // as the top-bar three above
    saveOkButton.onClick = [this]
    {
        const auto name = juce::File::createLegalFileName (saveNameEditor.getText().trim());
        if (name.isEmpty())
            return;
        auto dir = PresetManager::userPresetDirectory();
        dir.createDirectory();
        const auto file = dir.getChildFile (name + ".anabasis");
        if (processor.savePresetFile (file))
        {
            // A SAVE IS AN APPLY as far as "which preset is in use" is
            // concerned, and the wrapper says so itself now: `savePresetFile`
            // sets the ADR-0022 identity to the file it just wrote, so the
            // tick and the ‹ › arrows land on the USER row — even when the
            // name matches a factory preset's. Nothing for this editor to
            // record: the identity is wrapper state, shared by every window
            // and carried with the session, which is what the editor-local
            // hint that used to live here could never be.
            showSavePreset (false);
            refreshPresetDisplay (true);
        }
    };
    saveCancelButton.onClick = [this] { showSavePreset (false); };
    savePresetBackdrop.addAndMakeVisible (saveOkButton);
    savePresetBackdrop.addAndMakeVisible (saveCancelButton);

    // -- listeners / cadence -------------------------------------------------
    apvts.addParameterListener (pid::advancedMode, this);
    apvts.addParameterListener (pid::bypass, this);
    advanced   = apvts.getRawParameterValue (pid::advancedMode)->load() >= 0.5f;
    uiAnimOn   = (bool) ist.getProperty (iid::uiAnimations, true);
    tooltipsOn = (bool) ist.getProperty (iid::tooltipsOn, false);
    // …and the graph well's cached mode, from the SAME read the layout below
    // and the 24 Hz tick both use. It caches WHAT IS CURRENTLY SHOWN, so its
    // seed has to be the observed state rather than the product default —
    // the `shownTpMode` lesson (DOCUMENTATION_COVERAGE, 2026-08-06), and the
    // reason a hard-coded seed here is a defect waiting for the default to
    // move: it did move, at 0.1.2, when GR became the default view.
    shownSpectrumOn = (bool) ist.getProperty (iid::spectrumOn, false);
    applyTooltipsEnabled();

    // The Ceiling boxes already show the right unit: their attachments were
    // created above, and each rendered its text through the parameter's own
    // `getText`, i.e. through `CeilingUnitSource`. So there is nothing to
    // refresh here — only the CACHE to align with what is on screen, which is
    // why this is an assignment and not a `refreshCeilingUnit()` call (that
    // would repaint both boxes to redraw the string they already carry).
    // Reading the same predicate the suffix came from is what makes
    // "shownTpMode == the mode the visible suffix reflects" true by
    // construction rather than by coincidence — including in the unwired-holder
    // case, where both say off.
    //
    // BEFORE `startTimerHz`, deliberately: the tick is the one reader of this
    // member, and seeding after arming it would make correctness depend on the
    // message loop not running mid-constructor. It cannot — but that is the
    // "safe by ordering" argument this file declines elsewhere, and the fix
    // costs one line's placement.
    shownTpMode = processor.ceilingUnit.truePeakEngaged();

    startTimerHz (24);
    seedAnimatedFromValues();          // after every attachment — see there
    refreshPresetDisplay (true);
    // …and the third display datum that was waiting for a tick to become true.
    // `undoButton`/`redoButton` are constructed enabled, so an editor opened on
    // an empty history rendered both live for the first ~42 ms. Harmless to
    // click (`undo()` returns on an empty stack) and purely cosmetic — but it
    // is the same class as the preset mark and the animated knob positions,
    // both of which are now seeded here rather than discovered a frame later,
    // and the seeding shares the timer's function rather than repeating its
    // two comparisons.
    refreshUndoRedoEnablement();
    updateModeVisibility();
    applyUiScale();
    // …and lay out explicitly. `applyUiScale()` reaches `setSize`, but
    // `setSize` is a NO-OP when the dimensions are unchanged, so the layout is
    // not guaranteed by that call alone — see the guard at the top of
    // `resized()`. Idempotent, and it costs one layout pass at construction.
    resized();

#if JUCE_MAC || JUCE_WINDOWS
    glContext.attachTo (*this);    // §6.1: GPU compositing on these two only
#endif

    // THE ANIMATION CLOCK STARTS LAST, and the placement is the point. It was
    // constructed in the member-initialiser list, where it ran BEFORE
    // `lastFrameTime` and `uiAnimOn` — both declared after it in the header, so
    // both still uninitialised at that instant — and its callback
    // (`stepMicroAnims`) reads all three of those and `animated`, which the
    // `registerAnimated` calls above fill. Unreachable today, because vblank
    // callbacks are delivered on the message thread and cannot preempt a
    // constructor running there; but that is the same platform-dispatch
    // argument the destructor already refuses to rely on one screen down
    // (`animVBlank = {}` runs FIRST there, so the callback cannot outlive the
    // state it reads). The two ends of the lifetime now say the same thing.
    animVBlank = juce::VBlankAttachment (this, [this]
    {
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const double dt  = juce::jlimit (0.0, 0.05, now - lastFrameTime);
        lastFrameTime = now;
        stepMicroAnims (dt);
    });
}

AnabasisAudioProcessorEditor::~AnabasisAudioProcessorEditor()
{
    // FIRST, for the reason `~AnabasisAudioProcessor` calls `stopDraining()`
    // first and for the reason `startDraining()` exists at all: both base
    // classes stop themselves in THEIR destructors, which run AFTER every
    // member here is gone, so `timerCallback` and `handleAsyncUpdate` — which
    // touch `meterView`, `animated`, the attachments — are quiet only because
    // the message thread is the one executing this destructor. That is "safe
    // by ordering", the argument this PR stopped relying on everywhere else;
    // say it instead.
    stopTimer();
    cancelPendingUpdate();
    // The THIRD source, and leaving it out made the guarantee above two thirds
    // true: `animVBlank`'s callback (`stepMicroAnims`) touches `animated`,
    // `uiAnimOn` and `lastFrameTime`. It is declared BEFORE `lastFrameTime` and
    // `uiAnimOn`, so reverse-order destruction frees those two FIRST and leaves
    // the attachment armed over them — the direction an earlier revision of
    // this comment had backwards. Move-assigning an empty one detaches here
    // instead. Same reasoning, applied to every source rather than to the two
    // that came to mind.
    animVBlank = {};

#if JUCE_MAC || JUCE_WINDOWS
    glContext.detach();
#endif
    processor.apvts.removeParameterListener (pid::advancedMode, this);
    processor.apvts.removeParameterListener (pid::bypass, this);
    tooltips.setLookAndFeel (nullptr);   // before `lnf` dies — paired with the ctor
    setLookAndFeel (nullptr);
}

// ============================================================================
//  Setup helpers (family grammar — provenance in the header)
// ============================================================================
void AnabasisAudioProcessorEditor::setupRotary (juce::Slider& s, juce::Label& l,
                                                const juce::String& name, const juce::String& tip,
                                                const juce::String& accessibleName)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setColour (juce::Slider::textBoxTextColourId, colours::text);
    s.setColour (juce::Slider::textBoxHighlightColourId, colours::accent.withAlpha (0.30f));
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setTooltip (tidyTip (tip));
    s.setRepaintsOnMouseActivity (true);
    addAndMakeVisible (s);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 14);

    l.setText (name, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setColour (juce::Label::textColourId, colours::textDim);
    l.setFont (juce::Font (juce::FontOptions (11.5f)));
    addAndMakeVisible (l);
    registerAnimated (s);
    // Accessibility (brief §8, the deliberate delta from Anamorph): every
    // control carries its REGISTRY name as title/description, so a screen
    // reader announces the same wording the automation lane shows. Since the
    // 0.1.2 caption/name split (items 8–10) the visible caption may drop the
    // stage prefix; a reader announces controls in a flat list — the same
    // no-panel context an automation lane has — so the full name stays here.
    const auto& a11y = accessibleName.isNotEmpty() ? accessibleName : name;
    s.setTitle (a11y);
    s.setDescription (a11y);
    // …and the OTHER half of §8, which had no implementation until 0.1.1:
    // KEYBOARD OPERABILITY. This line is the one that DOES something, and the
    // reason is a JUCE default that differs per widget class — verified in the
    // vendored source rather than assumed: `Slider` ends its constructor with
    // `setWantsKeyboardFocus (false)`, while `Button` sets it true
    // unconditionally and `ComboBox` sets `! isLabelEditable` (true for ours).
    // So before 0.1.1 tab traversal reached every button and combo and SKIPPED
    // all forty knobs, and `Slider::keyPressed`'s arrow handling — which JUCE
    // already implements — was unreachable on every one of them.
    //
    // Accepting focus is all that is added, and the distinction matters: a
    // focusABLE control never TAKES focus, it only receives it when the user
    // tabs or clicks — so `EDITOR_WANTS_KEYBOARD_FOCUS FALSE` stays as it is
    // and the plugin still never steals the host's transport keys. In a plugin
    // the feature is live exactly when the host has already given the editor
    // focus; in the Standalone, always.
    s.setWantsKeyboardFocus (true);
}

void AnabasisAudioProcessorEditor::attachSlider (juce::Slider& s, const char* id)
{
    sliderAtts.add (new SliderAttachment (processor.apvts, id, s));

    auto* p = processor.apvts.getParameter (id);
    if (auto* k = dynamic_cast<Knob*> (&s); k != nullptr && p != nullptr)
    {
        k->resetValue = p->getNormalisableRange().convertFrom0to1 (p->getDefaultValue());
        k->resetParam = p;
    }
}

void AnabasisAudioProcessorEditor::passComboHoverThrough (juce::ComboBox& box)
{
    for (auto* child : box.getChildren())
        if (dynamic_cast<juce::Label*> (child) != nullptr)
            child->setInterceptsMouseClicks (false, false);
}

void AnabasisAudioProcessorEditor::setupCombo (juce::ComboBox& box, const char* id,
                                               const juce::String& tip)
{
    auto* cp = processor.apvts.getParameter (id);
    if (cp != nullptr)
        box.addItemList (cp->getAllValueStrings(), 1);
    box.setTooltip (tidyTip (tip));
    box.setRepaintsOnMouseActivity (true);
    passComboHoverThrough (box);
    allCombos.add (&box);
    addAndMakeVisible (box);
    comboAtts.add (new ComboBoxAttachment (processor.apvts, id, box));
    registerAnimated (box);
    // Title = the REGISTRY name (brief §8), no longer the tooltip: since the
    // R2 tooltip set the two are different strings, and the title must keep
    // announcing what the automation lane shows.
    box.setTitle (cp != nullptr ? cp->getName (24) : tidyTip (tip));
    // REDUNDANT against JUCE's default (`ComboBox` already sets
    // `! isLabelEditable`, and ours are not editable) and kept anyway: the
    // default is JUCE's to change, and this file should not depend on it
    // silently. Stated as redundant so nobody reads it as the fix — the fix is
    // in `setupRotary`, where the default is the opposite way round.
    box.setWantsKeyboardFocus (true);
}

void AnabasisAudioProcessorEditor::setupToggle (juce::ToggleButton& t, const char* id,
                                                const juce::String& text, const juce::String& tip)
{
    t.setButtonText (text);
    if (tip.isNotEmpty()) t.setTooltip (tidyTip (tip));
    addAndMakeVisible (t);
    buttonAtts.add (new ButtonAttachment (processor.apvts, id, t));
    registerAnimated (t);
    // Registry name as title (brief §8) — see setupCombo.
    auto* p = processor.apvts.getParameter (id);
    t.setTitle (p != nullptr ? p->getName (24) : text);
    t.setWantsKeyboardFocus (true);   // redundant vs Button's default — see setupCombo
}

void AnabasisAudioProcessorEditor::setupComboInternal (juce::ComboBox& box,
                                                       const juce::StringArray& items,
                                                       const juce::String& name,
                                                       const juce::String& tip, juce::Value value)
{
    box.addItemList (items, 1);          // JUCE reserves item ID 0 for "nothing selected"
    box.setTooltip (tidyTip (tip));
    box.setRepaintsOnMouseActivity (true);
    passComboHoverThrough (box);
    allCombos.add (&box);
    addAndMakeVisible (box);
    // The InternalState fields are 0-BASED and their encodings are consumed
    // (and serialized) that way — `iid::oversample` 0..4 = Off/2x/4x/8x/16x,
    // `iid::osPhase` 0 min / 1 linear, `iid::offlineQuality` 0 Follow / 1 Force
    // Max. Binding `getSelectedIdAsValue()` straight onto the property stored
    // the ITEM ID, i.e. index + 1: every choice landed one step high (picking
    // "Off" turned oversampling ON, "Minimum" phase gave linear phase, "Follow"
    // forced maximum offline quality), and the stored default 0 matched no item
    // so the boxes opened blank. Map index ↔ value explicitly, the same shape
    // `uiScaleBox` uses for index ↔ percent — the encoding is contract, the
    // widget's numbering is not.
    box.setSelectedItemIndex (juce::jlimit (0, items.size() - 1, (int) value.getValue()),
                              juce::dontSendNotification);
    // `b = &box` rather than `&box`: `box` is a reference PARAMETER, so capturing
    // it by reference has the same lifetime defect as the `ist` capture above —
    // the parameter dies with this call, the closure outlives it. The pointer is
    // captured by value and the ComboBox it names is an editor member, which is
    // what actually keeps it alive. (`value` is a `juce::Value`, a refcounted
    // handle, and is correctly captured by copy already.)
    box.onChange = [b = &box, value]() mutable
    { value.setValue (juce::jmax (0, b->getSelectedItemIndex())); };
    registerAnimated (box);
    // The accessibility name. `setupCombo` (the APVTS path) sets one from the
    // parameter's registry name; these host-hidden combos have none, and unlike
    // a Button — which falls back to its button text — a ComboBox with an empty
    // title exposes no name at all. The Settings row's LABEL is the honest
    // source (`name`); the tooltip stopped being usable for this when the R2
    // set made it descriptive prose. The state suite finds these by title.
    box.setTitle (name);
    // The §8 tail call the other helpers make. Redundant here for the same
    // reason as in `setupCombo` — a non-editable `ComboBox` is focusable by
    // JUCE default — so the Settings panel was never actually unreachable;
    // the 0.1.1 audit reported it as a gap and the mutation check disproved
    // the consequence. Kept for consistency and to pin the intent, and
    // labelled so the next reader does not re-derive that.
    box.setWantsKeyboardFocus (true);
}

void AnabasisAudioProcessorEditor::setupToggleInternal (juce::ToggleButton& t,
                                                        const juce::String& text,
                                                        const juce::String& name,
                                                        const juce::String& tip, juce::Value value)
{
    t.setButtonText (text);
    t.setTooltip (tidyTip (tip));
    addAndMakeVisible (t);
    t.getToggleStateValue().referTo (value);
    registerAnimated (t);
    t.setTitle (name.isNotEmpty() ? name : text);   // as setupToggle does
    t.setWantsKeyboardFocus (true);   // redundant vs Button's default — see setupCombo
}

// ============================================================================
//  Paint / layout
// ============================================================================
void AnabasisAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);

    auto top = getLocalBounds().removeFromTop (kBarH).toFloat();
    g.setColour (colours::bgPanel);
    g.fillRect (top);
    g.setColour (colours::outline);
    g.drawLine (0, top.getBottom(), (float) getWidth(), top.getBottom(), 1.0f);

    g.setColour (colours::text);
    g.setFont (juce::Font (juce::FontOptions (22.0f)).withExtraKerningFactor (0.18f));
    g.drawText ("ANABASIS", 18, 0, 240, kBarH, juce::Justification::centredLeft);
    g.setColour (colours::accent);
    g.setFont (juce::Font (juce::FontOptions (10.0f)).withExtraKerningFactor (0.25f));
    g.drawText ("MASTERING MAXIMIZER", 162, 0, 190, kBarH, juce::Justification::centredLeft);

    if (advanced)
    {
        // §6.3 — four panel zones, utility + macro rows, bottom metering well.
        const char* titles[] = { "COMP", "CLIP / COLOUR", "LIMITER", "EQ" };
        const int panelW = (getWidth() - 5 * 8) / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto r = juce::Rectangle<int> (8 + i * (panelW + 8), kBarH + 6,
                                           panelW, kPanelRowH - 12).toFloat();
            glass::fillPanel (g, r, 10.0f, colours::bgPanel.withAlpha (0.55f), 0.8f);
            g.setColour (colours::textDim);
            g.setFont (juce::Font (juce::FontOptions (11.0f)).withExtraKerningFactor (0.22f));
            g.drawText (titles[i], r.toNearestInt().removeFromTop (22).reduced (10, 0),
                        juce::Justification::centredLeft);
        }
        const int utilTop  = kBarH + kPanelRowH;
        const int meterTop = utilTop + kUtilityH;
        auto util  = juce::Rectangle<int> (0, utilTop, getWidth(), kUtilityH).toFloat().reduced (8.0f, 4.0f);
        auto meter = juce::Rectangle<int> (0, meterTop, getWidth(), kMeterRowH).toFloat().reduced (8.0f, 6.0f);
        g.setColour (colours::bgPanel.withAlpha (0.45f));
        g.fillRoundedRectangle (util, 10.0f);
        glass::fillPanel (g, meter, 10.0f, colours::bgPanel.withAlpha (0.5f), 0.8f);

        // §5.3 / §6.3: per-parameter detach badges — an accent dot on each
        // control the user has taken off its macro curve.
        // The ids come from `managed_params::ids` — the ONE list the mapper and
        // the wrapper's detach discriminator already share — rather than being
        // written out again here, which is what this table used to do. The
        // knobs are a PARALLEL array in that list's order.
        //
        // THE SIZE IS NOW ACTUALLY CHECKED. This array was declared with an
        // explicit `[managed_params::kCount]` bound and the comment claimed a
        // mismatch was a compile error. It was not: aggregate initialisation
        // with FEWER initialisers than the bound is legal C++ and
        // value-initialises the rest, so raising `kCount` to 10 without adding a
        // tenth knob compiled cleanly and then dereferenced a null
        // `juce::Slider*` in this very loop — a crash at paint time instead of a
        // build failure, which is the opposite of what the comment promised.
        // The bound is deduced now and the `static_assert` is the check; the
        // matching one for `ids` vs `kCount` lives beside their declarations.
        const juce::Slider* const badged[] = {
            &limGainK, &thresholdK, &ratioK, &driveK, &shapeK,
            &depthK,   &dynTiltK,   &eqTiltK, &colToneK,
        };
        static_assert (std::size (badged) == (std::size_t) managed_params::kCount,
                       "the badge table must carry exactly one knob per managed parameter");
        g.setColour (colours::accent);
        for (int i = 0; i < managed_params::kCount; ++i)
            if (const auto* k = badged[i];
                processor.detachMask().contains (managed_params::ids[i]))
                g.fillEllipse ((float) k->getRight() - 10.0f, (float) k->getY() + 2.0f,
                               7.0f, 7.0f);
    }
    else
    {
        // §6.2 — Simple: knob focus area, right meter panel, bottom GR strip.
        auto rightPanel = juce::Rectangle<int> (getWidth() - 300, kBarH + 8,
                                                292, kSimpleH - kBarH - 128 - 16).toFloat();
        glass::fillPanel (g, rightPanel, 10.0f, colours::bgPanel.withAlpha (0.55f), 0.8f);
        auto grStrip = juce::Rectangle<int> (0, kSimpleH - 120, getWidth(), 120)
                           .toFloat().reduced (8.0f, 6.0f);
        glass::fillPanel (g, grStrip, 10.0f, colours::bgPanel.withAlpha (0.5f), 0.8f);
    }
}

void AnabasisAudioProcessorEditor::resized()
{
    // `layoutAdvanced`/`layoutSimple` dereference the view unique_ptrs, and
    // they are safe today only because nothing sets a size before the views
    // exist — the first `setSize` is `applyUiScale()` at the end of the
    // constructor. That is "safe by ordering" again: a `setSize` moved up, or a
    // host-driven `setScaleFactor` during construction, would crash here rather
    // than lay out nothing. One guard says what the layout requires.
    //
    // The `jassert` is the other half, added round 44: without it the guard
    // turned a crash into a SILENTLY blank window, which is strictly harder to
    // diagnose. `juce::Component::setSize` is a no-op when the size is
    // unchanged, so an early return here followed by the constructor's
    // `applyUiScale()` — which passes the very dimensions the guard left in
    // place — would never re-enter this function, and every child would stay at
    // zero bounds with nothing reported. The constructor now also calls
    // `resized()` unconditionally at its end, so the layout no longer depends on
    // `setSize` observing a change.
    if (meterView == nullptr || grView == nullptr || spectrumView == nullptr
        || clipCurve == nullptr || eqCurve == nullptr)
    {
        jassertfalse;   // a size arrived before the views existed — see above
        return;
    }

    auto bounds = getLocalBounds();
    auto bar = bounds.removeFromTop (kBarH);

    titleButton.setBounds (bar.getX() + 8, 0, 330, kBarH);

    auto r = bar.reduced (8, 9);
    bypassToggle.setBounds (r.removeFromRight (84));
    r.removeFromRight (6);
    advancedToggle.setBounds (r.removeFromRight (66));
    r.removeFromRight (6);
    settingsButton.setBounds (r.removeFromRight (74));
    r.removeFromRight (6);
    redoButton.setBounds (r.removeFromRight (30));
    undoButton.setBounds (r.removeFromRight (30));
    r.removeFromRight (10);
    copyButton.setBounds (r.removeFromRight (46));
    r.removeFromRight (6);
    abControl.setBounds (r.removeFromRight (46).reduced (0, 1)); // the sibling's shorter oval (#4)

    // Preset browser fills between the wordmark and the right cluster.
    r.removeFromLeft (352 - r.getX());
    presetPrev.setBounds (r.removeFromLeft (24));
    presetNext.setBounds (r.removeFromRight (24));
    presetName.setBounds (r.reduced (2, 0));

    dimOverlay.setBounds (getLocalBounds().withTrimmedTop (kBarH));

    for (auto* b : { &aboutBackdrop, &settingsBackdrop, &savePresetBackdrop })
        b->setBounds (getLocalBounds());
    aboutBackdrop.panel = getLocalBounds().withSizeKeepingCentre (440, 290);   // the sibling's About geometry
    // The sibling's link band: LEFT-aligned at the panel's content inset,
    // 170×20, 50 px up from the panel bottom — not centred.
    aboutLink.setBounds (aboutBackdrop.panel.reduced (30, 26).getX(),
                         aboutBackdrop.panel.getBottom() - 50, 170, 20);
    // 350 fits the content exactly (title 24+6, SIX 30+6 combo rows, two
    // 26+6 toggles, 16 px top/bottom = 350): the 310 this held was sized for
    // four combos and three toggles, and 0.1.1 traded the True-Peak Meter
    // toggle for the two ADR-0020 standard selectors — a net +32. Recomputed
    // rather than nudged, because the number this replaced was itself the
    // recomputation that removed ~90 px of empty glass (R2 item 2).
    settingsBackdrop.panel = getLocalBounds().withSizeKeepingCentre (380, 350);
    savePresetBackdrop.panel = getLocalBounds().withSizeKeepingCentre (340, 150);

    {
        auto sp = settingsBackdrop.panel.reduced (20, 16);
        settingsTitle.setBounds (sp.removeFromTop (24));
        sp.removeFromTop (6);
        auto row = [&sp] (int h = 30) { auto rr = sp.removeFromTop (h); sp.removeFromTop (6); return rr; };
        auto splitRow = [] (juce::Rectangle<int> rr, juce::Label& l, juce::Component& c)
        {
            l.setBounds (rr.removeFromLeft (130));
            c.setBounds (rr.reduced (0, 2));
        };
        splitRow (row(), oversampleLabel, oversampleBox);
        splitRow (row(), phaseLabel,      phaseBox);
        splitRow (row(), offlineLabel,    offlineBox);
        splitRow (row(), integratedLabel, integratedBox);
        splitRow (row(), rmsRefLabel,     rmsRefBox);
        splitRow (row(), uiScaleLabel,    uiScaleBox);
        animToggle.setBounds (row (26));
        tooltipsToggle.setBounds (row (26));
    }
    {
        auto sp = savePresetBackdrop.panel.reduced (20, 16);
        saveTitle.setBounds (sp.removeFromTop (24));
        sp.removeFromTop (8);
        saveNameEditor.setBounds (sp.removeFromTop (28));
        sp.removeFromTop (10);
        auto btns = sp.removeFromTop (26);
        // Save LEFT, Cancel RIGHT (owner directive 2026-08-05).
        saveOkButton.setBounds (btns.removeFromLeft (btns.getWidth() / 2).reduced (4, 0));
        saveCancelButton.setBounds (btns.reduced (4, 0));
    }

    if (advanced)
        layoutAdvanced (bounds);
    else
        layoutSimple (bounds);
}

void AnabasisAudioProcessorEditor::layoutAdvanced (juce::Rectangle<int> body)
{
    const int panelW = (getWidth() - 5 * 8) / 4;
    auto panel = [&] (int i)
    {
        return juce::Rectangle<int> (8 + i * (panelW + 8), kBarH + 6, panelW, kPanelRowH - 12)
                   .reduced (8).withTrimmedTop (20);
    };

    // A knob cell: knob + value box (from setTextBoxStyle) + caption label.
    auto placeRow = [] (juce::Rectangle<int>& area,
                        std::initializer_list<std::pair<juce::Slider*, juce::Label*>> cells,
                        int cellH = 84)
    {
        auto rowArea = area.removeFromTop (cellH);
        const int w = rowArea.getWidth() / juce::jmax (1, (int) cells.size());
        for (auto& [s, l] : cells)
        {
            auto cell = rowArea.removeFromLeft (w);
            l->setBounds (cell.removeFromBottom (13));
            s->setBounds (cell.reduced (2, 0));
        }
    };

    // R2 item 2 (2026-08-05): every zone's MODE combo takes the SAME slot — the
    // first row of the panel body, right half — which is the slot the EQ panel
    // already used. Before this, three zones parked their combo in three
    // different places, and the LIMITER's foot row squeezed AUTO + TP + Style
    // into one line where both toggle labels truncated to "..":
    // `drawToggleButton` fits text into whatever is right of the pill, and
    // 44–52 px cells leave ~16 px. The foot rows now carry toggles only, at
    // half-panel widths their labels fit.
    //
    // "Panel body, first row" and not the caption band itself: `panel()` hands
    // back the zone rect already `withTrimmedTop (20)`, so the caption the
    // `paint` pass draws is OUTSIDE this rectangle and the combo row sits
    // directly beneath it. 20 px is short for a combo (they are 22–24), so
    // moving it up beside the caption would mean shrinking the control rather
    // than relocating it. An earlier revision of this comment said "in the zone
    // HEADER, top-right beside the caption", which describes a layout the code
    // never produced.
    //
    // FULL panel width since 0.1.1 (owner directive): the half-width box
    // clipped "Transparent" to "Transpar…" in the LIMITER's style combo, and
    // the fix is applied to all FOUR zones so the row reads as one system —
    // each combo spans its zone body with the same 10 px side margins the
    // half-width box already had on its right (8 px panel reduce + 2 px cell
    // reduce, now symmetric).
    {   // COMP
        auto a = panel (0);
        auto boxRow = a.removeFromTop (24);
        detectorBox.setBounds (boxRow.reduced (2, 1));
        placeRow (a, { { &ratioK, &ratioL }, { &thresholdK, &thresholdL } });
        placeRow (a, { { &attackK, &attackL }, { &releaseK, &releaseL } });
        placeRow (a, { { &kneeK, &kneeL }, { &compMixK, &compMixL } });
        // The ADR-0019 stereo-link knob rides a single-knob row, the CLIP
        // zone's Dynamic Tame grammar — 74 px, so the zone's remaining budget
        // (24 combo + 3×84 rows + 26 toggles + 8 + 14 meter = 324 of 398)
        // closes exactly instead of leaving the foot row floating.
        auto lrow = a.removeFromTop (74);
        placeRow (lrow, { { &compLinkK, &compLinkL } }, 74);
        auto row = a.removeFromTop (26);
        compAutoToggle.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (2, 2));
        a.removeFromTop (8);
        compGrMeter.setBounds (a.removeFromTop (14));   // [GR meter] — this stage's own
    }
    {   // CLIP / COLOUR
        auto a = panel (1);
        auto boxRow = a.removeFromTop (24);
        modelBox.setBounds (boxRow.reduced (2, 1));
        placeRow (a, { { &shapeK, &shapeL }, { &driveK, &driveL } });
        placeRow (a, { { &clipMixK, &clipMixL }, { &depthK, &depthL } });
        placeRow (a, { { &balanceK, &balanceL }, { &colToneK, &colToneL } });
        // 76, not the 84 the paired rows use: this row carries ONE knob, and
        // the 8 px it gives back are what stop the curve well overhanging the
        // panel. The zone body is 398 px (446 − 12, `reduced (8)`, minus the
        // 20 px caption band); the combo row and the four knob rows take 358,
        // leaving exactly the 40 px the `jmax` floor below insists on. At 84
        // the floor won by 8 px and the curve drew past the panel's outline —
        // a pre-existing overhang (10 px before the combo row moved here, so
        // this is the arithmetic that finally closes it, not one it opened).
        auto row = a.removeFromTop (76);
        placeRow (row, { { &dynTiltK, &dynTiltL } }, 76);
        a.removeFromTop (6);
        clipCurve->setBounds (a.removeFromTop (juce::jmax (40, a.getHeight())));  // [live curve]
    }
    {   // LIMITER
        auto a = panel (2);
        auto boxRow = a.removeFromTop (24);
        styleBox.setBounds (boxRow.reduced (2, 1));
        placeRow (a, { { &limGainK, &limGainL }, { &ceilingK, &ceilingL } });
        placeRow (a, { { &lookaheadK, &lookaheadL }, { &limReleaseK, &limReleaseL } });
        placeRow (a, { { &linkK, &linkL }, { &preserveK, &preserveL } });
        auto row = a.removeFromTop (26);
        limAutoToggle.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (2, 2));
        tpToggle.setBounds (row.reduced (2, 2));
        a.removeFromTop (8);
        limGrMeter.setBounds (a.removeFromTop (14));    // [GR meter] — this stage's own
    }
    {   // EQ (the densest panel: three-across rows, smaller cells)
        auto a = panel (3);
        auto boxRow = a.removeFromTop (24);
        eqPosBox.setBounds (boxRow.reduced (2, 1));
        placeRow (a, { { &eqTiltK, &eqTiltL }, { &lsFreqK, &lsFreqL }, { &lsGainK, &lsGainL } }, 78);
        placeRow (a, { { &hsFreqK, &hsFreqL }, { &hsGainK, &hsGainL }, { &b1FreqK, &b1FreqL } }, 78);
        placeRow (a, { { &b1GainK, &b1GainL }, { &b1QK, &b1QL }, { &b2FreqK, &b2FreqL } }, 78);
        placeRow (a, { { &b2GainK, &b2GainL }, { &b2QK, &b2QL } }, 78);
        a.removeFromTop (4);
        eqCurve->setBounds (a);                         // [response curve]
    }

    // Utility row: input gain + SC HPF + dither cluster + monitor toggles.
    auto util = juce::Rectangle<int> (0, kBarH + kPanelRowH, getWidth(), kUtilityH)
                    .reduced (16, 4);
    {
        // Input Gain and SC HPF ride as FADERS here (R2 item 2): a 40 px-tall
        // strip knob was the smallest control on the page for two parameters
        // set by ear at session start — the family's linear style gives them a
        // full-width track instead. Same Knob objects, same attachments/reset/
        // value box; only the presentation changed (the style is set at setup).
        auto left = util.removeFromLeft (360);
        auto cell = left.removeFromLeft (175);
        inputGainL.setBounds (cell.removeFromBottom (12));
        inputGainK.setBounds (cell);
        left.removeFromLeft (10);
        cell = left.removeFromLeft (175);
        scHpfL.setBounds (cell.removeFromBottom (12));
        scHpfK.setBounds (cell);

        auto right = util.removeFromRight (300);
        auto toggles = right;
        compToggle.setBounds (toggles.removeFromLeft (92).reduced (2, 14));
        deltaToggle.setBounds (toggles.removeFromLeft (92).reduced (2, 14));
        freezeToggle.setBounds (toggles.removeFromLeft (100).reduced (2, 14));

        // Caption UNDER the cluster, aligned with the fader captions to its
        // left (same removeFromBottom(12) grammar), centred on the combo+SHAPE
        // pair so it names the group, not one control. HORIZONTAL inset only
        // (0.1.2 item 7): the previous `.reduced (12, 4)` trimmed 4 px off the
        // bottom too, so this caption's baseline floated 4 px above the
        // Input Gain / SC HPF captions it sits beside.
        auto mid = util.reduced (12, 0);
        const int clusterW = 100 + 8 + 88;
        auto cluster = mid.removeFromLeft (clusterW);
        ditherCaption.setBounds (cluster.removeFromBottom (12));
        auto row = cluster.withSizeKeepingCentre (clusterW, 26);
        ditherBox.setBounds (row.removeFromLeft (100));
        row.removeFromLeft (8);
        shapingToggle.setBounds (row.removeFromLeft (88));
    }

    // §6.3 shared metering strip: ONE graph well (GR history / spectrum,
    // switched by the toggle pill on the graph itself — int_spectrumOn is the
    // mode) · loudness block right. The two views share the SAME bounds and
    // only visibility flips, so a mode switch needs no relayout. Directly
    // under the utility row since 0.1.2 — the read-only macro row that sat
    // between them was removed (item 11).
    auto strip = juce::Rectangle<int> (0, kBarH + kPanelRowH + kUtilityH,
                                       getWidth(), kMeterRowH).reduced (8, 6);
    meterView->setBounds (strip.removeFromRight (300));
    spectrumView->setBounds (strip);
    grView->setBounds (strip);
    const bool spectrumOn = (bool) processor.internalState.state()
                                .getProperty (iid::spectrumOn, false);
    spectrumView->setVisible (spectrumOn);
    grView->setVisible (! spectrumOn);

    juce::ignoreUnused (body);
}

void AnabasisAudioProcessorEditor::layoutSimple (juce::Rectangle<int> body)
{
    juce::ignoreUnused (body);
    auto left = juce::Rectangle<int> (0, kBarH, getWidth() - 308, kSimpleH - kBarH - 128);

    // The one knob — the unambiguous visual focus (§5.1/§8).
    auto knobArea = left.removeFromTop (330).withSizeKeepingCentre (250, 300);
    bigLoudnessL.setBounds (knobArea.removeFromBottom (18));
    bigLoudnessK.setBounds (knobArea);
    editedDot.setBounds (bigLoudnessK.getRight() - 6, bigLoudnessK.getY() + 16, 14, 14);

    // Secondary macro row: CHARACTER · TONE · CEILING(+lock).
    auto macroRow = left.removeFromTop (118).reduced (24, 0);
    const int w = macroRow.getWidth() / 3;
    auto place = [] (juce::Rectangle<int> c, Knob& k, juce::Label& l)
    {
        l.setBounds (c.removeFromBottom (13));
        k.setBounds (c.reduced (8, 0));
    };
    place (macroRow.removeFromLeft (w), simpleCharacterK, simpleCharacterL);
    place (macroRow.removeFromLeft (w), simpleToneK, simpleToneL);
    auto ceilCell = macroRow;
    // The ceiling's two mode switches stack in the right column: TP above
    // LOCK — TP decides what the ceiling MEANS (dBTP vs sample peak), the
    // lock decides whether presets may move it.
    {
        // 74/70 px, not the 54/52 this shipped at first: `drawToggleButton`
        // fits the label into what is right of the pill (~35 px in), so a
        // 52 px cell left "LOCK" ~16 px and it truncated to "L…" (R2 item 2).
        auto col = ceilCell.removeFromRight (74);
        tpSimpleToggle.setBounds (col.removeFromTop (col.getHeight() / 2)
                                     .withSizeKeepingCentre (70, 20));
        ceilingLockToggle.setBounds (col.withSizeKeepingCentre (70, 20));
    }
    place (ceilCell, simpleCeilingK, simpleCeilingL);

    // Toggle row: [Loudness Comp] [Delta] [Freeze] [Learn] + out LUFS.
    auto toggles = left.removeFromTop (40).reduced (24, 4);
    compToggle.setBounds (toggles.removeFromLeft (96));
    deltaToggle.setBounds (toggles.removeFromLeft (86));
    freezeToggle.setBounds (toggles.removeFromLeft (94));
    learnButton.setBounds (toggles.removeFromLeft (78).reduced (0, 2));
    outLufsValue.setBounds (toggles.removeFromRight (72));
    outLufsCaption.setBounds (toggles.removeFromRight (70));

    // §6.2 wells: the right meter panel and the bottom graph well — since
    // 2026-08-05 the SAME switchable GR/spectrum well as Advanced (the §6.2
    // "GR-only Simple strip" wireframe is superseded by the owner's combined-
    // view directive; DESIGN is superseded section by section as built).
    {
        const bool spectrumOn = (bool) processor.internalState.state()
                                    .getProperty (iid::spectrumOn, false);
        spectrumView->setVisible (spectrumOn);
        grView->setVisible (! spectrumOn);
    }
    meterView->setBounds (juce::Rectangle<int> (getWidth() - 300, kBarH + 8,
                                                292, kSimpleH - kBarH - 128 - 16));
    const auto well = juce::Rectangle<int> (0, kSimpleH - 120, getWidth(), 120)
                          .reduced (8, 6);
    grView->setBounds (well);
    spectrumView->setBounds (well);
}

// ============================================================================
//  Mode / overlays / cadence
// ============================================================================
void AnabasisAudioProcessorEditor::updateModeVisibility()
{
    advanced = processor.apvts.getRawParameterValue (pid::advancedMode)->load() >= 0.5f;

    const bool adv = advanced;
    juce::Component* advOnly[] = {
        &ratioK, &thresholdK, &attackK, &releaseK, &kneeK, &compMixK, &compLinkK,
        &ratioL, &thresholdL, &attackL, &releaseL, &kneeL, &compMixL, &compLinkL,
        &compAutoToggle, &detectorBox,
        &shapeK, &driveK, &clipMixK, &balanceK, &colToneK, &depthK, &dynTiltK,
        &shapeL, &driveL, &clipMixL, &balanceL, &colToneL, &depthL, &dynTiltL,
        &modelBox,
        &limGainK, &ceilingK, &lookaheadK, &limReleaseK, &linkK, &preserveK,
        &limGainL, &ceilingL, &lookaheadL, &limReleaseL, &linkL, &preserveL,
        &limAutoToggle, &tpToggle, &styleBox,
        &eqTiltK, &lsFreqK, &lsGainK, &hsFreqK, &hsGainK,
        &b1FreqK, &b1GainK, &b1QK, &b2FreqK, &b2GainK, &b2QK,
        &eqTiltL, &lsFreqL, &lsGainL, &hsFreqL, &hsGainL,
        &b1FreqL, &b1GainL, &b1QL, &b2FreqL, &b2GainL, &b2QL,
        &eqPosBox,
        &inputGainK, &scHpfK, &inputGainL, &scHpfL,
        &ditherBox, &ditherCaption, &shapingToggle,
    };
    for (auto* c : advOnly)
        c->setVisible (adv);

    for (juce::Component* c : { (juce::Component*) clipCurve.get(), (juce::Component*) eqCurve.get(),
                                (juce::Component*) &compGrMeter, (juce::Component*) &limGrMeter })
        c->setVisible (adv);

    juce::Component* simpleOnly[] = {
        &bigLoudnessK, &bigLoudnessL, &simpleCharacterK, &simpleCharacterL,
        &simpleToneK, &simpleToneL, &simpleCeilingK, &simpleCeilingL,
        &ceilingLockToggle, &tpSimpleToggle, &learnButton, &outLufsCaption, &outLufsValue,
    };
    for (auto* c : simpleOnly)
        c->setVisible (! adv);

    // Shared in BOTH views (§6.2 toggle row / §6.3 utility row): the layout
    // that runs next places them for whichever view is active.
    for (auto* c : { &compToggle, &deltaToggle, &freezeToggle })
        c->setVisible (true);

    if (adv)
        editedDot.setVisible (false);   // Advanced shows per-control badges instead
}

// The Settings boxes are written by the user through `onChange` and read back
// here, because the InternalState tree changes UNDER them too: a session load
// runs `InternalState::replaceFrom`, which rewrites the same tree object the
// editor is bound to. The round-25 off-by-one fix replaced a two-way
// `Value::referTo` with a one-shot seed plus a writer, which left the
// widget→state direction working and state→widget silent — a panel left open
// across a project load then showed the PREVIOUS project's oversampling, phase
// and offline quality, and "correcting" one of them would have written back a
// setting that was already active. Re-seeding on the existing 24 Hz tick keeps
// the explicit index↔value mapping (the thing the referTo could not express)
// and restores the missing direction; it only touches a box whose selection
// actually differs, so it is a comparison per tick in the steady state.
// The Ceiling's UNIT follows `truePeakMode` (ADR-0015) — and a JUCE Slider
// recomputes its value-box label only inside `updateText()`, which runs on a
// value change, a `setTextBoxStyle`, a relayout or a look-and-feel change.
// NEVER on a repaint. Flipping TP moves no ceiling value and triggers no
// relayout (the graph-well branch that used to call `resized()` from the tick
// is a visibility flip now), so the box went on showing the previous suffix
// until the ceiling itself was touched: a generic host editor re-queries
// `getText` and was right, while the plugin's own readout carried exactly the
// stale claim the mode-aware unit exists to remove.
//
// BOTH controls are refreshed because both are the same parameter shown twice
// — the Advanced limiter zone's `ceilingK` and the Simple row's
// `simpleCeilingK`. `updateText()` re-runs the attachment's
// `textFromValueFunction`, which is the parameter's own `getText`.
//
// Edge-gated: `updateText()` sets the label and repaints, and this runs 24
// times a second.
void AnabasisAudioProcessorEditor::refreshCeilingUnit()
{
    // Read through `CeilingUnitSource`, NOT through the raw parameter: that
    // predicate is what the value-text lambda itself consults, so the gate and
    // the suffix cannot disagree by construction. A raw read is the same answer
    // whenever the holder is wired — and a different one when it is not, where
    // the lambda falls back to " dB" while a raw `true` would say the box shows
    // " dBTP". One decider, one truth; the constructor seeds `shownTpMode` from
    // this same call for the same reason.
    const bool tp = processor.ceilingUnit.truePeakEngaged();
    if (tp == shownTpMode)
        return;
    shownTpMode = tp;
    ceilingK.updateText();
    simpleCeilingK.updateText();
}

void AnabasisAudioProcessorEditor::refreshInternalSettingsBoxes()
{
    const auto& ist = processor.internalState.state();
    auto reseed = [] (juce::ComboBox& box, int wantIndex)
    {
        wantIndex = juce::jlimit (0, juce::jmax (0, box.getNumItems() - 1), wantIndex);
        if (box.getSelectedItemIndex() != wantIndex)
            box.setSelectedItemIndex (wantIndex, juce::dontSendNotification);
    };
    reseed (oversampleBox, (int) ist.getProperty (iid::oversample, 0));
    reseed (phaseBox,      (int) ist.getProperty (iid::osPhase, 0));
    reseed (offlineBox,    (int) ist.getProperty (iid::offlineQuality, 0));
    // The ADR-0020 pair joins the re-seeded set for the reason the header
    // states: their widget state is an INDEX and the stored value is an int,
    // so a `juce::Value` binding cannot carry them and a project load would
    // otherwise leave both boxes showing the previous session's standards.
    reseed (integratedBox, (int) ist.getProperty (iid::integratedStd, 0));
    reseed (rmsRefBox,     (int) ist.getProperty (iid::rmsRef, 0));

    // uiScale is the same shape with one extra step: the box only DISPLAYS the
    // percent, so a stored change has to reach `applyUiScale()` as well or the
    // panel would read 150 % while the window stayed at 100 %.
    //
    // The read NORMALISES (see `normalisedUiScale`), and it happens before the
    // branch rather than inside it — that ordering is the fix, not an
    // arrangement. When the branch owned the convergence, a stored value whose
    // nearest step equalled the displayed one (110 against a box showing 100 %)
    // was clamped on every read and written back on none, so the session
    // re-serialised the illegal percent for ever.
    const int wantScaleIdx = nearestScaleIndex (normalisedUiScale());
    if (wantScaleIdx != uiScaleBox.getSelectedItemIndex())
    {
        uiScaleBox.setSelectedItemIndex (wantScaleIdx, juce::dontSendNotification);
        applyUiScale();
    }
}

// THE ONE READ of `iid::uiScale`, and it is now a READ — it clamps, it does not
// write. The correction moved to `InternalState::replaceFrom`, which is where a
// value the schema cannot represent enters (a hand-edited session, or one from a
// build whose ladder has since changed) and where every other field's read rule
// already lives.
//
// It used to write back from here, and this function is reached by
// `refreshInternalSettingsBoxes` — the 24 Hz settings poll — which made a
// DISPLAY TIMER a writer of the wrapper's `InternalState` tree. `replaceFrom` is
// the opposing writer, and VST3 does not promise `setStateInformation` on the
// message thread (KI-003), so that was a new writer pairing on the very poll
// round 51 had just cleaned of ValueTree access. Converging at adoption gets the
// same result — a session's illegal percent is corrected once, before anything
// can read it — with no writer on the poll at all.
//
// The clamp STAYS here because it is free and because it keeps this function
// total: the tree can hold an illegal percent only in the window before
// `replaceFrom` runs (a test writing the live tree directly, say), and a reader
// that returned it would put the rendered transform and the displayed step back
// out of agreement, which is the defect the single reading exists to prevent.
int AnabasisAudioProcessorEditor::normalisedUiScale() const
{
    return kScaleSteps[nearestScaleIndex (
        (int) processor.internalState.state().getProperty (iid::uiScale, 100))];
}

void AnabasisAudioProcessorEditor::applyUiScale()
{
    // Same reading as the box's, so the transform and the selection cannot
    // describe different steps — that was the whole defect.
    const float scale = (float) normalisedUiScale() / 100.0f;

    setSize (kWidth, advanced ? kAdvancedH : kSimpleH);
    setTransform (juce::AffineTransform::scale (hostScale * scale));
#if JUCE_MAC || JUCE_WINDOWS
    if (glContext.isAttached())
        glContext.triggerRepaint();
#endif
}

void AnabasisAudioProcessorEditor::setScaleFactor (float newScale)
{
    hostScale = (newScale > 0.0f) ? newScale : 1.0f;
    applyUiScale();
}

void AnabasisAudioProcessorEditor::parameterChanged (const juce::String&, float)
{
    // Of the two ids this listens to, `bypass` is AUTOMATABLE (`advancedMode`
    // is not — ADR-0010's X11 rationale — but an undo restore writes it, since
    // ADR-0018), and APVTS delivers on whichever thread wrote the value, so a
    // host automating Bypass calls this FROM THE AUDIO THREAD. `triggerAsyncUpdate()` posts to
    // the platform message queue, which takes a lock and on some platforms
    // allocates: the REALTIME_AUDIO_POLICY hard red line that
    // `MacroEngine::parameterChanged` refuses and that the wrapper's
    // `drainDetachBitsSoon` was rewritten to avoid. Post ONLY when already on
    // the message thread — where posting is free and the refresh stays exactly
    // as immediate as it was. Otherwise raise a flag the 24 Hz timer consumes,
    // which costs ≤ ~42 ms on a path that is host automation rather than a user
    // gesture (and the timer already re-derives the bypass dim independently,
    // so the visible half never waited on this).
    uiRefreshPending.store (true, std::memory_order_relaxed);
    if (juce::MessageManager::existsAndIsCurrentThread())
        triggerAsyncUpdate();
}

void AnabasisAudioProcessorEditor::handleAsyncUpdate()
{
    uiRefreshPending.store (false, std::memory_order_relaxed);
    updateModeVisibility();
    applyUiScale();
    dimOverlay.setVisible (processor.apvts.getRawParameterValue (pid::bypass)->load() >= 0.5f);
    repaint();
}

void AnabasisAudioProcessorEditor::timerCallback()
{
    // The off-message-thread half of parameterChanged (see there) — and the
    // only consumer of that flag, since the on-thread half posts normally.
    if (uiRefreshPending.exchange (false, std::memory_order_relaxed))
        handleAsyncUpdate();

    // The authoritative combo hover flag `drawComboBox` prefers. Without it
    // that code fell back for ever to a live cursor test it documents as a
    // pre-first-tick stopgap — and `allCombos`, collected by every combo
    // setup, was dead state: a half-ported piece of the Anamorph editor. One
    // flag per combo per tick; the boxes already repaint on mouse activity, so
    // this changes what is drawn, not how often.
    for (auto* c : allCombos)
        c->getProperties().set ("hov", c->isMouseOver (true));
    refreshInternalSettingsBoxes();
    refreshPresetDisplay();
    const bool bypassed = processor.apvts.getRawParameterValue (pid::bypass)->load() >= 0.5f;
    if (bypassed != dimOverlay.isVisible())
        dimOverlay.setVisible (bypassed);

    // -- out-LUFS readout (render short-term, the §2.9 tap) ------------------
    if (! advanced)
    {
        const float s = processor.meterLufsS();
        outLufsValue.setText (s <= -99.0f ? juce::String ("-")
                                          : juce::String (s, 1),
                              juce::dontSendNotification);
    }

    // -- Learn button state (§5.4 grammar) -----------------------------------
    {
        const auto& a = processor.adaptiveReadout();
        const double nowMs = juce::Time::getMillisecondCounterHiRes();
        juce::String text ("LEARN");
        if (a.isLearning())
        {
            const double leftMs = kLearnMinPassMs - (nowMs - learnStartedMs);
            if (leftMs > 0.0)
                text = juce::String ((int) std::ceil (leftMs * 0.001));
            learnButton.setColour (juce::TextButton::textColourOffId, colours::accent);
        }
        else if (learnStopPending)
        {
            // The commit lands at a block top; once the engine reports the
            // pass over, compare what the references did. Unmoved + already
            // learned-state unchanged = the documented empty pass.
            learnStopPending = false;
            const bool moved = a.hasLearned() != hadLearnedAtStop
                            || ! juce::exactlyEqual (a.publishedRefOnset(), refOnsetAtStop)
                            || ! juce::exactlyEqual (a.publishedRefTilt(), refTiltAtStop);
            if (! moved)
                emptyFlashUntilMs = nowMs + 1500.0;
        }
        if (! a.isLearning())
            learnButton.setColour (juce::TextButton::textColourOffId,
                                   nowMs < emptyFlashUntilMs ? colours::warn : colours::text);
        if (text != learnButton.getButtonText())
            learnButton.setButtonText (text);
    }

    // -- undo/redo button states --------------------------------------------
    refreshUndoRedoEnablement();

    // -- Advanced panel wells: per-stage GR + curve refreshes ----------------
    if (advanced)
    {
        // Per-channel lanes (0.1.2 item 12). The limiter lanes read the
        // per-channel copies rather than `meterGrDb()`: that figure is the
        // wrapper-published deepest-channel value the GR history shares, and
        // the two lanes exist precisely to stop folding the channels. Mono
        // layouts draw one full-height lane.
        const bool monoOut = processor.getTotalNumOutputChannels() < 2;
        compGrMeter.setGrDb (processor.meterCompGrDbCh (0),
                             processor.meterCompGrDbCh (1), monoOut);
        limGrMeter.setGrDb (processor.meterLimGrDbCh (0),
                            processor.meterLimGrDbCh (1), monoOut);
        clipCurve->refresh();
        eqCurve->refresh();
    }

    // -- the Ceiling's unit follows truePeakMode (ADR-0015) ------------------
    refreshCeilingUnit();

    // -- graph-well mode follows int_spectrumOn (the corner chips) -----------
    {
        const bool on = (bool) processor.internalState.state()
                            .getProperty (iid::spectrumOn, false);
        if (on != shownSpectrumOn)
        {
            shownSpectrumOn = on;
            // Both views hold the SAME bounds (set unconditionally by both
            // layouts), so a mode switch is a visibility flip, not the
            // `resized()` this used to trigger when the strip re-partitioned.
            spectrumView->setVisible (on);
            grView->setVisible (! on);
        }
    }

    // -- §5.3 badges: repaint when the mask changes, show the edited dot -----
    {
        const auto fp = processor.detachMask().joinIntoString ("|");
        if (fp != lastMaskFingerprint)
        {
            lastMaskFingerprint = fp;
            editedDot.setVisible (! advanced && fp.isNotEmpty());
            repaint();
        }
        else if (! advanced)
            editedDot.setVisible (fp.isNotEmpty());
    }
}

// Guarded assignments: `setEnabled` repaints, and the tick runs 24 times a
// second, so an unconditional write would repaint two buttons every frame.
void AnabasisAudioProcessorEditor::refreshUndoRedoEnablement()
{
    if (undoButton.isEnabled() != processor.canUndo())
        undoButton.setEnabled (processor.canUndo());
    if (redoButton.isEnabled() != processor.canRedo())
        redoButton.setEnabled (processor.canRedo());
}

void AnabasisAudioProcessorEditor::refreshPresetDisplay (bool recomputeNow)
{
    auto name = processor.currentPresetName();
    if (name.isEmpty())
        name = "Preset";               // §6.2 wireframe placeholder (C8: owner wording TODO)
    // The dirty compare is a full slot-tree equivalence — throttle it to
    // every 8th tick (~3 Hz) and reuse the last answer between.
    //
    // `recomputeNow` is what every NON-tick caller passes, and its absence was
    // a visible lag rather than a saving: undo/redo, the A/B toggle, a preset
    // apply, ‹/› and a save all call this straight after changing the state
    // the mark describes, and all they did was advance the divider — so the
    // freshly applied preset could keep rendering the PREVIOUS state's " *"
    // for up to ~333 ms. The throttle exists for the 24 Hz tick, which asks
    // "did anything change?" on spec; a caller that already knows it did is
    // not what it was protecting against.
    if (recomputeNow || ++dirtyPollDivider >= 8)
    {
        dirtyPollDivider = 0;
        shownDirty = processor.presetDirty();
    }
    const auto shown = shownDirty ? name + " *" : name;
    if (shown != presetShownName)
    {
        presetShownName = shown;
        presetName.setButtonText (shown);
    }
}

void AnabasisAudioProcessorEditor::showPresetMenu()
{
    auto dir = PresetManager::userPresetDirectory();
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.anabasis");
    files.sort();

    juce::PopupMenu m;
    // NO `setLookAndFeel (&lnf)`: that handed the menu a RAW pointer to an
    // editor member, and the menu window can outlive the editor if the host
    // tears the window down while it is open — the one part of the round-24
    // SafePointer hardening the look-and-feel did not cover. Parenting the
    // menu to the editor (`withParentComponent` below) closes it by
    // CONSTRUCTION instead: JUCE's MenuWindow is then a CHILD of this
    // component, so it cannot outlive it, and `Component::getLookAndFeel()`
    // walks up to the editor's `lnf` on its own — the family styling arrives
    // with no pointer to dangle. Both available alternatives were worse:
    // `dismissAllActiveMenus()` in the destructor also closes another
    // instance's menu, and a shared static LookAndFeel trades this for
    // static-destruction order at DLL unload.
    int factoryCount = 0;
    const auto* factory = PresetManager::factoryPresets (factoryCount);
    // ONE resolved row, ticked in whichever section it falls (ADR-0022). Each
    // row used to test its own NAME against the current one, so a user preset
    // sharing a factory preset's name ticked BOTH rows; the resolver answers
    // identity-first (exactly one row, or none — an outside-folder file and a
    // deleted preset are on no row and must not tick a same-named substitute)
    // and falls back to the name only for identity-less state.
    const int cur = PresetManager::selectedPresetRow (processor.currentPresetSelection(),
                                                      processor.currentPresetName(),
                                                      factory, factoryCount, files);
    m.addSectionHeader ("FACTORY");
    for (int i = 0; i < factoryCount; ++i)
        m.addItem (20001 + i, factory[i].name, true, i == cur);
    if (! files.isEmpty())
    {
        m.addSectionHeader ("USER");
        for (int i = 0; i < files.size(); ++i)
            m.addItem (i + 1, files.getReference (i).getFileNameWithoutExtension(), true,
                       factoryCount + i == cur);
    }
    const juce::String ellip = juce::String::charToString ((juce::juce_wchar) 0x2026);
    m.addSeparator();
    m.addItem (10001, "Save Preset" + ellip);
    m.addItem (10002, "Load Preset" + ellip);

    m.showMenuAsync (juce::PopupMenu::Options()
                         .withTargetComponent (presetName)
                         .withParentComponent (this)   // lifetime + look-and-feel; see above
                         .withMinimumWidth (228),
        // SafePointer, not a raw `this`: the menu outlives the editor if the
        // host tears the window down while it is open. JUCE dismisses with
        // result 0 in that case, so the early return already covered the
        // common path — this covers it by construction instead of by
        // dismissal ordering, and the same shape guards the file chooser.
        [safeThis = juce::Component::SafePointer<AnabasisAudioProcessorEditor> (this),
         files] (int r)
        {
            if (r == 0 || safeThis == nullptr) return;
            if (r == 10001) { safeThis->showSavePreset (true); return; }
            if (r == 10002) { safeThis->showLoadPreset(); return; }
            if (r >= 20001)
            {
                safeThis->processor.applyFactoryPreset (r - 20001);
                safeThis->refreshPresetDisplay (true);
                return;
            }
            if (r - 1 < files.size())
            {
                // A corrupt or foreign file is a documented no-op
                // (`parsePresetFile` refuses it before the undo bracket), and
                // the wrapper records the identity only on a successful apply
                // — so the indicator cannot claim a file is the active source
                // while the processor never moved.
                safeThis->processor.applyPresetFile (files.getReference (r - 1));
                safeThis->refreshPresetDisplay (true);
            }
        });
}

void AnabasisAudioProcessorEditor::showLoadPreset()
{
    auto dir = PresetManager::userPresetDirectory();
    dir.createDirectory();
    fileChooser = std::make_unique<juce::FileChooser> ("Load Preset", dir, "*.anabasis");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
        [safeThis = juce::Component::SafePointer<AnabasisAudioProcessorEditor> (this)]
        (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (safeThis != nullptr && f.existsAsFile())
            {
                // `existsAsFile()` is not the readability test;
                // `parsePresetFile` is, and it refuses a foreign root — a
                // failed apply moves nothing and records no identity.
                safeThis->processor.applyPresetFile (f);
                safeThis->refreshPresetDisplay (true);
            }
        });
}

void AnabasisAudioProcessorEditor::showSavePreset (bool show)
{
    savePresetBackdrop.setVisible (show);
    if (show)
    {
        saveNameEditor.setText (processor.currentPresetName(), juce::dontSendNotification);
        saveNameEditor.grabKeyboardFocus();
    }
}

void AnabasisAudioProcessorEditor::showAbout (bool show)
{
    aboutBackdrop.setVisible (show);
    aboutLink.setVisible (show);
}

void AnabasisAudioProcessorEditor::showSettings (bool show)
{
    settingsBackdrop.setVisible (show);
}

void AnabasisAudioProcessorEditor::applyTooltipsEnabled()
{
    tooltipsOn = tooltipsToggle.getToggleState();
    tooltips.setMillisecondsBeforeTipAppears (tooltipsOn ? 600 : 1000000000);
}

// ============================================================================
//  Micro-anim driver — lean adaptation of Anamorph F3: eases "hovA"/"actA"/
//  "onA"/"vpos" per display frame; repaints only what moved; the UI-animation
//  toggle snaps instead of easing (never disables function).
// ============================================================================
void AnabasisAudioProcessorEditor::registerAnimated (juce::Component& c)
{
    for (const auto& w : animated)
        if (w.comp == &c)
            return;

    AnimatedWidget w;
    w.comp   = &c;
    w.slider = dynamic_cast<juce::Slider*> (&c);
    w.toggle = dynamic_cast<juce::ToggleButton*> (&c);
    animated.add (w);

    // Only the hover/press pair here: they start at rest by definition. The
    // VALUE-derived properties (`vpos`, `onA`) cannot be seeded at this point
    // — `setupRotary`/`setupToggle` register the widget BEFORE `attachSlider`/
    // `attachToggle` construct the APVTS attachment, so the control still has
    // JUCE's default 0..10 range and value 0. Seeding here recorded exactly the
    // minimum this was meant to stop showing. `seedAnimatedFromValues()` does
    // it once, after every attachment exists.
    auto& props = c.getProperties();
    props.set ("hovA", 0.0);
    props.set ("actA", 0.0);
}

void AnabasisAudioProcessorEditor::seedAnimatedFromValues()
{
    // ONE pass over the registry, called at the end of the constructor — the
    // only point at which every attachment has run. `stepMicroAnims` eases
    // `vpos` toward the slider's real proportion and `onA` toward the toggle's
    // state; an unset `var` reads as 0.0, and `drawRotarySlider` prefers `vpos`
    // whenever the control is not being dragged, so an unseeded editor opened
    // with every knob sweeping up from its minimum and every ON toggle fading
    // in. `Knob::doReset` seeds the same `vpos` before a reset — that is the
    // case where the sweep IS wanted, and the reason this is a separate pass
    // rather than something the ease could do for itself.
    for (auto& w : animated)
    {
        auto& props = w.comp->getProperties();
        if (w.slider != nullptr)
            props.set ("vpos", (double) w.slider->valueToProportionOfLength (w.slider->getValue()));
        if (w.toggle != nullptr)
            props.set ("onA", w.toggle->getToggleState() ? 1.0 : 0.0);
    }
}

void AnabasisAudioProcessorEditor::stepMicroAnims (double dt)
{
    if (! isShowing())
        return;

    const double up   = 1.0 - std::pow (0.0001, dt * 3.2);   // fast in
    const double down = 1.0 - std::pow (0.01,   dt * 2.2);   // gentler out

    for (auto& w : animated)
    {
        auto& props = w.comp->getProperties();
        bool moved = false;

        auto ease = [&] (const char* key, double target)
        {
            const double v = (double) props[key];
            if (std::abs (v - target) < 1.0e-3)
            {
                if (! juce::exactlyEqual (v, target)) { props.set (key, target); moved = true; }
                return;
            }
            const double a = target > v ? up : down;
            props.set (key, uiAnimOn ? v + (target - v) * a : target);
            moved = true;
        };

        const bool over = w.comp->isMouseOver (true);
        const bool downNow = w.comp->isMouseButtonDown (true);
        ease ("hovA", over ? 1.0 : 0.0);
        ease ("actA", downNow ? 1.0 : 0.0);
        if (w.toggle != nullptr)
            ease ("onA", w.toggle->getToggleState() ? 1.0 : 0.0);
        if (w.slider != nullptr)
        {
            const double target = w.slider->valueToProportionOfLength (w.slider->getValue());
            // Track the live value directly while dragging; ease on jumps
            // (reset, preset, A/B) so the travel reads as one movement.
            if (w.slider->isMouseButtonDown())
            {
                if (! juce::exactlyEqual ((double) props["vpos"], target))
                { props.set ("vpos", target); moved = true; }
            }
            else
                ease ("vpos", target);
        }

        if (moved)
            w.comp->repaint();
    }
}
