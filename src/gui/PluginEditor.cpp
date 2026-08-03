#include "PluginEditor.h"
#include "../PluginProcessor.h"

using namespace abgui;

// Tooltips carry the parameter's registry display name plus any DESIGN-
// specified note; free prose is owner-supplied wording (C8) and is NOT
// invented here — the mechanism ships, the copy lands when specified.
static juce::String tidyTip (const juce::String& tip) { return tip.trim(); }

// The UI-scale step list, in ONE place: the combo builds its item list from it,
// `applyUiScale` maps the stored percent back through it, and the settings
// re-seed compares against it. Three private copies is how the three drift.
static constexpr int kScaleSteps[]  = { 80, 90, 100, 125, 150, 175, 200 };
static constexpr int kNumScaleSteps = (int) (sizeof (kScaleSteps) / sizeof (kScaleSteps[0]));

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
    : juce::AudioProcessorEditor (p), processor (p),
      animVBlank (this, [this]
      {
          const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
          const double dt  = juce::jlimit (0.0, 0.05, now - lastFrameTime);
          lastFrameTime = now;
          stepMicroAnims (dt);
      })
{
    setLookAndFeel (&lnf);
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

    abControl.getActive = [this] { return processor.activeSlotIndex(); };
    abControl.onToggle  = [this]
    {
        processor.switchToSlot (1 - processor.activeSlotIndex());
        abControl.repaint();
        refreshPresetDisplay();
    };
    addAndMakeVisible (abControl);
    registerAnimated (abControl);

    copyButton.onClick = [this] { processor.copySlotToOther(); };
    addAndMakeVisible (copyButton);
    registerAnimated (copyButton);

    undoButton.setButtonText (juce::String::charToString ((juce::juce_wchar) 0x21B6));
    redoButton.setButtonText (juce::String::charToString ((juce::juce_wchar) 0x21B7));
    undoButton.onClick = [this] { processor.undo(); refreshPresetDisplay(); };
    redoButton.onClick = [this] { processor.redo(); refreshPresetDisplay(); };
    addAndMakeVisible (undoButton);
    addAndMakeVisible (redoButton);
    registerAnimated (undoButton);
    registerAnimated (redoButton);

    settingsButton.onClick = [this] { showSettings (true); };
    addAndMakeVisible (settingsButton);
    registerAnimated (settingsButton);

    setupToggle (advancedToggle, pid::advancedMode, "ADV", paramName (pid::advancedMode));
    setupToggle (bypassToggle, pid::bypass, "BYPASS", paramName (pid::bypass));
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
        int idx = -1;
        for (int i = 0; i < factoryCount; ++i)
            if (processor.currentPresetName() == factory[i].name) { idx = i; break; }
        if (idx < 0)
            for (int i = 0; i < files.size(); ++i)
                if (files.getReference (i).getFileNameWithoutExtension()
                        == processor.currentPresetName())
                    { idx = factoryCount + i; break; }
        idx = (idx < 0 ? (dir > 0 ? 0 : total - 1) : (idx + dir + total) % total);
        if (idx < factoryCount)
            processor.applyFactoryPreset (idx);
        else
            processor.applyPresetFile (files.getReference (idx - factoryCount));
        refreshPresetDisplay();
    };
    presetPrev.onClick = [stepPreset] { stepPreset (-1); };
    presetNext.onClick = [stepPreset] { stepPreset (+1); };
    presetName.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (presetPrev);
    addAndMakeVisible (presetNext);
    addAndMakeVisible (presetName);

    // -- COMP panel ----------------------------------------------------------
    auto rotary = [this, &paramName] (Knob& k, juce::Label& l, const char* id)
    {
        setupRotary (k, l, paramName (id), paramName (id));
        attachSlider (k, id);
    };
    rotary (ratioK, ratioL, pid::compRatio);
    rotary (thresholdK, thresholdL, pid::compThreshold);
    rotary (attackK, attackL, pid::compAttack);
    rotary (releaseK, releaseL, pid::compRelease);
    rotary (kneeK, kneeL, pid::compKnee);
    rotary (compMixK, compMixL, pid::compMix);
    setupToggle (compAutoToggle, pid::compAutoRelease, "AUTO", paramName (pid::compAutoRelease));
    setupCombo (detectorBox, pid::compDetector, paramName (pid::compDetector));

    // -- CLIP / COLOUR panel -------------------------------------------------
    rotary (shapeK, shapeL, pid::clipShape);
    rotary (driveK, driveL, pid::clipDrive);
    rotary (clipMixK, clipMixL, pid::clipMix);
    rotary (balanceK, balanceL, pid::colourBalance);
    rotary (colToneK, colToneL, pid::colourTone);
    rotary (depthK, depthL, pid::colourDepth);
    rotary (dynTiltK, dynTiltL, pid::dynTilt);
    setupCombo (modelBox, pid::colourModel, paramName (pid::colourModel));

    // -- LIMITER panel -------------------------------------------------------
    rotary (limGainK, limGainL, pid::limGain);
    rotary (ceilingK, ceilingL, pid::ceiling);
    rotary (lookaheadK, lookaheadL, pid::lookahead);
    rotary (limReleaseK, limReleaseL, pid::limRelease);
    rotary (linkK, linkL, pid::stereoLink);
    rotary (preserveK, preserveL, pid::transientPreserve);
    setupToggle (limAutoToggle, pid::limAutoRelease, "AUTO", paramName (pid::limAutoRelease));
    setupToggle (tpToggle, pid::truePeakMode, "TP", paramName (pid::truePeakMode));
    setupCombo (styleBox, pid::limStyle, paramName (pid::limStyle));

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
    setupCombo (eqPosBox, pid::eqPosition, paramName (pid::eqPosition));

    // -- utility row ---------------------------------------------------------
    rotary (inputGainK, inputGainL, pid::inputGain);
    rotary (scHpfK, scHpfL, pid::scHpfFreq);
    setupCombo (ditherBox, pid::dither, paramName (pid::dither));
    setupToggle (shapingToggle, pid::ditherShaping, "SHAPE", paramName (pid::ditherShaping));
    setupToggle (compToggle, pid::loudnessComp, "COMP", paramName (pid::loudnessComp));
    setupToggle (deltaToggle, pid::deltaMonitor, "DELTA", paramName (pid::deltaMonitor));
    setupToggle (freezeToggle, pid::freeze, "FREEZE", paramName (pid::freeze));

    // -- macro row (read-only in Advanced, §6.3: the macros are driven from
    //    the Simple view; here they display position + detach badges) --------
    rotary (macroLoudnessK, macroLoudnessL, pid::loudness);
    rotary (macroCharacterK, macroCharacterL, pid::character);
    rotary (macroToneK, macroToneL, pid::tone);
    for (auto* k : { &macroLoudnessK, &macroCharacterK, &macroToneK })
    {
        k->setInterceptsMouseClicks (false, false);   // display only
        k->setAlpha (0.75f);
    }

    // -- Simple view (§6.2) --------------------------------------------------
    rotary (bigLoudnessK, bigLoudnessL, pid::loudness);
    rotary (simpleCharacterK, simpleCharacterL, pid::character);
    rotary (simpleToneK, simpleToneL, pid::tone);
    rotary (simpleCeilingK, simpleCeilingL, pid::ceiling);
    bigLoudnessL.setFont (juce::Font (juce::FontOptions (13.5f)).withExtraKerningFactor (0.2f));

    setupToggleInternal (ceilingLockToggle, "LOCK", "Ceiling lock",
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
    addAndMakeVisible (*grView);
    addChildComponent (*spectrumView);   // Advanced strip only; int_spectrumOn gates

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
    settingsRow (offlineLabel,    "Offline render");
    settingsRow (uiScaleLabel,    "UI scale");

    setupComboInternal (oversampleBox, { "Off", "2x", "4x", "8x", "16x" },
                        "Oversampling", ist.getPropertyAsValue (iid::oversample, nullptr));
    // DESIGN §6.4 specifies the latency note lives in this control's tooltip.
    setupComboInternal (phaseBox, { "Minimum", "Linear" },
                        "Phase. Linear phase adds latency (reported to the host).",
                        ist.getPropertyAsValue (iid::osPhase, nullptr));
    setupComboInternal (offlineBox, { "Follow", "Force Max" },
                        "Offline render", ist.getPropertyAsValue (iid::offlineQuality, nullptr));
    // int_uiScale stores PERCENT; the combo maps index<->percent through the
    // step list, so the stored value stays meaningful outside this editor.
    uiScaleBox.addItemList ({ "80%", "90%", "100%", "125%", "150%", "175%", "200%" }, 1);
    {
        const int pct = (int) ist.getProperty (iid::uiScale, 100);
        int idx = 2;
        for (int i = 0; i < kNumScaleSteps; ++i)
            if (kScaleSteps[i] == pct) idx = i;
        uiScaleBox.setSelectedItemIndex (idx, juce::dontSendNotification);
    }
    uiScaleBox.onChange = [this, &ist]
    {
        ist.setProperty (iid::uiScale,
                         kScaleSteps[juce::jlimit (0, kNumScaleSteps - 1,
                                                   uiScaleBox.getSelectedItemIndex())],
                         nullptr);
        applyUiScale();
    };
    passComboHoverThrough (uiScaleBox);
    allCombos.add (&uiScaleBox);
    settingsBackdrop.addAndMakeVisible (uiScaleBox);
    for (auto* b : { &oversampleBox, &phaseBox, &offlineBox })
    {
        removeChildComponent (b);
        settingsBackdrop.addAndMakeVisible (b);
    }

    setupToggleInternal (animToggle, "UI animation", "UI animation",
                         ist.getPropertyAsValue (iid::uiAnimations, nullptr));
    setupToggleInternal (tooltipsToggle, "Tooltips", "Tooltips",
                         ist.getPropertyAsValue (iid::tooltipsOn, nullptr));
    setupToggleInternal (tpMeterToggle, "True-peak meter", "True-peak meter",
                         ist.getPropertyAsValue (iid::tpMeterOn, nullptr));
    setupToggleInternal (spectrumToggle, "Spectrum", "Spectrum",
                         ist.getPropertyAsValue (iid::spectrumOn, nullptr));
    // Target-line selection (§6.4): three bits of int_meterTargets, in the
    // LoudnessMeterView::kTargets order. Platform names are identifiers, not
    // invented prose.
    {
        const char* names[] = { "Spotify", "Apple Music", "YouTube" };
        juce::ToggleButton* bits[] = { &targetSpToggle, &targetApToggle, &targetYtToggle };
        const int mask = (int) ist.getProperty (iid::meterTargets, ~0);
        for (int t = 0; t < 3; ++t)
        {
            auto* tog = bits[t];
            tog->setButtonText (names[t]);
            tog->setToggleState ((mask & (1 << t)) != 0, juce::dontSendNotification);
            tog->onStateChange = [this, t, tog]
            {
                auto& tree = processor.internalState.state();
                int m2 = (int) tree.getProperty (iid::meterTargets, ~0);
                m2 = tog->getToggleState() ? (m2 | (1 << t)) : (m2 & ~(1 << t));
                tree.setProperty (iid::meterTargets, m2, nullptr);
            };
            settingsBackdrop.addAndMakeVisible (*tog);
            registerAnimated (*tog);
        }
    }
    for (auto* t : { &animToggle, &tooltipsToggle, &tpMeterToggle, &spectrumToggle })
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
    savePresetBackdrop.addAndMakeVisible (saveNameEditor);
    saveOkButton.onClick = [this]
    {
        const auto name = juce::File::createLegalFileName (saveNameEditor.getText().trim());
        if (name.isEmpty())
            return;
        auto dir = PresetManager::userPresetDirectory();
        dir.createDirectory();
        if (processor.savePresetFile (dir.getChildFile (name + ".anabasis")))
        {
            showSavePreset (false);
            refreshPresetDisplay();
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
    applyTooltipsEnabled();

    startTimerHz (24);
    refreshPresetDisplay();
    updateModeVisibility();
    applyUiScale();

#if JUCE_MAC || JUCE_WINDOWS
    glContext.attachTo (*this);    // §6.1: GPU compositing on these two only
#endif
}

AnabasisAudioProcessorEditor::~AnabasisAudioProcessorEditor()
{
#if JUCE_MAC || JUCE_WINDOWS
    glContext.detach();
#endif
    processor.apvts.removeParameterListener (pid::advancedMode, this);
    processor.apvts.removeParameterListener (pid::bypass, this);
    setLookAndFeel (nullptr);
}

// ============================================================================
//  Setup helpers (family grammar — provenance in the header)
// ============================================================================
void AnabasisAudioProcessorEditor::setupRotary (juce::Slider& s, juce::Label& l,
                                                const juce::String& name, const juce::String& tip)
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
    // control carries its registry name as title/description, so a screen
    // reader announces the same wording the automation lane shows.
    s.setTitle (name);
    s.setDescription (name);
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
    if (auto* cp = processor.apvts.getParameter (id))
        box.addItemList (cp->getAllValueStrings(), 1);
    box.setTooltip (tidyTip (tip));
    box.setRepaintsOnMouseActivity (true);
    passComboHoverThrough (box);
    allCombos.add (&box);
    addAndMakeVisible (box);
    comboAtts.add (new ComboBoxAttachment (processor.apvts, id, box));
    registerAnimated (box);
    box.setTitle (tip);
}

void AnabasisAudioProcessorEditor::setupToggle (juce::ToggleButton& t, const char* id,
                                                const juce::String& text, const juce::String& tip)
{
    t.setButtonText (text);
    if (tip.isNotEmpty()) t.setTooltip (tidyTip (tip));
    addAndMakeVisible (t);
    buttonAtts.add (new ButtonAttachment (processor.apvts, id, t));
    registerAnimated (t);
    t.setTitle (tip.isNotEmpty() ? tip : text);
}

void AnabasisAudioProcessorEditor::setupComboInternal (juce::ComboBox& box,
                                                       const juce::StringArray& items,
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
    box.onChange = [&box, value]() mutable
    { value.setValue (juce::jmax (0, box.getSelectedItemIndex())); };
    registerAnimated (box);
}

void AnabasisAudioProcessorEditor::setupToggleInternal (juce::ToggleButton& t,
                                                        const juce::String& text,
                                                        const juce::String& tip, juce::Value value)
{
    t.setButtonText (text);
    t.setTooltip (tidyTip (tip));
    addAndMakeVisible (t);
    t.getToggleStateValue().referTo (value);
    registerAnimated (t);
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
        const int macroTop = utilTop + kUtilityH;
        const int meterTop = macroTop + kMacroRowH;
        auto util  = juce::Rectangle<int> (0, utilTop, getWidth(), kUtilityH).toFloat().reduced (8.0f, 4.0f);
        auto macro = juce::Rectangle<int> (0, macroTop, getWidth(), kMacroRowH).toFloat().reduced (8.0f, 4.0f);
        auto meter = juce::Rectangle<int> (0, meterTop, getWidth(), kMeterRowH).toFloat().reduced (8.0f, 6.0f);
        g.setColour (colours::bgPanel.withAlpha (0.45f));
        g.fillRoundedRectangle (util, 10.0f);
        g.fillRoundedRectangle (macro, 10.0f);
        glass::fillPanel (g, meter, 10.0f, colours::bgPanel.withAlpha (0.5f), 0.8f);

        // §5.3 / §6.3: per-parameter detach badges — an accent dot on each
        // control the user has taken off its macro curve.
        const std::pair<const char*, const juce::Slider*> badged[] = {
            { "limGain", &limGainK },   { "compThreshold", &thresholdK },
            { "compRatio", &ratioK },   { "clipDrive", &driveK },
            { "clipShape", &shapeK },   { "colourDepth", &depthK },
            { "dynTilt", &dynTiltK },   { "eqTilt", &eqTiltK },
            { "colourTone", &colToneK },
        };
        g.setColour (colours::accent);
        for (const auto& [id, k] : badged)
            if (processor.detachMask().contains (juce::String (id)))
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
    abControl.setBounds (r.removeFromRight (64));

    // Preset browser fills between the wordmark and the right cluster.
    r.removeFromLeft (352 - r.getX());
    presetPrev.setBounds (r.removeFromLeft (24));
    presetNext.setBounds (r.removeFromRight (24));
    presetName.setBounds (r.reduced (2, 0));

    dimOverlay.setBounds (getLocalBounds().withTrimmedTop (kBarH));

    for (auto* b : { &aboutBackdrop, &settingsBackdrop, &savePresetBackdrop })
        b->setBounds (getLocalBounds());
    aboutBackdrop.panel = getLocalBounds().withSizeKeepingCentre (400, 232);
    aboutLink.setBounds (aboutBackdrop.panel.withTrimmedTop (176).withTrimmedBottom (24)
                             .withSizeKeepingCentre (180, 20));
    settingsBackdrop.panel = getLocalBounds().withSizeKeepingCentre (380, 398);
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
        splitRow (row(), uiScaleLabel,    uiScaleBox);
        animToggle.setBounds (row (26));
        tooltipsToggle.setBounds (row (26));
        tpMeterToggle.setBounds (row (26));
        spectrumToggle.setBounds (row (26));
        auto tr = row (26);
        targetSpToggle.setBounds (tr.removeFromLeft (110));
        targetApToggle.setBounds (tr.removeFromLeft (130));
        targetYtToggle.setBounds (tr);
    }
    {
        auto sp = savePresetBackdrop.panel.reduced (20, 16);
        saveTitle.setBounds (sp.removeFromTop (24));
        sp.removeFromTop (8);
        saveNameEditor.setBounds (sp.removeFromTop (28));
        sp.removeFromTop (10);
        auto btns = sp.removeFromTop (26);
        saveCancelButton.setBounds (btns.removeFromLeft (btns.getWidth() / 2).reduced (4, 0));
        saveOkButton.setBounds (btns.reduced (4, 0));
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

    {   // COMP
        auto a = panel (0);
        placeRow (a, { { &ratioK, &ratioL }, { &thresholdK, &thresholdL } });
        placeRow (a, { { &attackK, &attackL }, { &releaseK, &releaseL } });
        placeRow (a, { { &kneeK, &kneeL }, { &compMixK, &compMixL } });
        auto row = a.removeFromTop (26);
        compAutoToggle.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (2, 2));
        detectorBox.setBounds (row.reduced (2, 2));
        a.removeFromTop (8);
        compGrMeter.setBounds (a.removeFromTop (14));   // [GR meter] — this stage's own
    }
    {   // CLIP / COLOUR
        auto a = panel (1);
        placeRow (a, { { &shapeK, &shapeL }, { &driveK, &driveL } });
        placeRow (a, { { &clipMixK, &clipMixL }, { &depthK, &depthL } });
        placeRow (a, { { &balanceK, &balanceL }, { &colToneK, &colToneL } });
        auto row = a.removeFromTop (84);
        placeRow (row, { { &dynTiltK, &dynTiltL } });
        auto boxRow = a.removeFromTop (26);
        modelBox.setBounds (boxRow.reduced (2, 2));
        a.removeFromTop (6);
        clipCurve->setBounds (a.removeFromTop (juce::jmax (40, a.getHeight())));  // [live curve]
    }
    {   // LIMITER
        auto a = panel (2);
        placeRow (a, { { &limGainK, &limGainL }, { &ceilingK, &ceilingL } });
        placeRow (a, { { &lookaheadK, &lookaheadL }, { &limReleaseK, &limReleaseL } });
        placeRow (a, { { &linkK, &linkL }, { &preserveK, &preserveL } });
        auto row = a.removeFromTop (26);
        limAutoToggle.setBounds (row.removeFromLeft (56).reduced (2, 2));
        tpToggle.setBounds (row.removeFromLeft (48).reduced (2, 2));
        styleBox.setBounds (row.reduced (2, 2));
        a.removeFromTop (8);
        limGrMeter.setBounds (a.removeFromTop (14));    // [GR meter] — this stage's own
    }
    {   // EQ (the densest panel: three-across rows, smaller cells)
        auto a = panel (3);
        auto boxRow = a.removeFromTop (24);
        eqPosBox.setBounds (boxRow.removeFromRight (boxRow.getWidth() / 2).reduced (2, 1));
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
        auto left = util.removeFromLeft (260);
        auto cell = left.removeFromLeft (120);
        inputGainL.setBounds (cell.removeFromBottom (12));
        inputGainK.setBounds (cell);
        cell = left.removeFromLeft (120);
        scHpfL.setBounds (cell.removeFromBottom (12));
        scHpfK.setBounds (cell);

        auto right = util.removeFromRight (300);
        auto toggles = right;
        compToggle.setBounds (toggles.removeFromLeft (92).reduced (2, 14));
        deltaToggle.setBounds (toggles.removeFromLeft (92).reduced (2, 14));
        freezeToggle.setBounds (toggles.removeFromLeft (100).reduced (2, 14));

        auto mid = util.reduced (12, 14);
        ditherBox.setBounds (mid.removeFromLeft (110));
        mid.removeFromLeft (8);
        shapingToggle.setBounds (mid.removeFromLeft (90));
    }

    // Macro row: read-only L / C / T (badges land with the §5.3 grammar).
    auto macro = juce::Rectangle<int> (0, kBarH + kPanelRowH + kUtilityH, getWidth(), kMacroRowH)
                     .reduced (16, 2);
    {
        const int w = macro.getWidth() / 3;
        auto cell = [&macro, w]() { return macro.removeFromLeft (w).reduced (60, 0); };
        auto place = [] (juce::Rectangle<int> c, Knob& k, juce::Label& l)
        {
            l.setBounds (c.removeFromBottom (12));
            k.setBounds (c);
        };
        place (cell(), macroLoudnessK, macroLoudnessL);
        place (cell(), macroCharacterK, macroCharacterL);
        place (cell(), macroToneK, macroToneL);
    }

    // §6.3 shared metering strip: GR history left · spectrum middle
    // (dismissible — GR widens into its share when it is off) · loudness
    // block right.
    auto strip = juce::Rectangle<int> (0, kBarH + kPanelRowH + kUtilityH + kMacroRowH,
                                       getWidth(), kMeterRowH).reduced (8, 6);
    meterView->setBounds (strip.removeFromRight (300));
    const bool spectrumOn = (bool) processor.internalState.state()
                                .getProperty (iid::spectrumOn, true);
    spectrumView->setVisible (spectrumOn);
    if (spectrumOn)
        spectrumView->setBounds (strip.removeFromRight (strip.getWidth() / 2));
    grView->setBounds (strip);

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
    ceilingLockToggle.setBounds (ceilCell.removeFromRight (54).withSizeKeepingCentre (52, 20));
    place (ceilCell, simpleCeilingK, simpleCeilingL);

    // Toggle row: [Loudness Comp] [Delta] [Freeze] [Learn] + out LUFS.
    auto toggles = left.removeFromTop (40).reduced (24, 4);
    compToggle.setBounds (toggles.removeFromLeft (96));
    deltaToggle.setBounds (toggles.removeFromLeft (86));
    freezeToggle.setBounds (toggles.removeFromLeft (94));
    learnButton.setBounds (toggles.removeFromLeft (78).reduced (0, 2));
    outLufsValue.setBounds (toggles.removeFromRight (72));
    outLufsCaption.setBounds (toggles.removeFromRight (70));

    // §6.2 wells: the right meter panel and the bottom GR strip.
    spectrumView->setVisible (false);    // §6.2: the Simple strip is GR-only
    meterView->setBounds (juce::Rectangle<int> (getWidth() - 300, kBarH + 8,
                                                292, kSimpleH - kBarH - 128 - 16));
    grView->setBounds (juce::Rectangle<int> (0, kSimpleH - 120, getWidth(), 120)
                           .reduced (8, 6));
}

