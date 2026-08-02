#include "PluginEditor.h"
#include "../PluginProcessor.h"

using namespace abgui;

// Tooltips carry the parameter's registry display name plus any DESIGN-
// specified note; free prose is owner-supplied wording (C8) and is NOT
// invented here — the mechanism ships, the copy lands when specified.
static juce::String tidyTip (const juce::String& tip) { return tip.trim(); }

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
        auto dir_ = PresetManager::userPresetDirectory();
        auto files = dir_.findChildFiles (juce::File::findFiles, false, "*.anabasis");
        files.sort();
        if (files.isEmpty())
            return;
        int idx = -1;
        for (int i = 0; i < files.size(); ++i)
            if (files.getReference (i).getFileNameWithoutExtension() == processor.currentPresetName())
                { idx = i; break; }
        idx = (idx < 0 ? (dir > 0 ? 0 : files.size() - 1)
                       : (idx + dir + files.size()) % files.size());
        processor.applyPresetFile (files.getReference (idx));
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
    static constexpr int kScaleSteps[] = { 80, 90, 100, 125, 150, 175, 200 };
    {
        const int pct = (int) ist.getProperty (iid::uiScale, 100);
        int idx = 2;
        for (int i = 0; i < 7; ++i)
            if (kScaleSteps[i] == pct) idx = i;
        uiScaleBox.setSelectedItemIndex (idx, juce::dontSendNotification);
    }
    uiScaleBox.onChange = [this, &ist]
    {
        ist.setProperty (iid::uiScale,
                         kScaleSteps[juce::jlimit (0, 6, uiScaleBox.getSelectedItemIndex())],
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
    for (auto* t : { &animToggle, &tooltipsToggle, &tpMeterToggle })
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
}

void AnabasisAudioProcessorEditor::setupToggle (juce::ToggleButton& t, const char* id,
                                                const juce::String& text, const juce::String& tip)
{
    t.setButtonText (text);
    if (tip.isNotEmpty()) t.setTooltip (tidyTip (tip));
    addAndMakeVisible (t);
    buttonAtts.add (new ButtonAttachment (processor.apvts, id, t));
    registerAnimated (t);
}

void AnabasisAudioProcessorEditor::setupComboInternal (juce::ComboBox& box,
                                                       const juce::StringArray& items,
                                                       const juce::String& tip, juce::Value value)
{
    box.addItemList (items, 1);
    box.setTooltip (tidyTip (tip));
    box.setRepaintsOnMouseActivity (true);
    passComboHoverThrough (box);
    allCombos.add (&box);
    addAndMakeVisible (box);
    box.getSelectedIdAsValue().referTo (value);
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
    settingsBackdrop.panel = getLocalBounds().withSizeKeepingCentre (380, 330);
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
    }
    {   // EQ (the densest panel: three-across rows, smaller cells)
        auto a = panel (3);
        auto boxRow = a.removeFromTop (24);
        eqPosBox.setBounds (boxRow.removeFromRight (boxRow.getWidth() / 2).reduced (2, 1));
        placeRow (a, { { &eqTiltK, &eqTiltL }, { &lsFreqK, &lsFreqL }, { &lsGainK, &lsGainL } }, 78);
        placeRow (a, { { &hsFreqK, &hsFreqL }, { &hsGainK, &hsGainL }, { &b1FreqK, &b1FreqL } }, 78);
        placeRow (a, { { &b1GainK, &b1GainL }, { &b1QK, &b1QL }, { &b2FreqK, &b2FreqL } }, 78);
        placeRow (a, { { &b2GainK, &b2GainL }, { &b2QK, &b2QL } }, 78);
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

    juce::ignoreUnused (body);
}

void AnabasisAudioProcessorEditor::layoutSimple (juce::Rectangle<int> body)
{
    // The Simple controls (big knob, macro row, toggle row) land with the
    // §6.2 pass; the frame, meter panel well and GR strip well are placed by
    // paint(). Hide the Advanced-only controls meanwhile.
    juce::ignoreUnused (body);
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
        &ditherBox, &shapingToggle, &compToggle, &deltaToggle, &freezeToggle,
        &macroLoudnessK, &macroCharacterK, &macroToneK,
        &macroLoudnessL, &macroCharacterL, &macroToneL,
    };
    for (auto* c : advOnly)
        c->setVisible (adv);
}

void AnabasisAudioProcessorEditor::applyUiScale()
{
    static constexpr int kScaleSteps[] = { 80, 90, 100, 125, 150, 175, 200 };
    const int pct = (int) processor.internalState.state().getProperty (iid::uiScale, 100);
    float scale = 1.0f;
    for (int i = 0; i < 7; ++i)
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
    triggerAsyncUpdate();   // may arrive off the message thread (APVTS rule)
}

void AnabasisAudioProcessorEditor::handleAsyncUpdate()
{
    updateModeVisibility();
    applyUiScale();
    dimOverlay.setVisible (processor.apvts.getRawParameterValue (pid::bypass)->load() >= 0.5f);
    repaint();
}

void AnabasisAudioProcessorEditor::timerCallback()
{
    refreshPresetDisplay();
    const bool bypassed = processor.apvts.getRawParameterValue (pid::bypass)->load() >= 0.5f;
    if (bypassed != dimOverlay.isVisible())
        dimOverlay.setVisible (bypassed);
}

void AnabasisAudioProcessorEditor::refreshPresetDisplay()
{
    auto name = processor.currentPresetName();
    if (name.isEmpty())
        name = "Preset";               // §6.2 wireframe placeholder (C8: owner wording TODO)
    if (name != presetShownName)
    {
        presetShownName = name;
        presetName.setButtonText (name);
    }
}

void AnabasisAudioProcessorEditor::showPresetMenu()
{
    auto dir = PresetManager::userPresetDirectory();
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.anabasis");
    files.sort();

    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
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
        [this, files] (int r)
        {
            if (r == 0) return;
            if (r == 10001) { showSavePreset (true); return; }
            if (r == 10002) { showLoadPreset(); return; }
            if (r - 1 < files.size())
            {
                processor.applyPresetFile (files.getReference (r - 1));
                refreshPresetDisplay();
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
        [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f.existsAsFile())
            {
                processor.applyPresetFile (f);
                refreshPresetDisplay();
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
