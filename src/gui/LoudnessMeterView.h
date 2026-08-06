#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <iterator>
#include "LookAndFeel.h"
#include "FrameClock.h"

class AnabasisAudioProcessor;

// ============================================================================
//  LoudnessMeterView — the §2.9 loudness display (DESIGN §6.2 right panel /
//  §6.3 metering strip): LUFS M/S/I with bars, dBTP and PLR. Reads ONLY the
//  processor's published relaxed atomics (THREADING_POLICY meter row) at
//  FrameClock pace, behind a bitwise snapshot repaint gate (the Anamorph
//  LevelMeter recipe, ADR-0009).
//
//  Clicking the panel issues the §2.9 meter-hold reset request (integrated +
//  dBTP hold — the momentary-request row implemented at the P5 opening).
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

    // Snapshot gate: repaint only when a shown value actually changed.
    float shownM = 1.0f, shownS = 1.0f, shownI = 1.0f,
          shownTp = 1.0f, shownPlr = -1.0f,
          // The dBTP row's warn threshold is the USER's ceiling, so it is part
          // of the snapshot: a ceiling move must repaint the colour even when
          // no meter value changed.
          shownCeiling = 1.0f;
    // NOT a sentinel like the floats above — it is a real state, so it seeds
    // at the SHIPPED DEFAULT (`int_tpMeterOn` = false, ADR-0015). Seeded `true`
    // it made the first paint — the one before the first `FrameClock` tick can
    // correct the snapshot — draw a "TP -" row that then vanished, i.e. the
    // opening frame showed the opposite of the default configuration.
    bool  shownTpOn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessMeterView)
};
