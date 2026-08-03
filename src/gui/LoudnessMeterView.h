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
        const char* fullName;    // the Settings checkbox label and the tooltip
        float lufs;
    };
    // THE table for the OQ-008 values — and now for the NAMES and the "as of"
    // date too. The tooltip below and the three §6.4 Settings checkboxes each
    // used to carry their own copy of the platform names, and the tooltip
    // carried the numbers and the date as free text, which is precisely the
    // second-copy-that-must-agree shape this build spent ten review rounds
    // removing elsewhere: OQ-008 prescribes a per-release refresh, and that
    // refresh would have updated this table while leaving the tooltip quoting
    // the old figures. Everything user-visible about a target is derived from
    // here; `tooltipText()` builds the string.
    static constexpr Target kTargets[] = {
        { "Sp", "Spotify",     -14.0f },
        { "Ap", "Apple Music", -16.0f },
        { "YT", "YouTube",     -14.0f },
    };
    static constexpr int kNumTargets = 3;
    // The provenance the OQ-008 mechanism promises: shown next to the values so
    // a stale table is visibly stale. Bump it WITH the table, never separately.
    static constexpr const char* kTargetsAsOf = "2026-08";

    // The §2.9 meter tooltip, derived from the table above. Static so the
    // Settings panel and the tests can reach the same string the meter shows.
    static juce::String tooltipText();

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
          shownTp = 1.0f, shownPlr = -1.0f,
          // The dBTP row's warn threshold is the USER's ceiling, so it is part
          // of the snapshot: a ceiling move must repaint the colour even when
          // no meter value changed.
          shownCeiling = 1.0f;
    int   shownMask = -1;
    bool  shownTpOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessMeterView)
};
