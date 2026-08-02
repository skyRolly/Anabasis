#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel.h"
#include "FrameClock.h"

class AnabasisAudioProcessor;

// ============================================================================
//  LoudnessMeterView — the §2.9 loudness display (DESIGN §6.2 right panel /
//  §6.3 metering strip): LUFS M/S/I with bars, streaming target lines with
//  the loudness-penalty estimate, dBTP and PLR. Reads ONLY the processor's
//  published relaxed atomics (THREADING_POLICY meter row) at FrameClock pace,
//  behind a bitwise snapshot repaint gate (the Anamorph LevelMeter recipe,
//  ADR-0009).
//
//  Clicking the panel issues the §2.9 meter-hold reset request (integrated +
//  dBTP hold — the momentary-request row implemented at the P5 opening).
//
//  The target table (OQ-008 mechanism, ratified): ONE compiled table, each
//  value carrying its citation and the "as of" date, surfaced in the
//  tooltip; per-platform visibility via int_meterTargets; not user-editable
//  in v1. Values gathered 2026-08 (C2 — cited, not invented):
//    Spotify      −14 LUFS  (Normal mode; Loud −11 / Quiet −19)
//    Apple Music  −16 LUFS  (Sound Check)
//    YouTube      −14 LUFS
//  confirmed across soundplate.com/streaming-loudness-lufs-table and
//  trackgleam.com/learn/lufs-streaming-targets as of 2026-07/08; owner
//  verification against the first-party pages is the release check OQ-008
//  keeps open. Club/CD has NO published normalization target — that line
//  needs an owner-specified reference level and is deliberately absent.
// ============================================================================

class LoudnessMeterView : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    struct Target
    {
        const char* shortName;   // the §6.2 wireframe tags: Sp / Ap / YT
        float lufs;
    };
    static constexpr Target kTargets[] = {
        { "Sp", -14.0f },
        { "Ap", -16.0f },
        { "YT", -14.0f },
    };
    static constexpr int kNumTargets = 3;

    explicit LoudnessMeterView (AnabasisAudioProcessor&);
    ~LoudnessMeterView() override = default;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void visibilityChanged() override;

private:
    void tick (double dt);

    AnabasisAudioProcessor& processor;
    abgui::FrameClock clock;

    // Snapshot gate: repaint only when a shown value actually changed.
    float shownM = 1.0f, shownS = 1.0f, shownI = 1.0f,
          shownTp = 1.0f, shownPlr = -1.0f;
    int   shownMask = -1;
    bool  shownTpOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessMeterView)
};
