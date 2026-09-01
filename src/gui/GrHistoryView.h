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
//  index next tick; no ring DATA is cached across an epoch change). The one
//  value the view does carry between ticks since 0.2.8 is `smoothHead`, a
//  display-side estimate and not ring data: it is re-anchored on any rewind
//  and clamped to within one entry of the live head, so a stale value costs
//  at most one entry-pitch for one frame (`smoothedHead`).
//
//  Time base: one ring entry spans one HOST block (the recorded caveat), so
//  the window is mapped through the CURRENT prepared block size — an
//  approximation that drifts only when the host's delivered blocks differ
//  from its prepared size, and only in display width, never in data. Since
//  0.2.8 the same prepared pair also paces the SMOOTHED HEAD the trace's
//  sub-entry phase is read from (`entryPeriod`, `smoothedHead`, `phaseOf`),
//  held to within one entry of the real head — so a host whose cadence
//  differs degrades the MOTION to per-entry stepping at the host's own
//  cadence (a longer-than-prepared block parks the trace for the excess of
//  each block; a shorter one pins the phase near 0), never behind the data
//  and never more than one entry ahead of it. Bursty delivery — several
//  blocks per callback, which hosts rendering ahead of real time do — is the
//  same case at the burst rate; the worklog measures it and records the
//  lag-buffer design that would absorb it as a display-latency trade for
//  the owner, not taken here.
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
        return juce::jmin ((int64_t) (anabasis::GrHistoryBuffer::kSize - 1),
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
    //
    // `fill` and `phase` (0.2.8) locate the newest ENTRY inside the newest
    // bucket and inside its own period: the sub-bucket position `bucketX`
    // scrolls the whole trace by, one entry at a time instead of one bucket
    // at a time. `window` and `first` (0.2.8) are the READ window — see
    // `buckets` for why it is bucket-aligned and how it stays ring-safe.
    struct Buckets
    {
        int64_t stride;   // entries per bucket; ≥ 1, sized so `kFull` ≤ cols
        int64_t kFirst;   // oldest bucket drawn; see `buckets` for both bounds
        int64_t kHead;    // bucket holding entry `head - 1`, so never empty
        int64_t count;    // buckets to draw = kHead − kFirst + 1, ≥ 1
        int64_t kFull;    // buckets one FULL window renders; fixes the pitch
        int64_t fill;     // entries the newest bucket holds so far: 1 … stride
        double  phase;    // 0 … 1: how far into the newest entry's period the frame falls
        int64_t window;   // entries a frame may read: `want` rounded UP to whole buckets, ≤ kSize − 1
        int64_t first;    // oldest entry a frame may read: max (0, head − window)
    };

    // `head` = entries ever pushed (≥ 1), `want` = `windowEntries(...)`,
    // `cols` = the panel's pixel width (≥ 1), `phase` = `phaseOf (...)` — 0
    // for a frame drawn the instant the newest entry arrived, which is what a
    // caller without a frame clock means by leaving it out.
    static Buckets buckets (int64_t head, int64_t want, int cols, double phase = 0.0) noexcept
    {
        const int64_t c      = juce::jmax ((int64_t) 1, (int64_t) cols);
        const int64_t stride = juce::jmax ((int64_t) 1, (want + c - 1) / c);
        const int64_t kHead  = juce::jmax ((int64_t) 0, head - 1) / stride;
        // `kFull` ≥ 2 so the pitch below divides by at least 1 — reachable
        // only when the whole window fits in one bucket (want ≤ stride, i.e.
        // a panel around one pixel wide), where any pitch draws the same one
        // bucket at the right edge.
        const int64_t kFull  = juce::jmax ((int64_t) 2, (want + stride - 1) / stride);
        // THE READ WINDOW IS BUCKET-ALIGNED (0.2.8, the review's left-edge
        // finding): `want` rounded UP to `kFull` whole buckets — at most
        // `stride − 1` entries older than the 20 s, all of them off the
        // panel's left edge or inside the segment that crosses it. Until
        // 0.2.8 the window was `want` itself and a bucket had to lie WHOLLY
        // inside it, so the oldest DRAWN bucket sat up to a pitch inside the
        // left edge with a flat lead-in behind it — and once the trace
        // scrolled by the entry, that vertex walked left for `stride` entries
        // and then jumped a whole pitch RIGHT as its bucket expired: the one
        // bucket-rate discontinuity the fix had left on the panel. Aligned,
        // the oldest bucket the panel needs always has its last entry inside
        // the window, so the segment that crosses the edge is drawn exactly
        // (and clipped) and the lead-in is never reached. RING SAFETY is the
        // `kSize − 1` cap, the same argument as `windowEntries`: the loop
        // clamps every bucket's read range to `first` (`max (first, k ·
        // stride)`), so a partly expired oldest bucket reads only its
        // in-window remainder and no frame peeks below `head − (kSize − 1)`.
        // A saturated window whose `kFull · stride` would exceed the cap
        // (neither of this product's two plot widths does) keeps the cap as
        // its window, unaligned, and falls back to the lead-in.
        const int64_t window = juce::jmin (kFull * stride,
                                           (int64_t) anabasis::GrHistoryBuffer::kSize - 1);
        const int64_t first  = juce::jmax ((int64_t) 0, head - window);
        // Two lower bounds on the oldest bucket, both needed: the window
        // bound (the bucket HOLDING `first` — partly expired, its read range
        // clamped in the loop) and the width bound (the bucket just OFF the
        // panel's left edge, one beyond the `kFull` that fit at the fixed
        // pitch, so its segment crosses the edge). Never past `kHead`.
        const int64_t kFirst = juce::jmax (juce::jmin (kHead, first / stride), kHead - kFull);
        // ≥ 1 even for the `head == 0` frame `paintHistory` never draws, so the
        // range the struct states is true of every value it can hold.
        const int64_t fill   = juce::jmax ((int64_t) 1, head - kHead * stride);
        return { stride, kFirst, kHead, kHead - kFirst + 1, kFull, fill,
                 juce::jlimit (0.0, 1.0, phase), window, first };
    }

    // Does this frame draw the UNMEASURED zero region at the left edge?
    //
    // Pure and public for exactly the reason `windowEntries` and `buckets`
    // are, and the header comment there says it outright: "a version reachable
    // only from `paint` is a version no test can pin, which is how this went
    // unnoticed". The 0.1.3 left-edge defect was that same shape — the rule
    // lived inline in `paintHistory` and no mutant could reach it.
    //
    // The rule: the region is honest ONLY while the ring is still filling. A
    // FULL window reaches the same branch (as buckets expire, `kFirst`
    // alternates between the width bound and the window bound, so the oldest
    // bucket's x oscillates between the left edge and one pitch inside it) —
    // and there the strip left of it holds EXPIRED history, not unmeasured
    // data. Drawing the zero line across it and then dropping vertically into
    // the trace is what produced the flashing accent bar at bucket-expiry
    // rate. `head <= want` is exactly "still filling": `first` is
    // `max(0, head - want)`, and `GrHistoryBuffer::reset()` rewinds the write
    // index to 0, so a clear re-enters the filling case rather than being
    // misread as a scrolling window.
    static bool drawsZeroRegion (int64_t head, int64_t want) noexcept
    {
        return juce::jmax ((int64_t) 0, head - want) == 0;
    }

    // Where a bucket's reduction lands VERTICALLY. The trace hangs from the
    // top (`grDb` ≤ 0, zero reduction on `y0`) and the FULL panel height is
    // `abgui::meters::grSpanDb` of reduction — the same span the per-stage GR
    // meter lanes use, which is the whole point of the shared constant.
    //
    // Pure and public for exactly the reason `windowEntries`, `buckets` and
    // `drawsZeroRegion` are — and this mapping is the case that proves the
    // rule rather than merely repeating it. Until 0.1.6 it lived inline in
    // `paintHistory` as `jlimit (0, 1, -grDb / 12) * 0.5`: the trace reached
    // its deepest at HALF the panel height and drew a flat line for every
    // reduction past 12 dB, while the limiter's own meter — 24 dB, same
    // screen — kept filling. No test could reach the expression without a
    // graphics context, so the suites stayed green for the whole time the
    // display was under-reporting the plug-in's own work.
    //
    // The fix is a divisor and a dropped factor, NOT a re-scaling: 24 dB over
    // the full height puts 12 dB at half height, exactly where the old form
    // put it, so every reduction that was already visible is drawn at the
    // same y it was before and only the previously unreachable bottom half is
    // new.
    static float grY (float grDb, float y0, float height) noexcept
    {
        return y0 + height * juce::jlimit (0.0f, 1.0f, -grDb / abgui::meters::grSpanDb);
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
    // oldest DRAWN bucket on or just off the left edge (within a pitch
    // beyond it, never inside it: `buckets`), which is the 0.1.1 blank-strip
    // fix's property carried over.
    //
    // SCROLLED BY THE ENTRY, NOT BY THE BUCKET (0.2.8, the owner's jitter
    // report). Until 0.2.8 this read `right − (kHead − k) · pitch`: every
    // vertex stood still for `stride − 1` blocks and then jumped a whole pitch
    // when `kHead` incremented. The pitch is not an integer (1.447 px at
    // 48 kHz / 512 on the Simple well, 1.929 px at 1024), so each jump landed
    // every vertex on a new sub-pixel phase and the anti-aliased stroke
    // re-rasterised at every step — modelled at a 60 Hz vblank, 48 % of frames
    // drew no motion at all and the rest a 1.4 px lurch
    // (`worklogs/2026-09-01-gr-history-scroll-jitter.md`). A horizontal
    // segment is invariant under a horizontal shift, which is why the flat
    // zero-reduction line looked steady while everything sloped to its right
    // shimmered: the report is a description of this expression.
    //
    // Now a completed bucket sits `pitch / stride` further left for EVERY
    // entry pushed. The `(stride − fill)` term holds the trace one
    // entry-pitch to the right per entry the newest bucket has yet to collect
    // and runs down to zero as it fills, so the frame after a bucket
    // completes draws each vertex exactly one entry-pitch past where the
    // frame before drew it. Bucket identity and every completed bucket's
    // value are untouched — the same fixed-identity decimation, moved
    // smoothly. Adjacent COMPLETED buckets stay exactly one pitch apart; only
    // the newest bucket's own segment grows from one entry-pitch to one pitch
    // as it fills, and the newest vertex holds the right edge.
    //
    // `phase` (`phaseOf`) carries the same motion INTO the newest entry's
    // period: the trace sits a further `phase` entry-pitches left, where
    // `phase` is the fractional part of a head SMOOTHED at the nominal entry
    // rate (`smoothedHead`). That is what turns "however many entries this
    // frame happened to see" — 1 or 2 at 48 kHz / 512 on a 60 Hz display, a
    // 2:1 velocity beat — into one uniform step per frame, and it is the
    // only smoothing left once `stride == 1` (blocks of ~1062 samples and up
    // at 48 kHz on the Simple well, where one block IS one bucket). The
    // newest vertex joins that drift only while its bucket is COMPLETE and
    // waiting for the next one; a filling bucket's vertex stays pinned to
    // the edge. Either way no vertex ever moves rightward: the display
    // position is `−(head + phase)` entry-pitches from a fixed origin and
    // `head + phase` is the smoothed head, which never decreases. The strip a
    // drifting newest vertex vacates is drawn flat at its value
    // (`paintHistory`'s lead-out).
    static float bucketX (const Buckets& b, int64_t k, float x0, float width) noexcept
    {
        const float right    = x0 + (width - 1.0f);
        const float pitch    = (width - 1.0f) / (float) (b.kFull - 1);
        const float perEntry = pitch / (float) b.stride;
        if (k >= b.kHead)
            return right - (b.fill >= b.stride ? (float) b.phase : 0.0f) * perEntry;
        return right - (float) (b.kHead - k) * pitch
             + (float) ((double) (b.stride - b.fill) - b.phase) * perEntry;
    }

    // The entry range the NEWEST vertex aggregates: the trailing `stride`
    // entries `[max (first, head − stride), head)`, NOT the newest bucket's
    // own partial range `[kHead·stride, head)`. Until 0.2.8 it was the
    // latter, so at every bucket start the vertex on the right edge held the
    // min/max of a SINGLE block and then deepened as the bucket collected the
    // rest — a bucket-rate pop of the trace's tip (modelled in the worklog:
    // 0.99 dB mean frame-to-frame movement, 0.55 dB with the trailing window).
    // The trailing window is exactly as long as every completed bucket, and
    // it COINCIDES with the newest bucket at the instant that bucket
    // completes, so the vertex hands over to the completed value without a
    // step. `first` is the window/ring clamp `paintHistory` derives
    // (`max (0, head − want)`).
    static int64_t tipFirst (int64_t first, int64_t head, int64_t stride) noexcept
    {
        return juce::jmax (first, head - stride);
    }

    // The nominal seconds one ring entry spans: the prepared block over the
    // prepared rate, the time base `windowEntries` already maps the window
    // through, with the same recorded caveat (banner). Unprepared reads as
    // 48 kHz, as there.
    static double entryPeriod (double sampleRate, int blockSize) noexcept
    {
        const double sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        return (double) juce::jmax (1, blockSize) / sr;
    }

    // The SMOOTHED head the phase is read from: the previous value advanced
    // by the frame's seconds at the nominal entry rate, then held to
    // [head, head + 1] — never behind the data (an entry the estimate did
    // not expect snaps it forward, so the trace can only ever move LEFT),
    // and never more than one entry ahead (a stopped transport parks the
    // trace one entry-pitch on rather than letting it run ahead of the data;
    // a host whose blocks are longer than prepared parks it the same way for
    // the excess of every block, and one whose blocks are shorter keeps the
    // lower bound biting, so either degrades to per-entry stepping at the
    // host's cadence and never worse). A head that has REWOUND —
    // `GrHistoryBuffer::reset()` — re-anchors at phase 0 rather than
    // parking, which is the one way `previous` can exceed the cap. `dt` is
    // the frame clock's real gap clamped to 50 ms, so a longer stall only
    // ever UNDER-advances the estimate, which the lower bound then snaps to
    // `head`. A first draft read the phase as "seconds since the tick that
    // first saw the head move", which is 0 on every frame that sees an
    // entry — at 48 kHz / 512 that is every 60 Hz frame, so the ramp never
    // engaged and the display still stepped by whichever of 1 or 2 entries
    // the frame happened to catch (the review's finding; the worklog has
    // the numbers).
    static double smoothedHead (double previous, int64_t head, double dt, double period) noexcept
    {
        const double lo = (double) head, hi = lo + 1.0;
        if (previous > hi)
            return lo;
        return juce::jlimit (lo, hi, period > 0.0 ? previous + dt / period : previous);
    }

    // The fraction of the newest entry's period the smoothed head has
    // covered, 0 … 1 — what `bucketX` shifts the trace by.
    static double phaseOf (double smoothHead, int64_t head) noexcept
    {
        return juce::jlimit (0.0, 1.0, smoothHead - (double) head);
    }

    // The tick's idle gate: no new entry AND the smoothed head already at its
    // cap, so nothing this frame could draw differs from the last one — the
    // pre-0.2.8 rule, repaint on new data only, back in force after at most
    // one entry period of extra frames. A rewound head is "new" here and so
    // un-parks. Pure for the reason every other rule in this header is.
    static bool parked (int64_t head, int64_t shownHead, double smoothHead) noexcept
    {
        return head == shownHead && smoothHead >= (double) head + 1.0;
    }

    // Which head a frame draws: the one the tick computed the phase FOR, not
    // whatever `available()` reads at paint time. Where the deferred paint
    // runs after the vblank tick (macOS, Windows) an entry can land between
    // the two, and drawing the live head at the smoothed head's phase would
    // put that frame one sub-entry step ahead of the ramp — never rightward,
    // but a residual the ramp exists to remove. Entries below `shownHead`
    // are published by the ring's own contract (it was read from
    // `available()`), so the older head is always safe to draw; the live
    // head is used before the first tick and on a rewind.
    static int64_t paintHead (int64_t shownHead, int64_t live) noexcept
    {
        return (shownHead > 0 && shownHead <= live) ? shownHead : live;
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
    int64_t shownHead  = -1;   // repaint gate: new entries arrived
    double  smoothHead = 0.0;  // `smoothedHead`: the head advanced at the nominal rate

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrHistoryView)
};
