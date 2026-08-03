#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#if JUCE_MAC || JUCE_WINDOWS
 #include <juce_opengl/juce_opengl.h>
#endif
#include "LookAndFeel.h"
#include "LoudnessMeterView.h"
#include "GrHistoryView.h"
#include "SpectrumView.h"
#include "CurveView.h"
#include <array>

class AnabasisAudioProcessor;

// ============================================================================
//  AnabasisAudioProcessorEditor — the P5 editor (DESIGN §6, ADR-0009).
//
//  Frame grammar is Anamorph's (46 px top bar, Backdrop overlays, glass
//  panels, whole-window scale compose); the content is Anabasis's: the §6.3
//  Advanced zones (COMP · CLIP/COLOUR · LIMITER · EQ + utility and macro
//  rows + the shared bottom metering strip) and the §6.2 Simple view.
//  Provenance (ADR-0009): the Backdrop/DimLayer/ABControl/Knob structs and
//  the setup*/attach* helpers are adapted from Anamorph
//  src/PluginEditor.h:36-175 / .cpp:672-800 @ b6a3db8.
//
//  Threading: everything here is message-thread (or the paint context); the
//  processor is reached ONLY through APVTS attachments, InternalState
//  juce::Values, the published meter getters, and the momentary-request
//  methods — the THREADING_POLICY rows, nothing else.
//
//  Geometry: re-derived at P5 from the real control inventory (§6.2's
//  instruction) and landing on the family frame — the §6.3 zones fit the
//  Anamorph-sized 940-wide grid with the EQ panel (12 controls, the densest)
//  setting the panel-row height. Constants live HERE and nowhere else:
//  paint() and resized() both depend on them, and splitting them is how the
//  sibling product says they drift.
//
//  OpenGL: attached on macOS/Windows, NEVER on Linux/X11 (DESIGN §6.1,
//  ADR-0011 note) — on Linux this class has no GL member at all.
// ============================================================================

class AnabasisAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer,
                                     private juce::AudioProcessorValueTreeState::Listener,
                                     private juce::AsyncUpdater
{
public:
    explicit AnabasisAudioProcessorEditor (AnabasisAudioProcessor&);
    ~AnabasisAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    // Host DPI scale is COMPOSED with the user UI-scale instead of letting
    // JUCE's default overwrite our transform (the Windows window-size bug the
    // sibling product fixed — provenance header above).
    void setScaleFactor (float newScale) override;

    // The Settings panel's state→widget direction, run on the 24 Hz tick.
    // PUBLIC only so the headless suite can drive it: no message loop runs
    // there, so the tick never fires, and this direction has now been the
    // missing half TWICE — the three combos plus `uiScaleBox` (round 26) and
    // the three §6.4 target checkboxes (round 32). A direction nothing can
    // call is a direction nothing can guard. The implementation comment
    // carries the reasoning.
    void refreshInternalSettingsBoxes();

private:
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // Translucent modal backdrop hosting a centred panel (About / Settings /
    // Save preset). Click outside (or anywhere, for About) dismisses.
    struct Backdrop : public juce::Component
    {
        std::function<void()> onDismiss;
        juce::Rectangle<int>  panel;
        bool aboutText  = false;
        bool dropShadow = false;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (aboutText || ! panel.contains (e.getPosition()))
                if (onDismiss) onDismiss();
        }
    };

    // Bypass dim layer: painted on top, never blocks the mouse.
    struct DimLayer : public juce::Component
    {
        void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0x66090b0e)); }
    };

    // A/B control: "A / B", active letter bright, one click toggles.
    struct ABControl : public juce::Component, public juce::SettableTooltipClient
    {
        std::function<int()>  getActive;
        std::function<void()> onToggle;
        bool hovered = false;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override { if (onToggle) onToggle(); }
        void mouseEnter (const juce::MouseEvent&) override { hovered = true;  repaint(); }
        void mouseExit  (const juce::MouseEvent&) override { hovered = false; repaint(); }
    };

    // A slider that resets to its default on Alt-click / double-click, with
    // the reset gesture-wrapped so it lands in host automation as ONE edit
    // (and so the §5.3 discriminator sees a REAL gesture when that lands).
    struct Knob : public juce::Slider
    {
        double resetValue = 0.0;
        juce::RangedAudioParameter* resetParam = nullptr;
        void doReset()
        {
            getProperties().set ("vpos", (double) valueToProportionOfLength (getValue()));
            setValue (resetValue, juce::sendNotificationSync);
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isAltDown())
            {
                // Deliberately WITHOUT `Slider::mouseDown (e)`: the alt-click is
                // a complete gesture on its own, opened and closed here. The
                // accepted consequence, stated because it looks like an
                // omission — the slider never enters a drag, so an
                // alt-PRESS-AND-DRAG is inert rather than dragging on from the
                // reset value. JUCE tolerates it (its pimpl guards every later
                // `mouseDrag`/`mouseUp` on the drag object this never creates),
                // and the pattern is inherited from Anamorph (ADR-0009). Making
                // it drag would mean opening a second gesture inside the one
                // just closed, which is the shape §5.3/§7 spent this PR
                // untangling — a listening-pass call, not a repair.
                if (resetParam != nullptr) resetParam->beginChangeGesture();
                doReset();
                if (resetParam != nullptr) resetParam->endChangeGesture();
                return;
            }
            juce::Slider::mouseDown (e);
        }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            // BRACKETED, exactly like the alt-click path above: the two are the
            // same gesture as far as the user is concerned, and since P6 the
            // bracket decides two things — whether the reset becomes an undo
            // step (§7 keys on a completed message-thread drag) and whether a
            // managed parameter DETACHES from its macro (§5.3 keys on
            // "gesture-bracketed"). Unbracketed, a double-click reset silently
            // did neither, so the next macro pass took the value straight back
            // while an alt-click reset held.
            //
            // It does NOT nest inside the SliderAttachment's own drag gesture,
            // and two review rounds reached opposite conclusions about that, so
            // here is the answer with its citation: JUCE dispatches
            // `mouseDoubleClick` from `Component::internalMouseUp`, AFTER
            // `target->mouseUp (me)` (juce_Component.cpp, the "check for
            // double-click" block — read from the pinned tree, not from
            // memory). By then `Slider::mouseUp` has destroyed `currentDrag`
            // and the attachment has closed its gesture — which is also why
            // stock `Slider::mouseDoubleClick` can construct its own
            // `DragInProgress`. The host therefore sees one balanced
            // begin/end pair and `jassert (! isPerformingGesture)` cannot fire.
            // Exactly two: JUCE reports 3, 4 … for a rapid multi-click, and a
            // triple-click's third click therefore resets nothing. Matches
            // stock `Slider::mouseDoubleClick`, which is likewise reached only
            // on the double, and keeps one reset per intent instead of one per
            // click after the second.
            if (e.getNumberOfClicks() != 2)
                return;
            if (resetParam != nullptr) resetParam->beginChangeGesture();
            doReset();
            if (resetParam != nullptr) resetParam->endChangeGesture();
        }
    };

    // -- plumbing ------------------------------------------------------------
    void timerCallback() override;
    void parameterChanged (const juce::String&, float) override;  // advancedMode / bypass
    void handleAsyncUpdate() override;                            // → mode/dim refresh
    // Raised by parameterChanged when it may NOT post (an automatable id can be
    // delivered on the audio thread); consumed by the 24 Hz tick. See both.
    std::atomic<bool> uiRefreshPending { false };
    void applyUiScale();
    void updateModeVisibility();
    void layoutAdvanced (juce::Rectangle<int> body);
    void layoutSimple   (juce::Rectangle<int> body);
    // `recomputeNow`: skip the ~3 Hz throttle on the dirty compare. Every
    // caller that just CHANGED the state the mark describes passes true; the
    // 24 Hz tick is the only one that does not.
    void refreshPresetDisplay (bool recomputeNow = false);
    void showPresetMenu();
    void showSavePreset (bool);
    void showAbout (bool);
    void showSettings (bool);
    void applyTooltipsEnabled();
    void stepMicroAnims (double dt);
    void registerAnimated (juce::Component&);
    // Seeds the VALUE-derived animation properties (`vpos`, `onA`) for every
    // registered widget. Separate from `registerAnimated` because the setup
    // helpers register BEFORE the APVTS attachments run: at registration a
    // control still carries JUCE's default range and value.
    void seedAnimatedFromValues();

    void setupRotary (juce::Slider&, juce::Label&, const juce::String& name,
                      const juce::String& tip);
    void attachSlider (juce::Slider&, const char* id);
    void setupCombo (juce::ComboBox&, const char* id, const juce::String& tip);
    void setupToggle (juce::ToggleButton&, const char* id, const juce::String& text,
                      const juce::String& tip);
    void setupComboInternal (juce::ComboBox&, const juce::StringArray& items,
                             const juce::String& tip, juce::Value);
    void setupToggleInternal (juce::ToggleButton&, const juce::String& text,
                              const juce::String& tip, juce::Value);
    void passComboHoverThrough (juce::ComboBox&);

    AnabasisAudioProcessor& processor;
    abgui::AnabasisLookAndFeel lnf;
    // Null PARENT on purpose (a desktop tooltip is not clipped to the editor),
    // which is also why it needs `lnf` handed to it EXPLICITLY: a desktop
    // component has no parent to inherit a look-and-feel from, so it resolves
    // `LookAndFeel::getDefaultLookAndFeel()` and the family's `drawTooltip` /
    // `getTooltipBounds` overrides never ran — the capsule was adapted brand
    // code that nothing could reach. Wired in the constructor and cleared in
    // the destructor, both beside the editor's own `setLookAndFeel`, because
    // `lnf` is a member and must outlive every user of it. Declared AFTER
    // `lnf` so reverse-order destruction would be right even without that.
    juce::TooltipWindow tooltips { nullptr, 600 };

    // -- top bar -------------------------------------------------------------
    juce::TextButton   titleButton;            // ghost hit-area over the wordmark → About
    ABControl          abControl;
    juce::TextButton   copyButton { "Copy" };
    juce::TextButton   undoButton, redoButton;
    juce::TextButton   settingsButton { "Settings" };
    juce::ToggleButton advancedToggle, bypassToggle;
    juce::TextButton   presetPrev, presetNext, presetName;

    // -- COMP panel ----------------------------------------------------------
    Knob ratioK, thresholdK, attackK, releaseK, kneeK, compMixK;
    juce::Label ratioL, thresholdL, attackL, releaseL, kneeL, compMixL;
    juce::ToggleButton compAutoToggle;
    juce::ComboBox detectorBox;

    // -- CLIP / COLOUR panel -------------------------------------------------
    Knob shapeK, driveK, clipMixK, balanceK, colToneK, depthK, dynTiltK;
    juce::Label shapeL, driveL, clipMixL, balanceL, colToneL, depthL, dynTiltL;
    juce::ComboBox modelBox;

    // -- LIMITER panel -------------------------------------------------------
    Knob limGainK, ceilingK, lookaheadK, limReleaseK, linkK, preserveK;
    juce::Label limGainL, ceilingL, lookaheadL, limReleaseL, linkL, preserveL;
    juce::ToggleButton limAutoToggle, tpToggle;
    juce::ComboBox styleBox;

    // -- EQ panel ------------------------------------------------------------
    Knob eqTiltK, lsFreqK, lsGainK, hsFreqK, hsGainK,
         b1FreqK, b1GainK, b1QK, b2FreqK, b2GainK, b2QK;
    juce::Label eqTiltL, lsFreqL, lsGainL, hsFreqL, hsGainL,
                b1FreqL, b1GainL, b1QL, b2FreqL, b2GainL, b2QL;
    juce::ComboBox eqPosBox;

    // -- utility row (Advanced) ----------------------------------------------
    Knob inputGainK, scHpfK;
    juce::Label inputGainL, scHpfL;
    juce::ComboBox ditherBox;
    juce::ToggleButton shapingToggle, compToggle, deltaToggle, freezeToggle;

    // -- macro row (Advanced: read-only with detach badges, §6.3) ------------
    Knob macroLoudnessK, macroCharacterK, macroToneK;
    juce::Label macroLoudnessL, macroCharacterL, macroToneL;

    // -- Simple view (§6.2): the big knob IS the product ---------------------
    Knob bigLoudnessK, simpleCharacterK, simpleToneK, simpleCeilingK;
    juce::Label bigLoudnessL, simpleCharacterL, simpleToneL, simpleCeilingL;
    juce::ToggleButton ceilingLockToggle;          // int_ceilingLock (§4.2)
    juce::TextButton   learnButton { "LEARN" };    // §5.4 explicit start/end
    juce::Label outLufsCaption, outLufsValue;      // live render short-term

    // §5.3 "edited" indicator + reset-to-macro affordance: an accent dot that
    // appears when any managed parameter is detached; clicking it re-engages
    // in place (wording for its tooltip is owner-supplied — C8 TODO).
    struct EditedDot : public juce::Component, public juce::SettableTooltipClient
    {
        std::function<void()> onClick;
        void paint (juce::Graphics& g) override
        {
            g.setColour (abgui::colours::accent);
            g.fillEllipse (getLocalBounds().toFloat().reduced (2.0f));
        }
        void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    };
    EditedDot editedDot;

    // -- §2.9 visualisers (their own FrameClocks; created after the frame) ---
    std::unique_ptr<LoudnessMeterView> meterView;
    std::unique_ptr<GrHistoryView>     grView;
    std::unique_ptr<SpectrumView>      spectrumView;
    std::unique_ptr<CurveView>         clipCurve, eqCurve;
    GrMiniMeter compGrMeter, limGrMeter;
    bool shownSpectrumOn = true;

    // Learn UI state (§5.4 grammar): explicit start → minimum pass → explicit
    // end; an empty pass flashes the button in `warn` (wordless readout).
    double learnStartedMs   = 0.0;
    bool   learnStopPending = false;
    float  refOnsetAtStop = 0.0f, refTiltAtStop = 0.0f;
    bool   hadLearnedAtStop = false;
    double emptyFlashUntilMs = 0.0;
    juce::String lastMaskFingerprint;

    // -- overlays ------------------------------------------------------------
    DimLayer dimOverlay;
    Backdrop aboutBackdrop, settingsBackdrop, savePresetBackdrop;
    juce::HyperlinkButton aboutLink { "www.rolly.tech", juce::URL ("https://www.rolly.tech") };

    // -- Settings controls (all InternalState-bound, §6.4) -------------------
    juce::Label    settingsTitle;
    juce::ComboBox oversampleBox, phaseBox, offlineBox, uiScaleBox;
    juce::Label    oversampleLabel, phaseLabel, offlineLabel, uiScaleLabel;
    juce::ToggleButton animToggle, tooltipsToggle, tpMeterToggle;
    // One per `LoudnessMeterView::kTargets` entry, sized FROM that table:
    // three named members meant the count lived here as well as there, and
    // in the two loops and the layout row that walked them.
    std::array<juce::ToggleButton, (size_t) LoudnessMeterView::kNumTargets> targetToggles;
    juce::ToggleButton spectrumToggle;

    // -- Save-preset overlay -------------------------------------------------
    juce::Label      saveTitle;
    juce::TextEditor saveNameEditor;
    juce::TextButton saveOkButton { "Save" }, saveCancelButton { "Cancel" };
    std::unique_ptr<juce::FileChooser> fileChooser;
    void showLoadPreset();

    juce::OwnedArray<SliderAttachment>   sliderAtts;
    juce::OwnedArray<ButtonAttachment>   buttonAtts;
    juce::OwnedArray<ComboBoxAttachment> comboAtts;
    juce::Array<juce::ComboBox*>         allCombos;

    // -- micro-anim driver (lean adaptation of Anamorph F3) ------------------
    struct AnimatedWidget
    {
        juce::Component*    comp   = nullptr;
        juce::Slider*       slider = nullptr;
        juce::ToggleButton* toggle = nullptr;
    };
    juce::Array<AnimatedWidget> animated;
    juce::VBlankAttachment animVBlank;
    double lastFrameTime = 0.0;
    bool   uiAnimOn = true;

    bool  advanced   = false;
    bool  tooltipsOn = false;
    float hostScale  = 1.0f;
    juce::String presetShownName;
    bool shownDirty = false;
    int  dirtyPollDivider = 0;

#if JUCE_MAC || JUCE_WINDOWS
    juce::OpenGLContext glContext;
#endif

    // Geometry of record (see the banner). Simple keeps the family frame; the
    // Advanced height is DERIVED: 46 top bar + 446 panel row (EQ: header 22 +
    // combo row 26 + four 78 px knob rows + curve well) + 64 utility + 78
    // macro row + 266 metering strip = 900, which lands on the family number
    // by construction rather than by copying it.
    // §5.4 minimum Learn pass: the features are ~1.5 s integrated, so a pass
    // must outlast several time constants before the sums describe the passage
    // rather than what preceded it (the P4-recorded grammar debt).
    static constexpr double kLearnMinPassMs = 5000.0;

    static constexpr int kWidth      = 940;
    static constexpr int kSimpleH    = 720;
    static constexpr int kBarH       = 46;
    static constexpr int kPanelRowH  = 446;
    static constexpr int kUtilityH   = 64;
    static constexpr int kMacroRowH  = 78;
    static constexpr int kMeterRowH  = 266;
    static constexpr int kAdvancedH  = kBarH + kPanelRowH + kUtilityH + kMacroRowH + kMeterRowH;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnabasisAudioProcessorEditor)
};
