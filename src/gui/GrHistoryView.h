#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
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
//  Reader contract (THREAD_MODEL, decided at the P5 opening), in two halves
//  because a frame can lose a race in two different ways:
//
//   * RESET. The epoch is sampled before and after every batch — odd or
//     moved means the batch raced a host-thread clear and the frame is
//     discarded (the reader re-derives from the fresh index next tick; no
//     ring DATA is cached across an epoch change).
//   * WRAPAROUND. `push` writes the slot and THEN publishes the index, so
//     when a reader sees `available() == L` the audio thread may be mid-write
//     on slot `L & kMask`. A peek of index `n` touches that same slot when
//     `L − n ≥ kSize`, so every read must satisfy `L − n ≤ kSize − 1`
//     (`readFloor`) — measured against the LIVE index, not against the head
//     the frame draws. `available()` is re-read after the batch and the frame
//     discarded if the producer has since advanced past that bound, which is
//     the epoch guard's discipline applied to the other failure mode.
//
//  A PHASE ONLY MEANS SOMETHING IN THE TIMELINE IT WAS MEASURED IN, so the
//  epoch it was published under is published with it (`publishedEpoch`,
//  release; read acquire before the two scalars). `GrHistoryBuffer::reset()`
//  rewinds the write index, so a phase from before a clear describes indices
//  that no longer exist — and a paint landing after the clear but before the
//  next tick would otherwise pair the fresh head with that stale value and
//  draw the first frame of the new history one entry-pitch out. The epoch is
//  what a frame checks; `frameFor` anchors at phase 0 whenever it does not
//  match, and the release/acquire pair is what makes "matches" mean the phase
//  published under it is the one this frame can see.
//
//  The values the view carries between ticks since 0.2.8 are `shownHead` and
//  `smoothHead` — display-side estimates, never ring data. `smoothHead` is
//  re-anchored on any rewind and clamped to within one entry of the live
//  head, so a stale value costs at most one entry-pitch for one frame
//  (`smoothedHead`). Both are PUBLISHED by `tick` on the message thread and
//  READ by `paintHistory` on whichever thread paints — the message thread on
//  Linux, the GL render thread on macOS/Windows (THREAD_MODEL "Which context
//  paints"). That is THREADING_POLICY's Message → Painting row (ADR-0027):
//  relaxed atomic scalars, read-only from the painting side, ordering no
//  other memory. They are read as a PAIR, and the pairing is safe by value
//  rather than by synchronisation — see `frameFor`.
//
//  Time base: one ring entry spans one HOST block (the recorded caveat), so
//  the window is mapped through the prepared (rate, block) pair — an
//  approximation that drifts only when the host's delivered blocks differ
//  from its prepared size, and only in display width, never in data. The pair
//  is the RING's (`GrHistoryBuffer::prepared`, stored inside the clear that
//  starts a timeline and read under the same epoch bracket as the entries),
//  not `AudioProcessor`'s: those members are plain, the host writes them from
//  its callback thread, and this view reads on two others. Since
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
//  the owner, not taken here and filed as OQ-017 (0.2.11 re-measured both
//  cases on the real paint path and left them by instruction).
//
//  WHAT A FRAME DRAWS (0.2.11): complete buckets only, each created once at
//  the value it keeps and moved as one rigid body with the rest — the newest
//  vertex is no longer a live estimate. `bucketX` carries the argument.
//
//  WHAT A FRAME SHOWS (0.2.12): the plot's columns left of `visibleRight`
//  only. The strip between the newest complete vertex and the clip edge —
//  the lead-out, plus the newest segment in the frame it appears — is the
//  one part of the trace that changes other than by translation, and it is
//  now clipped rather than drawn to the edge. Nothing else moved: the anchor,
//  `bucketX` and the left edge are what they were in 0.2.11.
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

    // The frame clock's callback, and the ONE thing that publishes the pair a
    // frame draws. Public for the reason `SpectrumView::tick` is — a direction
    // nothing can call is a direction nothing can guard, and the frame the
    // view shows the instant it becomes visible (`visibilityChanged`) is a
    // property only a test that can drive this by hand can pin.
    void tick (double dt);

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
    // at a time. `kLast` (0.2.11) is the newest bucket a frame DRAWS — the
    // newest COMPLETE one. `kHead` is complete only when `fill == stride`;
    // until then its vertex does not exist yet (see `bucketX` for why a
    // half-collected bucket is never drawn). `window` and `first` (0.2.8) are
    // the READ window — see `buckets` for why it is bucket-aligned and how it
    // stays ring-safe.
    struct Buckets
    {
        int64_t stride;   // entries per bucket; ≥ 1, sized so `kFull` ≤ cols
        int64_t kFirst;   // oldest bucket drawn; see `buckets` for both bounds
        int64_t kHead;    // bucket holding entry `head - 1`, so never empty
        int64_t kLast;    // newest bucket DRAWN: kHead if complete, else kHead − 1 (may be kFirst − 1)
        int64_t count;    // buckets to draw = kLast − kFirst + 1, ≥ 0 (0 only before the first bucket completes)
        int64_t kFull;    // buckets one FULL window renders; fixes the pitch
        int64_t fill;     // entries the newest bucket holds so far: 1 … stride; == stride means complete
        double  phase;    // 0 … 1: how far into the newest entry's period the frame falls
        int64_t window;   // the window LENGTH in entries: `want` rounded UP to whole buckets, capped to the ring
        int64_t first;    // oldest entry a frame may read: the oldest DRAWN bucket's own first entry, `kFirst · stride`
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
        //
        // …and capped so that `kFull` WHOLE buckets plus the alignment the
        // read window needs (`stride − 1` entries, below) fit inside the
        // ring's one safe lap: `kFull · stride + stride − 1 ≤ kSize − 1`. The
        // cap is on `kFull` rather than on the window length because the two
        // must agree — the panel renders `kFull` buckets at a pitch derived
        // from `kFull`, and a window shorter than that would put the oldest
        // DRAWN bucket a pitch inside the left edge with the flat lead-in
        // behind it, which is the bucket-rate walk 0.2.8 removed. It binds
        // only at a saturated window (`want` at `windowEntries`' clamp:
        // blocks of about 234 samples or fewer at 48 kHz, 937 at 192 kHz),
        // where it costs one bucket of the twenty seconds and 0.2 % of the
        // pitch; every ordinary window sits far below it (1875 · 3 against
        // 4089 at 48 kHz / 512). A panel so narrow that even two buckets
        // overflow the ring keeps its two and leaves the ring to
        // `firstDrawn`, which is where safety is enforced in any case.
        const int64_t kRing  = ((int64_t) anabasis::GrHistoryBuffer::kSize - stride) / stride;
        const int64_t kFull  = juce::jmax ((int64_t) 2,
                                           juce::jmin ((want + stride - 1) / stride, kRing - kMaxLead));
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
        // (and clipped) and the lead-in is never reached.
        //
        // AND THE START IS ALIGNED TOO (0.2.12 review). `window` is a LENGTH,
        // so `head − window` lands mid-bucket on every head that is not a
        // multiple of `stride`, and until this round THAT was `first`: the
        // loop clamped the oldest drawn bucket's read range to it
        // (`bucketReads`), so as the head advanced that bucket lost its
        // earliest entries one at a time and the value it yields — a min over
        // its span — changed while the bucket was still drawn. Its vertex
        // sits off the left edge, but the segment from it to its neighbour
        // crosses the edge, so the visible sliver re-shaped: 340 value
        // changes in 1800 frames at 48 kHz / 512, up to 1.53 dB (5.9 px on
        // the Simple well, 15.5 px on the Advanced), on 19 % of frames — and
        // never on any other drawn bucket, in 3.9 million readings. `first`
        // is now `kFirst · stride`, the oldest DRAWN bucket's OWN first
        // entry, so every drawn bucket aggregates its complete span for its
        // whole visible life. `kFirst` is untouched — the same bucket, the
        // same x, the same crossing segment — and what changes is only which
        // entries it aggregates: its own `stride` of them, always, rather
        // than however many the 20-second boundary happened to leave inside
        // it. The display therefore reaches up to `stride − 1` entries
        // (32 ms at 48 kHz / 512) past the nominal window, every one of them
        // off the left edge: this item's "read and never shown".
        //
        // THE PANEL COVERS MORE BUCKETS THAN THE PITCH DIVIDES (0.2.12).
        // `kFull` is the pitch divisor — the buckets ONE window renders — and
        // it stays exactly what it was, so the pitch, the bucket boundaries
        // and every value are untouched. What is new is that `paintHistory`
        // draws its frame `hiddenColumns` further right, so the boundary lands
        // on the plot's own right edge and the panel's leftmost columns come
        // free; `leadBuckets` more buckets of OLDER history fill them, at the
        // same pitch. `cover` is therefore what the window must hold and how
        // far back `kFirst` may reach — the display shows `lead · stride`
        // entries more than the nominal twenty seconds (96 ms at
        // 48 kHz / 512), all of it inside the panel now rather than off its
        // left edge.
        //
        // RING SAFETY, the same argument as `windowEntries`: the alignment
        // reaches at most `stride − 1` entries below `head − window`, and
        // `kFull`'s cap above — `kRing` less the four buckets `leadBuckets`
        // can ask for — is exactly the room for both, so `head − first` stays
        // inside the ring's one safe lap at every head. What a LAPPING
        // producer can still take is `firstDrawn`'s question, not this one.
        const int64_t cover  = juce::jmin (kFull + leadBuckets (kFull, (int) c), kRing);
        const int64_t window = cover * stride;
        const int64_t start  = juce::jmax ((int64_t) 0, head - window);
        // Two lower bounds on the oldest bucket, both needed: the window
        // bound (the bucket HOLDING the window's start, whose own earlier
        // entries the alignment above then takes in) and the width bound (the
        // bucket just OFF the panel's left edge, one beyond the `kFull` that
        // fit at the fixed pitch, so its segment crosses the edge). Never
        // past `kHead`.
        const int64_t kFirst = juce::jmax (juce::jmin (kHead, start / stride), kHead - cover);
        const int64_t first  = kFirst * stride;
        // ≥ 1 even for the `head == 0` frame `paintHistory` never draws, so the
        // range the struct states is true of every value it can hold.
        const int64_t fill   = juce::jmax ((int64_t) 1, head - kHead * stride);
        // The newest bucket is drawn only once it is complete (0.2.11): a
        // frame never shows a value that a later entry could still revise.
        // `kLast` is `kFirst − 1` and `count` 0 exactly while the very first
        // bucket is still collecting (`head < stride`).
        const int64_t kLast  = fill >= stride ? kHead : kHead - 1;
        return { stride, kFirst, kHead, kLast, kLast - kFirst + 1, kFull, fill,
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

    // Where bucket `k` lands: anchored at the RIGHT edge (the newest COMPLETE
    // bucket at `x0 + width − 1` the instant it completes) on a FIXED pitch — the width divided by the bucket
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
    // smoothly. Adjacent buckets stay exactly one pitch apart.
    //
    // `phase` (`phaseOf`) carries the same motion INTO the newest entry's
    // period: the trace sits a further `phase` entry-pitches left, where
    // `phase` is the fractional part of a head SMOOTHED at the nominal entry
    // rate (`smoothedHead`). That is what turns "however many entries this
    // frame happened to see" — 1 or 2 at 48 kHz / 512 on a 60 Hz display, a
    // 2:1 velocity beat — into one uniform step per frame, and it is the
    // only smoothing left once `stride == 1` (blocks of ~1062 samples and up
    // at 48 kHz on the Simple well, where one block IS one bucket). No vertex
    // ever moves rightward: the display position is `−(head + phase)`
    // entry-pitches from a fixed origin and `head + phase` is the smoothed
    // head, which never decreases.
    //
    // EVERY DRAWN VERTEX IS A COMPLETE BUCKET, AND EVERY ONE OBEYS THE SAME
    // LAW (0.2.11, the owner's second report). Until 0.2.11 the newest bucket
    // was drawn while it was still collecting: pinned to the edge, valued as
    // a sliding minimum over the trailing `stride` entries, then released to
    // drift once complete with a flat lead-out behind it. Every part of that
    // was a vertex being REVISED after it had been shown — its height on
    // every entry (measured on the real limiter at 2–5 px per frame, up to
    // 22 px on the Simple well and 57 on the Advanced), its segment's slope
    // on every frame as the previous vertex scrolled away from a pinned end,
    // and its shape once more at hand-over, a flat ledge collapsing into the
    // one-pitch spike the bucket would keep. The owner saw exactly that: the
    // newly generated line changing instantaneously and while moving, with
    // the completed trace gliding rigidly behind it. So the newest vertex is
    // now created ONCE, when its bucket's last entry lands, at the value it
    // will keep — `buckets` reports it as `kLast`, and a half-collected
    // `kHead` is never drawn at all. A frame therefore contains only frozen
    // values at rigid positions: the expression below is one linear function
    // of the smoothed head for every k, equal to
    //   right − perEntry · ((head + phase) − (k + 1) · stride),
    // so each vertex sits where its bucket's LAST entry falls in time. The
    // newest drawn vertex lies between `right − pitch` (a bucket has just
    // started collecting) and `right` (one has just completed); the strip
    // between it and the edge holds no data yet and is drawn flat at its
    // value (`paintHistory`'s lead-out). What the display gives up is up to
    // `stride − 1` entries of latency on the newest value — 21 ms at
    // 48 kHz / 512 on the Simple well, 32 ms on the Advanced — which is the
    // price of never revising a drawn vertex. For `k > kLast` the expression
    // is still well defined (a point to the right of the anchor) and unused.
    static float bucketX (const Buckets& b, int64_t k, float x0, float width) noexcept
    {
        const float right    = x0 + (width - 1.0f);
        const float pitch    = (width - 1.0f) / (float) (b.kFull - 1);
        const float perEntry = pitch / (float) b.stride;
        return right - (float) (b.kHead - k) * pitch
             + (float) ((double) (b.stride - b.fill) - b.phase) * perEntry;
    }

    // THE VISIBLE RIGHT BOUNDARY (0.2.12, the owner's third report): the first
    // plot column `paintHistory` does NOT show. What 0.2.11 left on screen was
    // the strip from the newest complete vertex to the clip edge — the
    // lead-out, drawn flat at the last value because the bucket that belongs
    // there is still collecting — and the owner saw exactly that: a
    // horizontal stub at the right edge, one to `pitch + 1` pixels long
    // (1–2.4 px at 48 kHz / 512 on the Simple well), whose height jumped, and
    // whose left neighbour snapped from flat to sloped, once per bucket (31
    // times a second at 48 kHz / 512 on the Simple well), in the GR stroke and the
    // level fill alike since both run the same path. Frozen values placed
    // rigidly do not help there: the stub IS the placeholder for a value
    // that does not exist yet, so the only honest thing to do with it is not
    // to show it.
    //
    // The bound follows from `bucketX`. With `fill ∈ [1, stride]` and
    // `phase ∈ [0, 1]` the newest drawn vertex sits at
    //   right − pitch ≤ x(kLast) ≤ right
    // on every frame — the lower end only at phase 1, a parked frame — so
    // the lead-out never starts left of `right − pitch`, and with the clip's
    // right edge at `B = floor (right − pitch)`, the first column the
    // lead-out can reach, no column shown ever carries it. The further `− 1`
    // is the owner's margin for the one thing that bound does not cover: the
    // stroke JOIN at the vertex that was newest until this frame re-shapes
    // when its successor appears (a horizontal lead-out gives way to a sloped
    // segment), and that vertex sits between `right − 2·pitch` and
    // `right − pitch` — up to a frame's travel inside `B` on a 60 Hz clock
    // (0.65 px measured at 48 kHz / 1024), further on a slower one — while a
    // JUCE mitred join can reach four half-widths from it. The boundary sweep
    // in the worklog shows the one column of margin taking the only
    // configuration that registered a residual (48 kHz / 1024) to the
    // interior's floor; it is a measured margin, not a bound, and a re-shape
    // that did reach a shown column would be confined to the stroke's own
    // width around one vertex, never a jump in height.
    //
    // ONLY the clip reads this. `right`, `pitch`, `bucketX`, the read window
    // and the left edge are untouched, so every column that is still shown
    // is pixel-for-pixel what 0.2.11 showed there. The strip is
    // `ceil (pitch) + 2` columns wide — 4 for every pitch up to 2 px, which is
    // every block up to 1024 samples at every rate from 44.1 kHz and 2048 from
    // 48 kHz up, on either well (44.1 kHz / 2048 on the Simple well is 5;
    // 4096-sample blocks add up to three more) — and since 0.2.12 the PANEL
    // does not pay for it: `paintHistory` draws its frame that many columns
    // further right, so the strip falls outside the plot and the plot shows
    // its full width (`hiddenColumns`). The lead-out itself stays in the path,
    // wholly behind the clip (`paintHistory` says why).
    //
    // That is the bound for every window of THREE OR MORE buckets, which is
    // every configuration a real-time host presents. `buckets` floors `kFull`
    // at 2, and a two-bucket window is the one geometry where the bound above
    // cannot be honoured at all — the next block states the cap that answers
    // it and what it costs.
    // WHAT THE STRIP MAY COST (0.2.12 review): `pitch` is `span / (kFull − 1)`
    // and `buckets` FLOORS `kFull` at 2 so the division is safe — so a window
    // holding two entries or fewer reports one pitch as the WHOLE span, and
    // `right − pitch` is then the left edge itself. Hiding a pitch there hid
    // the panel: `floor (right − pitch) − 1` came out one column LEFT of the
    // plot, the clip's width clamped to zero, and the GR history disappeared
    // — measured blank on 100 % of frames at 48 kHz / 480000 and
    // 44.1 kHz / 441000 on both wells. That geometry is `want ≤ 2`, which is
    // `blockSize ≥ 10 · sampleRate`: a host delivering ten seconds of audio in
    // one block, which offline renders do.
    //
    // The cap is `span / 2`, and it is not a rescue clamp — it is the same
    // bound one bucket further out. `pitch = span / 2` EXACTLY when
    // `kFull == 3`, so `min (pitch, span / 2)` leaves every geometry with
    // three or more buckets bit-for-bit as it was (the shipped configurations,
    // and every block shorter than `20 · sampleRate / 3` — 6.67 s at 48 kHz —
    // are all far above that), and it holds the boundary of a two-bucket
    // window where a three-bucket window would have put it: the plot's
    // mid-point. The boundary is therefore continuous and monotone in the
    // pitch across the whole block-size range, with no step at the two-bucket
    // transition, which a bare "if (kFull == 2)" special case could not
    // promise.
    //
    // WHAT A TWO-BUCKET WINDOW GIVES UP, stated rather than hidden: with
    // `kFull == 2` the newest drawn vertex sweeps the ENTIRE span once per
    // bucket (`bucketX`: `right − pitch ≤ x(kLast) ≤ right` with
    // `pitch == span`), so every column can carry the lead-out on some frame
    // and NO non-empty frame-independent boundary can exclude it. The two
    // requirements — hide the strip, show the history — are then genuinely
    // exclusive, and showing the history wins: a blank panel reports nothing
    // at all, and the fast right-edge artefact this bound exists to hide does
    // not exist at that geometry (one bucket completes every ten seconds, not
    // thirty times a second). A frame-DEPENDENT boundary would satisfy both
    // on paper and is rejected on sight: a clip that tracked `x(kLast)` would
    // move the plot's right edge at bucket rate, which is the class of defect
    // this whole round removes.
    //
    // The two clamps below are the total-function guards, both unreachable at
    // the shipped 904/604-column plots: a plot under two columns wide has no
    // boundary to give (`span < 1`, answered with an empty clip, as every
    // version before this one did for such a panel), and one under five keeps
    // its first column rather than losing it to the join margin.
    static int visibleRight (int64_t kFull, float x0, float width) noexcept
    {
        const float span = width - 1.0f;                // the anchor span, `right − x0`
        if (span < 1.0f)
            return (int) std::floor (x0);               // no plot: an empty clip, as before
        const float pitch  = span / (float) (kFull - 1);
        const float hidden = juce::jmin (pitch, 0.5f * span);
        return juce::jmax ((int) std::floor (x0) + 1,
                           (int) std::floor (x0 + span - hidden) - 1);
    }

    // The same boundary for a frame's own buckets. `buckets` needs the form
    // above while it is still building them, which is the only reason there
    // are two.
    static int visibleRight (const Buckets& b, float x0, float width) noexcept
    {
        return visibleRight (b.kFull, x0, width);
    }

    // THE COLUMNS THE BOUNDARY HIDES, and so how far right the trace is drawn
    // (0.2.12). `paintHistory` moves its whole drawing frame right by this
    // much, which lands the boundary on the plot's own right edge and leaves
    // the panel showing its full plot width — the width the spectrum view of
    // the same well shows, the two having identical bounds and the same
    // `reduced (10, 8)` inset. Nothing about the trace is scaled: the frame
    // keeps the area's WIDTH, so the pitch is what it was and every vertex
    // simply lands `hiddenColumns` further right than before.
    static int64_t hiddenColumns (int64_t kFull, int cols) noexcept
    {
        const int64_t c = juce::jmax ((int64_t) 1, (int64_t) cols);
        return c - (int64_t) visibleRight (kFull, 0.0f, (float) c);
    }

    // …and the buckets of OLDER history that shift needs on the left. Moving
    // the frame right frees `hiddenColumns` columns at the panel's left edge,
    // and they are filled by covering that many more pixels of history at the
    // SAME pitch — `ceil (hidden / pitch)` buckets, the whole point of the
    // exercise being that the trace is extended rather than stretched.
    // Bounded at four, which is also what the arithmetic gives: `hidden` is at
    // most `pitch + 3` (`visibleRight`'s floor plus its margin column), so the
    // quotient is at most `1 + 3 / pitch`, and `pitch ≥ 1` for every geometry
    // `buckets` produces (`kFull ≤ cols`).
    static constexpr int64_t kMaxLead = 4;

    static int64_t leadBuckets (int64_t kFull, int cols) noexcept
    {
        const int64_t c = juce::jmax ((int64_t) 1, (int64_t) cols);
        const float pitch = ((float) c - 1.0f) / (float) (kFull - 1);
        if (pitch <= 0.0f)
            return 0;
        return juce::jlimit ((int64_t) 0, kMaxLead,
                             (int64_t) std::ceil ((double) hiddenColumns (kFull, cols) / (double) pitch));
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
    //
    // `cleared` is the SAME re-anchor reached the other way (0.2.8 review,
    // finding 3). A clear rewinds the ring's write index, and the `previous >
    // hi` branch catches that — but only while the count is still below where
    // it was. A refill that has already returned the count to its old value
    // presents a head that has not visibly rewound at all, and carrying the
    // old sub-entry offset across it would draw the new history's first
    // frames one entry-pitch out. The epoch is what sees that case, so the
    // caller passes it in rather than this function guessing from the index.
    static double smoothedHead (double previous, int64_t head, double dt, double period,
                                bool cleared = false) noexcept
    {
        const double lo = (double) head, hi = lo + 1.0;
        if (cleared || previous > hi)
            return lo;
        return juce::jlimit (lo, hi, period > 0.0 ? previous + dt / period : previous);
    }

    // The fraction of the newest entry's period the smoothed head has
    // covered, 0 … 1 — what `bucketX` shifts the trace by.
    static double phaseOf (double smoothHead, int64_t head) noexcept
    {
        return juce::jlimit (0.0, 1.0, smoothHead - (double) head);
    }

    // The tick's idle gate: the history is the SAME history, no new entry has
    // arrived, and the smoothed head already sits at its cap — so nothing this
    // frame could draw differs from the last one, and the pre-0.2.8 rule
    // (repaint on new data only) is back in force after at most one entry
    // period of extra frames. A rewound head is "new" here and so un-parks.
    //
    // THE EPOCH IS PART OF THE IDENTITY (0.2.8 review, finding 3), because the
    // entry COUNT is not. `GrHistoryBuffer::reset()` rewinds the write index to
    // 0, so a clear followed by a refill to exactly the previous count presents
    // the tick with `head == shownHead` over completely different contents —
    // and the count-only gate parked on it and kept the old geometry on screen.
    // One frame if the transport keeps running (the next block makes the heads
    // differ), indefinitely if it stops right there, which is precisely when a
    // re-prepare happens. The ring already publishes the identity this needs:
    // `resetEpoch()` is bumped twice per clear, so any change — odd or even,
    // clear in flight or finished — means the contents are new. Ordinary
    // non-reset frames are unaffected: the epoch is constant across them, so
    // the gate is exactly the count rule it was.
    static bool parked (int64_t head, int64_t shownHead, double smoothHead,
                        uint32_t epoch, uint32_t shownEpoch) noexcept
    {
        return epoch == shownEpoch && head == shownHead && smoothHead >= (double) head + 1.0;
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

    // What one frame draws, from the two published scalars and the live write
    // index. It exists as a pure function because the two scalars are read
    // from the painting thread as SEPARATE relaxed atomics, so a frame may
    // pair a fresh `shownHead` with a stale `smoothHead` or the reverse, and
    // the claim that every such pairing is legal is a claim about VALUES that
    // a test can pin — the alternative being a consistent snapshot, i.e. a
    // new cross-thread mechanism for a display estimate that needs none.
    //
    // Why every pairing is legal: the trace's absolute position is
    // `−(head + phase)` entry-pitches from a fixed origin, `phase` is
    // `clamp (smoothHead − head, 0, 1)`, and `smoothHead ≥ head` holds for
    // every published pair (`smoothedHead`'s lower clamp) and therefore for
    // any CROSS pairing too, since both scalars only ever increase between
    // publications. So `head + phase` is `min (smoothHead, head + 1)` — a
    // value between two frames the ramp itself produces, never beyond them,
    // and non-decreasing across any sequence of pairings: the frame drawn
    // from a torn pair is a frame the display was about to draw anyway, and
    // no vertex ever moves rightward. A REWIND breaks the monotonicity, and
    // there the epoch guard discards the frame outright.
    struct Frame
    {
        int64_t head;    // the head this frame draws
        double  phase;   // 0 … 1 into the next entry's period
    };

    // `publishedEpoch` is the ring epoch the phase was computed under and
    // `liveEpoch` the one this frame is drawing. They differ exactly when a
    // clear has happened that the tick has not published for yet — the window
    // between `GrHistoryBuffer::reset()` and the next tick, which a paint can
    // fall into on its own schedule — and there the published phase belongs to
    // ring indices that no longer exist. A restarted timeline restarts at
    // phase 0, so that is what a frame draws until the tick republishes.
    //
    // The COUNT cannot stand in for the epoch here, and that is why this takes
    // one: after a clear the ring refills from 0, so `smoothHead - head` is
    // large and saturates the clamp to a phase of exactly 1 — one entry-pitch
    // out, which is the defect — and once the refill has passed the old count
    // (`shownHead <= live`, so `paintHead` no longer falls back) a stale
    // `smoothHead` is numerically indistinguishable from a current one. Only
    // the identity separates them.
    static Frame frameFor (int64_t shownHead, double smoothHead, uint32_t publishedEpoch,
                           int64_t live, uint32_t liveEpoch) noexcept
    {
        const int64_t h = paintHead (shownHead, live);
        return { h, publishedEpoch == liveEpoch ? phaseOf (smoothHead, h) : 0.0 };
    }

    // The oldest ring index a frame may read, given the producer's live write
    // index. `push` fills slot `live & kMask` and publishes afterwards, so the
    // reader must stay strictly inside one full lap of it: `live − n ≤
    // kSize − 1` for every peeked `n` (the ring's own `peek` comment argues
    // the same bound for the un-stale case, and `windowEntries` clamps to it).
    //
    // It takes the LIVE index rather than the head the frame draws, and that
    // is the whole fix (0.2.8 review, finding 1). `paintHead` deliberately
    // draws the head the TICK saw, which the audio thread may already have
    // moved past; deriving the floor from that stale head spent the one-slot
    // margin on the staleness, so at a SATURATED window (`window == kSize−1`,
    // reachable at 192 kHz/32 where ~100 blocks land per frame) a single
    // block arriving between the tick's read and the paint's read put the
    // oldest peek exactly on the slot the audio thread was filling. Clamping
    // `first` up to this floor costs nothing in every other case — the floor
    // is below `first` unless the window is saturated AND the head is stale —
    // and it moves no geometry: `bucketX` and `kFirst` never see it, only
    // which entries the oldest bucket aggregates.
    static int64_t readFloor (int64_t live) noexcept
    {
        return live - (int64_t) (anabasis::GrHistoryBuffer::kSize - 1);
    }

    // The entry range bucket `k` aggregates: its own span, clamped to the
    // frame's oldest readable index at the expiring end and to `head` at the
    // new end. For every DRAWN bucket BOTH clamps are now idle, so the range
    // is exactly `[k · stride, (k + 1) · stride)` — a constant of `k`, whose
    // value can never change between frames. The new-end clamp went idle at
    // 0.2.11 (`kLast`: a drawn bucket is complete; until then the newest
    // vertex read a trailing window that slid with every entry); the
    // expiring-end clamp at 0.2.12's review, when `buckets` began aligning
    // `first` to the oldest drawn bucket's own start and `paintHistory`
    // began asking `firstDrawn` which bucket that is. Both clamps stay,
    // because both state a bound this function must not be read without —
    // the caller's guarantee is what changed, not the arithmetic. Half-open,
    // and `e0 >= e1` means the bucket has nothing safe to read and is
    // skipped. Pure and public for the reason the geometry
    // statics are: the read set is where the ring-safety invariant lives,
    // and an invariant reachable only from `paint` is one no test can pin.
    struct Reads { int64_t e0, e1; };

    static Reads bucketReads (const Buckets& b, int64_t k, int64_t first, int64_t head) noexcept
    {
        return { juce::jmax (first, k * b.stride), juce::jmin (head, (k + 1) * b.stride) };
    }

    // The oldest bucket a FRAME may draw. `buckets` answers `kFirst` from the
    // window alone; this answers it from the RING, which the producer may
    // have moved under the batch (`readFloor` — the live index, not the
    // stale head the frame draws, and its banner argues why). A bucket whose
    // earliest entries have gone over the ring's edge cannot be drawn at the
    // value it has always had, and the one thing it must not do is come back
    // re-shaped from what is left of it — so it is DROPPED, which is what
    // "the bucket aged out of the backing window" honestly looks like. The
    // caller then reads from `firstDrawn · stride`, and every clamp in
    // `bucketReads` is idle for every bucket it draws.
    //
    // Idle at every ordinary window: the floor sits `kSize − 1` entries below
    // the LIVE head while `first` sits at most `kSize − stride` below the
    // drawn one, so this returns `kFirst` unless the window is saturated
    // (`want` at the clamp) AND the producer has lapped the batch — the
    // 192 kHz / 32 case `readFloor` was written for, where ~100 entries can
    // land between the tick and the paint. Dropping one bucket there costs
    // one pitch of the panel's oldest end for that frame; drawing it
    // truncated would cost the invariant.
    static int64_t firstDrawn (const Buckets& b, int64_t floorIndex) noexcept
    {
        if (floorIndex <= b.first)
            return b.kFirst;                            // the ordinary case, and every negative floor
        return juce::jmax (b.kFirst, (floorIndex + b.stride - 1) / b.stride);
    }

private:
    // The chip hit-area, in ONE place because `hitTest` and `mouseDown` must
    // agree about it — the rule `SpectrumView::chipHitArea` states.
    juce::Rectangle<int> chipHitArea() const noexcept;
    // The traces, split out of `paint` so the corner chip can be drawn AFTER
    // them (matching `SpectrumView`) without losing the reader contract's
    // three early returns — see the definition.
    void paintHistory (juce::Graphics&);

    AnabasisAudioProcessor& processor;
    abgui::FrameClock clock;

    // PUBLISHED by `tick` (message thread), READ by `paintHistory` (the thread
    // that paints — the message thread on Linux, the GL render thread on
    // macOS/Windows). Plain scalars here were a data race and therefore
    // undefined behaviour on those two platforms, whatever the generated code
    // happened to do (0.2.8 review, finding 2): the fix is THREADING_POLICY's
    // Message → Painting row as ADR-0027 defines it — relaxed atomics, written
    // on one thread, read-only on the painting side, ordering no other memory
    // and carrying no handshake. `memory_order_relaxed` is the whole
    // requirement: a one-frame-stale read draws a frame the ramp itself
    // produces (`frameFor`), so there is nothing for an acquire to order.
    //
    // The `static_assert` is not decoration. A platform where these are not
    // lock-free would put a lock in the paint path, which is a different
    // decision from the one taken here and must fail the build rather than
    // ship quietly.
    static_assert (std::atomic<int64_t>::is_always_lock_free
                       && std::atomic<double>::is_always_lock_free
                       && std::atomic<uint32_t>::is_always_lock_free,
                   "the painting thread reads these without blocking, or not at all");
    std::atomic<int64_t> shownHead  { -1 };   // repaint gate: new entries arrived
    std::atomic<double>  smoothHead { 0.0 };  // `smoothedHead`: the head advanced at the nominal rate

    // THE VALIDITY OF THE OTHER TWO, and the one access on this boundary that
    // is not relaxed. `tick` stores it LAST with `release`; `paintHistory`
    // loads it FIRST with `acquire`. That pair is the invariant the reset fix
    // rests on: a frame that sees this epoch also sees the `smoothHead` and
    // `shownHead` published under it, so "the epoch matches" is a statement
    // about the phase this frame can actually read rather than about a value
    // that may still be in flight. A relaxed store here would be enough on a
    // TSO target and NOT on a weakly ordered one, where the epoch could become
    // visible before the phase it announces and hand a fresh frame the stale
    // pre-reset offset — the exact defect this exists to close, reintroduced
    // through the back door. It orders those two publications and nothing
    // else; no payload crosses, and the audio thread is not involved.
    std::atomic<uint32_t> publishedEpoch { 0 };

    // Message thread ONLY — the tick's memory of which history it last drew
    // (`parked`). The painting side takes its epoch from the ring and from
    // `publishedEpoch`, and never reads this (ADR-0038 clause 5).
    uint32_t shownEpoch = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrHistoryView)
};
