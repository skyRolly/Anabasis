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
    // The clock is DETACHED FIRST, not left to reverse-order destruction. Its
    // tick reads `scratchL`/`fftData`/`inDb`/`outDb` and the `shown*` counters,
    // all declared AFTER `clock`, so `= default` freed them while the vblank
    // attachment was still armed over them. Unreachable today — a message-thread
    // destructor cannot interleave with a message-thread vblank callback — but
    // that is the "safe by ordering" argument `~AnabasisAudioProcessorEditor`
    // refuses to rely on for its own `animVBlank`, and this is the same thing
    // one class down. Stating it also means a future member reorder cannot
    // quietly take the guarantee away. `FrameClock::stop()` is idempotent.
    ~SpectrumView() override { clock.stop(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;   // top-right chip → GR history
    // Interactive ONLY over that chip; everything else falls through to whatever
    // is beneath. See the definition — this is how a partly-interactive overlay
    // opts out. `GrHistoryView` mirrors it for its own chip; `CurveView` opts
    // out wholesale.
    bool hitTest (int x, int y) override;
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
    // The chip hit-area, in ONE place because `hitTest` and `mouseDown` must
    // agree about it — see the definition.
    juce::Rectangle<int> chipHitArea() const noexcept;
    void analyse (const anabasis::ScopeBuffer&, std::vector<float>& smoothedDb, double dt);

    AnabasisAudioProcessor& processor;
    abgui::FrameClock clock;
    juce::dsp::FFT fft { kOrder };
    juce::dsp::WindowingFunction<float> window { kSize,
        juce::dsp::WindowingFunction<float>::hann };

    std::vector<float> scratchL, scratchR, fftData, inDb, outDb;
    // `shownInCount`/`shownOutCount` answer "are there new frames?" and nothing
    // else. Detecting a RESET is the generations' job — see tick(), where the
    // counter comparison that used to carry both duties is explained.
    uint64_t shownInCount = 0, shownOutCount = 0;
    uint32_t shownInGen = 0, shownOutGen = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumView)
};
