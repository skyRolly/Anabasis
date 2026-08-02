#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel.h"
#include "FrameClock.h"

class AnabasisAudioProcessor;

// ============================================================================
//  GrHistoryView — the §2.9 scrolling GR/waveform history (DESIGN §6.2/§6.3
//  bottom strip; Pro-L 2 presentation as the stated reference): the block
//  waveform peaks filled from the baseline, the gain-reduction trace hanging
//  from the top, over a 10–30 s window.
//
//  Reader contract (THREAD_MODEL, decided at the P5 opening): stateless
//  `peek`s against `available()`, and the RESET EPOCH sampled before and
//  after every batch — odd or moved means the batch raced a host-thread
//  clear and the frame is discarded (the reader re-derives from the fresh
//  index next tick; nothing is cached across an epoch change).
//
//  Time base: one ring entry spans one HOST block (the recorded caveat), so
//  the window is mapped through the CURRENT prepared block size — an
//  approximation that drifts only when the host's delivered blocks differ
//  from its prepared size, and only in display width, never in data.
// ============================================================================

class GrHistoryView : public juce::Component
{
public:
    explicit GrHistoryView (AnabasisAudioProcessor&);
    ~GrHistoryView() override = default;

    void paint (juce::Graphics&) override;
    void visibilityChanged() override;

    // 10–30 s per DESIGN §2.9; ⊕ default in the middle of the band.
    static constexpr double kWindowSeconds = 20.0;

private:
    void tick (double dt);

    AnabasisAudioProcessor& processor;
    abgui::FrameClock clock;
    int64_t shownHead = -1;   // repaint gate: new entries arrived

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrHistoryView)
};
