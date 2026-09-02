#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "LookAndFeel.h"
#include "FrameClock.h"
#include "../dsp/ScopeBuffer.h"

class AnabasisAudioProcessor;

// ============================================================================
//  SpectrumView — the §2.9 dual-trace input/output analyser, and one of the
//  TWO MODES of the shared graph well (`int_spectrumOn` true selects it,
//  false selects `GrHistoryView`; ADR-0016). It was brief §6's dismissible
//  overlay — "visible until dismissed" — until 2026-08-05, when the owner's
//  round-2 directive made the well a two-view switch: the corner chip now
//  names the view it swaps TO rather than dismissing anything, because the
//  well always shows one of the pair.
//
//  The audio thread only fills the two ScopeBuffer rings (engine taps:
//  post-input-gain and post-chain); the FFT runs HERE, on the paint side, per
//  the ADR-0011 / THREAD_MODEL division. Reads are stateless `readLatest`
//  peeks; a frame with no new samples repaints nothing.
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
    void mouseDown (const juce::MouseEvent&) override;   // bottom-left pill → GR history
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

    // HAS A RESET HAPPENED THAT THIS VIEW HAS NOT YET ACCOUNTED FOR? Two
    // SUFFICIENT conditions, neither of them necessary, and the OR is the whole
    // point — a `ScopeBuffer` reset is TWO stores (the index rewind, then the
    // generation bump) and a reader can observe either one first.
    //
    // The generation stays the RELIABLE detector and nothing about 0.2.7's
    // finding is reversed: it retired the COUNT as the SOLE detector because a
    // fast refill can pass the old value and the reset is then missed OUTRIGHT,
    // permanently. That is an insufficiency argument, never a soundness one, so
    // the count is re-admitted here as additional, earlier-firing evidence and
    // never as a replacement.
    //
    // Why the count term is SOUND: `write`'s modification order is 0 at
    // construction, then a non-decreasing run of `w + n` from `pushBlock`
    // (single producer), punctuated by 0 from `reset()` — the only writer of a
    // smaller value. `shownCount` is a value THIS thread obtained from an
    // earlier acquire load of that same object, so the two loads are
    // sequenced-before and read-read coherence ([intro.races]) forbids the
    // later one returning a value EARLIER in the modification order. A strictly
    // lower count is therefore proof that a 0-store intervened. It survives
    // arbitrary staleness, because coherence is stated over the modification
    // order rather than over real time.
    //
    // It cannot false-positive: both operands are unsigned and `shownCount`
    // starts at 0, so the first tick and a view attached to an already-running
    // processor both compare against 0. And it needs no change to `tick`'s idle
    // test, which keys on the same counts: `count < shownCount` implies
    // `count != shownCount`, so the gate can never swallow it.
    static bool resetObserved (uint32_t gen, uint32_t shownGen,
                               uint64_t count, uint64_t shownCount) noexcept
    { return gen != shownGen || count < shownCount; }

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
