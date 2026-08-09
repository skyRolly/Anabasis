#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel.h"
#include "FrameClock.h"
#include "../dsp/GrHistoryBuffer.h"

class AnabasisAudioProcessor;

// ============================================================================
//  GrHistoryView — the §2.9 scrolling GR/waveform history (DESIGN §6.2/§6.3
//  bottom strip; Pro-L 2 presentation as the stated reference): the block
//  waveform peaks filled from the baseline, the gain-reduction trace hanging
//  from the top, over a 10–30 s window.
//
//  Since 2026-08-05 this is one of the TWO MODES of the shared graph well
//  (`int_spectrumOn` is the mode flag — GR the default since 0.1.2; both
//  views hold identical bounds and only visibility flips). It carries the
//  bottom-left GR|SPEC toggle pill (`abgui::graph_switch`, one geometry and
//  one interaction for both views since 0.1.2 items 4+5), with the same
//  hit-area discipline: interactive over the pill ONLY, inert everywhere
//  else, so clicks over the trace pass through.
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

class GrHistoryView : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    explicit GrHistoryView (AnabasisAudioProcessor&);
    // Detached FIRST — the tick reads `shownHead`, declared after `clock`, so
    // `= default` freed it under an armed attachment. Same reasoning as
    // `~SpectrumView` and, one class up, `~AnabasisAudioProcessorEditor`'s
    // `animVBlank = {}`: not "safe by declaration order".
    ~GrHistoryView() override { clock.stop(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;   // bottom-left pill → spectrum
    // Interactive ONLY over the chip; everything else falls through — the same
    // per-pixel opt-out `SpectrumView::hitTest` documents at length.
    bool hitTest (int x, int y) override;
    void visibilityChanged() override;

    // 10–30 s per DESIGN §2.9; ⊕ default in the middle of the band.
    static constexpr double kWindowSeconds = 20.0;

    // How many entries behind the head the frame may read, given the prepared
    // rate and block size. Pure and public because the CLAMP is the part with
    // a correctness argument — `kSize - 1`, not `kSize`, because `peek` masks
    // the absolute index and `head - kSize` therefore aliases the slot the
    // audio thread is filling right now (it writes the slot, THEN publishes
    // head + 1), so a full-capacity window reads a half-written entry as its
    // oldest. A clamp that only exists inside `paint` is a clamp no test can
    // pin without a graphics context, which is how it sat one too wide.
    static int64_t windowEntries (double sampleRate, int blockSize) noexcept
    {
        const double sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        const int    bs = juce::jmax (1, blockSize);
        return juce::jmin<int64_t> (anabasis::GrHistoryBuffer::kSize - 1,
                                    (int64_t) std::ceil (kWindowSeconds * sr / (double) bs));
    }

    // The decimation geometry one frame draws. Public and pure for the reason
    // `windowEntries` is: the arithmetic with a correctness argument must be
    // pinnable without a graphics context, and this arithmetic had two bugs
    // that a `paint`-only version hid — a rightmost bucket that could come out
    // empty, and a bucket-per-pixel mapping that left the left third of the
    // panel permanently blank.
    //
    // Bucket k is the ABSOLUTE entry range [k·stride, (k+1)·stride), so an
    // entry never changes bucket and a completed bucket's decimated max never
    // changes — the property the 0.1.1 shimmer fix rests on. What the panel
    // width decides is only how many entries share a bucket, never where the
    // boundaries fall.
    struct Buckets
    {
        int64_t stride;   // entries per bucket; ≥ 1, sized so `kFull` ≤ cols
        int64_t kFirst;   // oldest bucket drawn; see `buckets` for both bounds
        int64_t kHead;    // bucket holding entry `head - 1`, so never empty
        int64_t count;    // buckets to draw = kHead − kFirst + 1, ≥ 1
        int64_t kFull;    // buckets one FULL window renders; fixes the pitch
    };

    // `head` = entries ever pushed (≥ 1), `want` = `windowEntries(...)`,
    // `cols` = the panel's pixel width (≥ 1).
    static Buckets buckets (int64_t head, int64_t want, int cols) noexcept
    {
        const int64_t c      = juce::jmax<int64_t> (1, (int64_t) cols);
        const int64_t stride = juce::jmax<int64_t> (1, (want + c - 1) / c);
        const int64_t first  = juce::jmax<int64_t> (0, head - want);
        const int64_t kHead  = juce::jmax<int64_t> (0, head - 1) / stride;
        // `kFull` ≥ 2 so the pitch below divides by at least 1 — reachable
        // only when the whole window fits in one bucket (want ≤ stride, i.e.
        // a panel around one pixel wide), where any pitch draws the same one
        // bucket at the right edge.
        const int64_t kFull  = juce::jmax<int64_t> (2, (want + stride - 1) / stride);
        // Two lower bounds on the oldest bucket, and both are needed: the
        // window bound (a bucket must lie WHOLLY inside [first, head)) and the
        // width bound (at the fixed pitch only `kFull` buckets fit on the
        // panel, so when the window's bucket count oscillates one above it —
        // stride-boundary phase — the oldest is truncated rather than the
        // pitch compressed; a truncated bucket is ≤ `stride` entries of the
        // oldest edge, the fixed-scale trade the 0.1.2 directive picks).
        const int64_t kFirst = juce::jmax (juce::jmin (kHead, (first + stride - 1) / stride),
                                           kHead - kFull + 1);
        return { stride, kFirst, kHead, kHead - kFirst + 1, kFull };
    }

    // Where bucket `k` lands: anchored at the RIGHT edge (bucket `kHead` at
    // `x0 + width − 1`) on a FIXED pitch — the width divided by the bucket
    // count of one FULL window — so a filling ring renders at exactly the
    // scale a settled one does and new entries appear at the right while the
    // unmeasured region stays empty on the left (0.1.2 item 3; the owner
    // directive that removed the previous stretch-to-fill, which zoomed the
    // early trace across the whole panel and re-spaced it as buckets grew).
    // The settled trace still spans the panel edge to edge — `kFull` is
    // derived from the same `want`/`stride` pair, so a full window lands its
    // oldest bucket on the left edge, which is the 0.1.1 blank-strip fix's
    // property carried over unchanged.
    static float bucketX (const Buckets& b, int64_t k, float x0, float width) noexcept
    {
        return x0 + (width - 1.0f)
             - (float) (b.kHead - k) * ((width - 1.0f) / (float) (b.kFull - 1));
    }

private:
    // The chip hit-area, in ONE place because `hitTest` and `mouseDown` must
    // agree about it — the rule `SpectrumView::chipHitArea` states.
    juce::Rectangle<int> chipHitArea() const noexcept;
    // The traces, split out of `paint` so the corner chip can be drawn AFTER
    // them (matching `SpectrumView`) without losing the reader contract's
    // three early returns — see the definition.
    void paintHistory (juce::Graphics&);
    void tick (double dt);

    AnabasisAudioProcessor& processor;
    abgui::FrameClock clock;
    int64_t shownHead = -1;   // repaint gate: new entries arrived

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrHistoryView)
};
