#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <iterator>
#include "LookAndFeel.h"
#include "FrameClock.h"
// The RMS sentinel seeds `heldRawRms` below — this header names the constant,
// so it includes the class that owns it rather than leaning on the .cpp's
// transitive include of the processor.
#include "../dsp/RmsMeter.h"

class AnabasisAudioProcessor;

// ============================================================================
//  LoudnessMeterView — the §2.9 WAVEFORM STATISTICS panel (DESIGN §6.2 right
//  panel / §6.3 metering strip), identical in BOTH editor views since 0.1.1
//  (ADR-0020): LUFS M/S/I with bars, then true peak, sample peak, RMS, LRA
//  and PLR as numeric rows. Reads ONLY the processor's published relaxed
//  atomics (THREADING_POLICY meter row) at FrameClock pace, behind a bitwise
//  snapshot repaint gate (the Anamorph LevelMeter recipe, ADR-0009).
//
//  TWO ROWS FOLLOW A SETTINGS-CHOSEN STANDARD, and the choice is resolved
//  HERE rather than in the DSP: the processor publishes BOTH integrated
//  readings (gated = BS.1770-2 onward, ungated = BS.1770-1) and the
//  mathematical RMS, and this view picks the integrated one and adds the
//  AES-17 +3.01 dB offset. The audio thread therefore never reads a display
//  preference, and flipping either setting is instant with no audio involved.
//
//  Clicking the panel issues the §2.9 meter-hold reset request (integrated +
//  both peak holds — the momentary-request row implemented at the P5
//  opening).
//
//  STREAMING TARGETS ARE GONE, deliberately (owner directive 2026-08-05,
//  superseding OQ-008): platform normalisation makes per-platform target
//  lines and penalty arithmetic noise for a modern master, which is pushed
//  against the CEILING, not against a platform figure. The compiled kTargets
//  table, the tick overlay, the penalty rows, the Settings checkboxes and
//  the int_meterTargets bitmask were all removed together — an old session
//  still carrying the field is ignored by the §4.4 unknown-field rule.
// ============================================================================

class LoudnessMeterView : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    // The §2.9 meter tooltip. Static so the tests can reach the same string
    // the meter shows.
    static juce::String tooltipText();

    // The PLR row, DERIVED rather than read. The processor publishes `pubPlr`
    // against the GATED integrated figure, but the I row above follows §3.5's
    // standard choice — so with BS.1770-1 selected the panel would print
    // `TP − I_gated` beside `I = I_ungated` and the row would not be the
    // difference of the two rows it sits under. PLR is a RELATIONSHIP between
    // two shown values, so it is computed from what is shown, on the message
    // thread where ADR-0020 resolves every display choice. Returns 0 while the
    // integrated reading is still the meter's sentinel, exactly as the
    // published figure does. Static so the suite can pin the rule without
    // constructing and ticking an editor.
    static float plrFromShown (float tpDb, float integratedLufs) noexcept;

    // The RMS numeric row's two display rules, static for the reason
    // `plrFromShown` is: they carry the correctness argument, and the suite can
    // then pin them without constructing and ticking an editor.
    //
    // `shouldAdoptRms` is the 0.1.3 readout cadence (see tick()): the row takes
    // a fresh measurement at a readable pace instead of chasing the ~24 Hz
    // meter tick. `holdingAReading` is the view's "the value I am currently
    // showing is a measurement, not the sentinel" bit — passed in rather than
    // decoded from the shown float, because the shown float has the reference
    // offset on it and would re-create exactly the sentinel-overlap ambiguity
    // `RmsMeter`'s two constants exist to prevent.
    //
    // `rmsWithReference` applies the §3.5 Settings choice. It is deliberately
    // NOT inside the hold: the reference is a pure function of the measurement
    // on screen, so flipping it must move the row on the next frame — the
    // invariant the `shownRms` member below states.
    static bool  shouldAdoptRms (float rawDb, bool holdingAReading, double sinceAdoptSecs) noexcept;
    static float rmsWithReference (float rawDb, bool aes17) noexcept;

    explicit LoudnessMeterView (AnabasisAudioProcessor&);
    // Detached FIRST — the tick reads the whole `shown*` snapshot, declared
    // after `clock`, so `= default` freed it under an armed attachment. Same
    // reasoning as `~SpectrumView` and, one class up,
    // `~AnabasisAudioProcessorEditor`'s `animVBlank = {}`: not "safe by
    // declaration order".
    ~LoudnessMeterView() override { clock.stop(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void visibilityChanged() override;

private:
    void tick (double dt);

    AnabasisAudioProcessor& processor;
    abgui::FrameClock clock;

    // Snapshot gate: repaint only when a shown value actually changed. The
    // seeds are deliberately impossible readings (a positive LUFS/dBTP, a
    // negative PLR) so the first tick always repaints once.
    float shownM = 1.0f, shownS = 1.0f, shownI = 1.0f,
          shownTp = 1.0f, shownPlr = -1.0f,
          // The dBTP row's warn threshold is the USER's ceiling, so it is part
          // of the snapshot: a ceiling move must repaint the colour even when
          // no meter value changed.
          shownCeiling = 1.0f,
          // ADR-0020's rows. `shownRms` holds the value AFTER the reference
          // offset and `shownI` after the standard choice, so a Settings flip
          // moves the snapshot and repaints without a separate flag — the
          // shape `shownTpOn` needed a flag for, and the reason it is gone
          // along with the field it mirrored.
          shownPeak = 1.0f, shownRms = 1.0f, shownLra = -1.0f;

    // The RMS readout hold (0.1.3 item 1 — see tick() for the analysis-vs-
    // display judgement, and the two static rules above for the arithmetic).
    // `heldRawRms` is the RAW meter value the row is currently showing, held
    // between adoptions; the reference offset is applied to it every tick, so
    // `shownRms` above keeps carrying the post-offset value and a Settings
    // flip still moves the snapshot immediately.
    static constexpr double kRmsReadoutHoldSecs = 1.0 / 3.0;
    float  heldRawRms    = anabasis::RmsMeter::kSilentDb;
    double rmsSinceAdopt = 0.0;
    bool   shownRmsValid = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessMeterView)
};
