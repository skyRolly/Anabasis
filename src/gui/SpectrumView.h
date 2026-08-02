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

private:
    void tick (double dt);
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
