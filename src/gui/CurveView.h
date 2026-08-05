#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel.h"
#include "../dsp/ClipSat.h"
#include "../dsp/MasteringEQ.h"

class AnabasisAudioProcessor;

// ============================================================================
//  CurveView — the §6.3 panel wells: the clipper's live transfer curve and
//  the EQ's response curve (DESIGN §1.3's one CurveView, two configurations).
//
//  One-source-of-truth rule (§2.4): both curves are EVALUATED through the
//  DSP's own code — `ClipSat::transfer` with the drive compensation the
//  audio path applies, and a scratch `MasteringEQ`'s `magnitudeDbAt` over
//  the very coefficient recompute the audio runs — never re-derived maths
//  that could drift.
//
//  GrMiniMeter rides along here: the per-panel GR bars for the COMP and
//  LIMITER wells, each labelled by its own stage — the answer to the
//  recorded "which reduction is the meter showing" question.
// ============================================================================

class CurveView : public juce::Component
{
public:
    enum class Mode { clipTransfer, eqResponse };

    CurveView (AnabasisAudioProcessor&, Mode);
    ~CurveView() override = default;

    void paint (juce::Graphics&) override;
    void refresh();   // editor timer: recompute + repaint when inputs moved

private:
    // EVERYTHING a curve is a function of, plus the fingerprint of exactly
    // those values. The two are produced by one pass (`readInputs`), which is
    // what makes the paint cache's label true by construction rather than by a
    // rule about call order — see paint().
    struct Inputs
    {
        juce::uint64 fingerprint = 0;
        double sampleRate = 48000.0;
        float clipShape = 0.0f, clipDrive = 0.0f;   // Mode::clipTransfer
        anabasis::EngineParameters eq {};           // Mode::eqResponse
    };
    Inputs readInputs() const;

    AnabasisAudioProcessor& processor;
    const Mode mode;
    anabasis::MasteringEQ scratchEq;   // GUI-side scratch; never the audio one
    juce::uint64 shownFingerprint = 0;   // refresh()'s edge detector, nothing else

    // The built curve, and the two inputs it was built from — see paint().
    void paintStatic (juce::Graphics&, juce::Rectangle<float> area) const;
    juce::Path cachedPath;
    juce::uint64 pathFingerprint = 0;
    juce::Rectangle<int> pathBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveView)
};

// A one-bar GR meter for a panel well (COMP / LIMITER), fed by the editor's
// timer from the per-stage published atomics.
class GrMiniMeter : public juce::Component
{
public:
    void setGrDb (float grDb)
    {
        if (! juce::exactlyEqual (grDb, shownGrDb))
        {
            shownGrDb = grDb;
            repaint();
        }
    }
    void paint (juce::Graphics& g) override
    {
        auto bar = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (abgui::colours::bgRaised);
        g.fillRoundedRectangle (bar, 3.0f);
        const float t = juce::jlimit (0.0f, 1.0f, -shownGrDb / 24.0f);
        if (t > 0.001f)
        {
            g.setColour (abgui::colours::accent);
            g.fillRoundedRectangle (bar.removeFromRight (bar.getWidth() * t), 3.0f);
        }
    }

private:
    float shownGrDb = 0.0f;
};
