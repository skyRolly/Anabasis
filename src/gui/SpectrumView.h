#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "LookAndFeel.h"
#include "FrameClock.h"
#include "../dsp/ScopeBuffer.h"

class AnabasisAudioProcessor;

// ============================================================================
//  SpectrumView — the §2.9 dual-trace input/output overlay (brief §6:
//  dismissible, visible until dismissed — `int_spectrumOn`). The audio thread
//  only fills the two ScopeBuffer rings (engine taps: post-input-gain and
//  post-chain); the FFT runs HERE, on the paint side, per the ADR-0011 /
//  THREAD_MODEL division. Reads are stateless `readLatest` peeks; a frame
//  with no new samples repaints nothing.
//
//  Display: 4096-point Hann FFT, mono-summed, log-f 20 Hz–20 kHz, −90..0 dB,
//  per-bin EMA smoothing so the trace holds still enough to read. The input
//  trace draws dim, the output in the accent — the same two-material rule
//  the meters use.
// ============================================================================

class SpectrumView : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    explicit SpectrumView (AnabasisAudioProcessor&);
    ~SpectrumView() override = default;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;   // top-right × dismisses
    void visibilityChanged() override;

    static constexpr int kOrder = 12;                    // 4096-point FFT
    static constexpr int kSize  = 1 << kOrder;
    static constexpr int kBins  = kSize / 2;

    // PUBLIC only so the headless suite can drive it: the `FrameClock` needs a
    // vblank that never arrives there, so the analyser's one behavioural edge —
    // a ring rewound by a re-prepare must drop the trace it can no longer
    // justify — would otherwise have no way to be exercised. Same reasoning as
    // `AnabasisAudioProcessorEditor::refreshInternalSettingsBoxes` and
    // `MacroEngine::drainTick`: a direction nothing can call is a direction
    // nothing can guard.
    void tick (double dt);

    // Read-only view of the smoothed analysis, for the same reason.
    const std::vector<float>& analysedInDb() const noexcept { return inDb; }

private:
    void analyse (const anabasis::ScopeBuffer&, std::vector<float>& smoothedDb, double dt);

    AnabasisAudioProcessor& processor;
    abgui::FrameClock clock;
    juce::dsp::FFT fft { kOrder };
    juce::dsp::WindowingFunction<float> window { kSize,
        juce::dsp::WindowingFunction<float>::hann };

    std::vector<float> scratchL, scratchR, fftData, inDb, outDb;
    uint64_t shownInCount = 0, shownOutCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumView)
};