// ============================================================================
//  Mode / overlays / cadence
// ============================================================================
void AnabasisAudioProcessorEditor::updateModeVisibility()
{
    advanced = processor.apvts.getRawParameterValue (pid::advancedMode)->load() >= 0.5f;

    const bool adv = advanced;
    juce::Component* advOnly[] = {
        &ratioK, &thresholdK, &attackK, &releaseK, &kneeK, &compMixK,
        &ratioL, &thresholdL, &attackL, &releaseL, &kneeL, &compMixL,
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
        &ditherBox, &shapingToggle,
        &macroLoudnessK, &macroCharacterK, &macroToneK,
        &macroLoudnessL, &macroCharacterL, &macroToneL,
    };
    for (auto* c : advOnly)
        c->setVisible (adv);

    for (juce::Component* c : { (juce::Component*) clipCurve.get(), (juce::Component*) eqCurve.get(),
                                (juce::Component*) &compGrMeter, (juce::Component*) &limGrMeter })
        c->setVisible (adv);

    juce::Component* simpleOnly[] = {
        &bigLoudnessK, &bigLoudnessL, &simpleCharacterK, &simpleCharacterL,
        &simpleToneK, &simpleToneL, &simpleCeilingK, &simpleCeilingL,
        &ceilingLockToggle, &learnButton, &outLufsCaption, &outLufsValue,
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

    // uiScale is the same shape with one extra step: the box only DISPLAYS the
    // percent, so a stored change has to reach `applyUiScale()` as well or the
    // panel would read 150 % while the window stayed at 100 %.
    const int pct = (int) ist.getProperty (iid::uiScale, 100);
    int wantScaleIdx = uiScaleBox.getSelectedItemIndex();
    for (int i = 0; i < kNumScaleSteps; ++i)
        if (kScaleSteps[i] == pct)
            wantScaleIdx = i;
    if (wantScaleIdx != uiScaleBox.getSelectedItemIndex())
    {
        uiScaleBox.setSelectedItemIndex (wantScaleIdx, juce::dontSendNotification);
        applyUiScale();
    }
}

void AnabasisAudioProcessorEditor::applyUiScale()
{
    const int pct = (int) processor.internalState.state().getProperty (iid::uiScale, 100);
    float scale = 1.0f;
    for (int i = 0; i < kNumScaleSteps; ++i)
        if (kScaleSteps[i] == pct)
            scale = (float) pct / 100.0f;

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
    // Both ids this listens to — advancedMode and bypass — are AUTOMATABLE, and
    // APVTS delivers on whichever thread wrote the value, so a host automating
    // Bypass calls this FROM THE AUDIO THREAD. `triggerAsyncUpdate()` posts to
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
    if (undoButton.isEnabled() != processor.canUndo())
        undoButton.setEnabled (processor.canUndo());
    if (redoButton.isEnabled() != processor.canRedo())
        redoButton.setEnabled (processor.canRedo());

    // -- Advanced panel wells: per-stage GR + curve refreshes ----------------
    if (advanced)
    {
        compGrMeter.setGrDb (processor.meterCompGrDb());
        limGrMeter.setGrDb (processor.meterGrDb());
        clipCurve->refresh();
        eqCurve->refresh();
    }

    // -- spectrum visibility follows int_spectrumOn (dismiss / Settings) -----
    {
        const bool on = (bool) processor.internalState.state()
                            .getProperty (iid::spectrumOn, true);
        if (on != shownSpectrumOn)
        {
            shownSpectrumOn = on;
            resized();                       // the strip re-partitions
            repaint();
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

void AnabasisAudioProcessorEditor::refreshPresetDisplay()
{
    auto name = processor.currentPresetName();
    if (name.isEmpty())
        name = "Preset";               // §6.2 wireframe placeholder (C8: owner wording TODO)
    // The dirty compare is a full slot-tree equivalence — throttle it to
    // every 8th tick (~3 Hz) and reuse the last answer between.
    if (++dirtyPollDivider >= 8)
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
    m.setLookAndFeel (&lnf);
    int factoryCount = 0;
    const auto* factory = PresetManager::factoryPresets (factoryCount);
    m.addSectionHeader ("FACTORY");
    for (int i = 0; i < factoryCount; ++i)
        m.addItem (20001 + i, factory[i].name, true,
                   processor.currentPresetName() == factory[i].name);
    if (! files.isEmpty())
    {
        m.addSectionHeader ("USER");
        for (int i = 0; i < files.size(); ++i)
            m.addItem (i + 1, files.getReference (i).getFileNameWithoutExtension(), true,
                       files.getReference (i).getFileNameWithoutExtension()
                           == processor.currentPresetName());
    }
    const juce::String ellip = juce::String::charToString ((juce::juce_wchar) 0x2026);
    m.addSeparator();
    m.addItem (10001, "Save Preset" + ellip);
    m.addItem (10002, "Load Preset" + ellip);

    m.showMenuAsync (juce::PopupMenu::Options()
                         .withTargetComponent (presetName)
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
                safeThis->refreshPresetDisplay();
                return;
            }
            if (r - 1 < files.size())
            {
                safeThis->processor.applyPresetFile (files.getReference (r - 1));
                safeThis->refreshPresetDisplay();
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
                safeThis->processor.applyPresetFile (f);
                safeThis->refreshPresetDisplay();
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

    auto& props = c.getProperties();
    props.set ("hovA", 0.0);
    props.set ("actA", 0.0);
    if (w.toggle != nullptr)
        props.set ("onA", w.toggle->getToggleState() ? 1.0 : 0.0);
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
