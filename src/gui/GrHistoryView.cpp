#include "GrHistoryView.h"
#include "../PluginProcessor.h"

using namespace abgui;

GrHistoryView::GrHistoryView (AnabasisAudioProcessor& p) : processor (p)
{
    // This view used to opt out of the mouse WHOLESALE
    // (`setInterceptsMouseClicks (false, false)`) — it had no affordance. Since
    // the combined graph well (2026-08-05) it owns the GR|SPEC mode pill, so it
    // opts out per-pixel through `hitTest` instead, exactly as `SpectrumView`
    // does for its copy: JUCE's default interception stays on, and `hitTest`
    // declines every point outside the pill.
    //
    // The tooltip therefore fires over the mode switch only — the same string
    // as `SpectrumView`'s, because it is one control drawn twice.
    setTooltip ("Switch the graph between the spectrum and the GR history");
}

// The mode switch's hit-area — the shared GR|SPEC pill (`abgui::graph_switch`,
// 0.1.1, reworked 0.1.2; ONE geometry for both views, expanded 2 px for the
// touch target). `hitTest` and `mouseDown` key on this one rectangle —
// computed separately, a click the view accepts but then ignores creeps back
// in one pixel at a time.
juce::Rectangle<int> GrHistoryView::chipHitArea() const noexcept
{
    return graph_switch::bounds (getWidth(), getHeight()).expanded (2);
}

bool GrHistoryView::hitTest (int x, int y)
{
    return chipHitArea().contains (x, y);
}

void GrHistoryView::mouseDown (const juce::MouseEvent& e)
{
    // Unreachable-false today (`hitTest` already refused everything outside the
    // pill) and kept so this function is correct standing alone — the same
    // reasoning `SpectrumView::mouseDown` records.
    if (! chipHitArea().contains (e.getPosition()))
        return;
    // The whole pill is ONE toggle (0.1.2 item 5): from the GR view any press
    // inside it switches the well to the spectrum. The 0.1.1 side-of-divider
    // semantics made a press on the active segment a silent no-op, which read
    // as a stuck control.
    processor.internalState.state().setProperty (iid::spectrumOn, true, nullptr);
}

void GrHistoryView::visibilityChanged()
{
    if (isVisible())
        clock.start (*this, [this] (double dt) { tick (dt); });
    else
        clock.stop();
}

void GrHistoryView::tick (double dt)
{
    const auto& ring = processor.grHistory();
    // The epoch is sampled BEFORE the index, so a clear that lands between the
    // two is attributed to the newer epoch and re-anchors on the next tick
    // rather than being missed: the tick's job here is only to decide whether
    // the history is the same history, and "not sure" must mean "not parked".
    const auto epoch    = ring.resetEpoch();
    const auto prepared = ring.prepared();
    const auto head     = ring.available();
    // The pair and the head were read under `epoch`; if a clear ran through
    // that read the pair may be half old and half new, so this tick publishes
    // nothing and the next re-derives from a settled epoch (`batchIntact` is
    // the fence-and-re-read the seqlock reader needs, not a second load).
    // Before 0.2.8's final review this read `getSampleRate()`/`getBlockSize()`
    // — `AudioProcessor`'s plain members, written by the host's callback
    // thread while this thread ticks: a data race on exactly the values the
    // scroll rate is derived from.
    if ((epoch & 1u) != 0u || ! ring.batchIntact (epoch))
        return;
    const auto shown = shownHead.load (std::memory_order_relaxed);
    // PARKED: the same history, no new entry, and the smoothed head already
    // one whole entry ahead of it (`smoothedHead`'s cap) — so nothing this
    // frame could draw differs from the last one and the pre-0.2.8 idle gate,
    // repaint on new data only, is back in force. A stopped transport costs
    // at most one entry period of extra frames after its last block, never a
    // paint per vblank. The EPOCH is what makes "the same history" mean the
    // contents rather than the count; see `parked`.
    if (parked (head, shown, smoothHead.load (std::memory_order_relaxed), epoch, shownEpoch))
        return;

    // A CLEAR RESTARTS THE TIMELINE, so the phase restarts with it: the ring
    // rewinds its write index to 0, and carrying the old sub-entry offset
    // across that would draw the first frames of the new history one
    // entry-pitch out. This is the same re-anchor `smoothedHead` performs for
    // a rewound head, reached here by the epoch instead — which is the only
    // way to see it when the refill has already returned the count to where
    // it was.
    const double smoothed = smoothedHead (smoothHead.load (std::memory_order_relaxed), head, dt,
                                          entryPeriod (prepared.rate, prepared.block),
                                          epoch != shownEpoch);

    // Publish for the painting thread. `smoothHead` FIRST so a paint landing
    // between the two stores pairs a fresh phase with the older head, which
    // `frameFor` resolves to `min (smoothHead, head + 1)` — the frame this
    // tick is about to draw, never one beyond it. The reverse order would let
    // a frame pair the new head with the previous phase, which is legal too
    // (also non-decreasing); the order is fixed here so the reasoning has one
    // case rather than two.
    smoothHead.store (smoothed, std::memory_order_relaxed);
    shownHead.store (head, std::memory_order_relaxed);
    // …and the epoch those two were computed under, LAST and with RELEASE, so
    // a frame that sees it sees them (the member's comment carries the whole
    // argument). This is what makes the phase's validity checkable rather than
    // assumed: until this store lands, a paint after a clear reads an epoch
    // that no longer matches the ring's and anchors at phase 0.
    publishedEpoch.store (epoch, std::memory_order_release);
    shownEpoch = epoch;
    repaint();
}

void GrHistoryView::paint (juce::Graphics& g)
{
    // TRACES FIRST, CHIP LAST — the same order `SpectrumView::paint` uses, so
    // the two chips genuinely look alike rather than only claiming to. Drawn
    // the other way round, the GR trace (which sits at `area.getY()`, ~8 px,
    // whenever there is no reduction) and the waveform run the full width and
    // cross the glyph.
    //
    // The history is a SEPARATE function because it early-returns three times
    // on the reader contract (odd epoch, empty window, epoch moved), and the
    // chip has to survive all three: the way back to the spectrum must not
    // disappear because the ring is empty or a clear is in flight. Returning
    // from `paintHistory` skips the traces, never the chip.
    paintHistory (g);

    // The shared GR|SPEC mode switch — translucent, and in the BOTTOM-LEFT
    // since 0.1.2 (item 4): the top-right it used to occupy is where the
    // newest reduction lands, the data the user is actively watching. Down
    // here it floats over the waveform's oldest end — the empty zero region
    // until a full window has played.
    graph_switch::paint (g, getWidth(), getHeight(), false);
}

void GrHistoryView::paintHistory (juce::Graphics& g)
{
    const auto& ring = processor.grHistory();

    // Epoch-guarded batch (the decided reader contract — header banner).
    const auto epoch0 = ring.resetEpoch();
    if ((epoch0 & 1u) != 0u)
        return;                                     // clear in flight: skip the frame

    // The LIVE write index — where the producer is now — and the frame the
    // two published scalars describe. The live index has two jobs and they
    // are different: `frameFor` uses it to decide whether the tick's head is
    // still usable (`paintHead`), and `readFloor` uses it to bound which
    // slots this batch may touch. Reading it ONCE keeps both answers derived
    // from the same observation.
    const int64_t live  = ring.available();
    // ACQUIRE FIRST, then the two relaxed loads: the pair with `tick`'s
    // release store is what makes a matching epoch mean "the phase published
    // under it is visible to this frame". Reading it after them would order
    // nothing and leave the reset defect open on weakly ordered targets.
    const auto    pubEpoch = publishedEpoch.load (std::memory_order_acquire);
    const auto    frame = frameFor (shownHead.load (std::memory_order_relaxed),
                                    smoothHead.load (std::memory_order_relaxed),
                                    pubEpoch, live, epoch0);
    const int64_t head  = frame.head;

    // The clamp lives in `windowEntries` (header) so it is testable without a
    // graphics context; `kSize - 1` and the reason for it are stated there.
    // Reachable at ordinary block sizes: 20 s at 48 kHz / 64 samples is 15000
    // entries, so `want` saturates for anything up to ~234 samples per block.
    // The pair is the RING's (`prepared`), read under this batch's epoch like
    // everything else the frame maps — see `tick` for what it replaced.
    const auto    prepared = ring.prepared();
    const int64_t want = windowEntries (prepared.rate, prepared.block);
    if (head <= 0)
        return;

    auto area = getLocalBounds().toFloat().reduced (10.0f, 8.0f);
    const int cols = juce::jmax (1, (int) area.getWidth());

    // FIXED-IDENTITY decimation buckets + an area fill (0.1.1, owner shimmer
    // report). The previous draw re-derived each column's entry range from the
    // MOVING window start every frame — `(count * cx) / cols` against `first`
    // — so as the ring advanced a few entries per tick, every bucket boundary
    // re-phased against the entries and each column's decimated max flickered
    // between neighbours; rendered as `cols` separate 1 px rectangles, the
    // result was a comb whose teeth shimmered while scrolling (most visible on
    // a HiDPI display, where the 1 px bars also land on half-pixels).
    //
    // Buckets are now keyed to ABSOLUTE ring indices: a constant integer
    // `stride` of entries per bucket, bucket k spanning [k·stride, (k+1)·stride).
    // A given entry therefore stays in the same bucket for its whole life on
    // screen, so a completed bucket's decimated max never changes and the
    // display scrolls instead of re-bucketing. Since 0.2.11 a bucket is drawn
    // ONLY once it is complete (`Buckets::kLast`), so no drawn value ever
    // changes at all — the still-filling bucket used to be drawn live, and
    // the owner saw its vertex being revised (the banner over `bucketX`).
    //
    // The waveform is ONE filled path under a polyline top edge rather than a
    // per-column rectangle comb, so the renderer anti-aliases a single shape.
    //
    // BUCKET COUNT AND PIXEL COLUMN ARE SEPARATE QUESTIONS. Conflating them —
    // one bucket per column, anchored at the newest — is what left a permanent
    // blank strip on the left, because `stride` rounds up and the window only
    // ever holds `want` entries, not `cols·stride` of them. `buckets` and
    // `bucketX` carry that arithmetic and its argument; they live in the header
    // for the reason `windowEntries` does — a version reachable only from
    // `paint` is a version no test can pin, which is how this went unnoticed.
    //
    // SCROLLED ONE ENTRY AT A TIME (0.2.8, the owner's jitter report — the
    // banner above `bucketX` carries the mechanism and the numbers). The
    // buckets are the same; what changed is that `bucketX` now places them by
    // the newest ENTRY's sub-bucket position and by how far the frame clock
    // says this frame sits inside that entry's period, so the trace advances
    // ~0.5–1 px every frame instead of standing still for two and lurching
    // a whole pitch on the third. The `phase` half is why this frame reads
    // `smoothHead`, which `tick` advances.
    const auto nb = buckets (head, want, cols, frame.phase);

    // The read window is `nb.window` / `nb.first` — bucket-aligned, argued in
    // `buckets` — held ABOVE the ring floor the producer's live position
    // imposes (`readFloor`, the 0.2.8 review's finding 1). The floor binds
    // only when the window is saturated AND the drawn head is stale, and it
    // moves no geometry: `bucketX` and `kFirst` never see it, only which
    // entries the oldest bucket aggregates.
    const int64_t first = juce::jmax (nb.first, readFloor (live));
    if (first >= head)
        return;                                     // the producer has lapped everything this
                                                    // frame could draw; blank is the honest
                                                    // answer and the next tick re-derives
    juce::Path wave, gr;
    bool started = false;
    float lastX = area.getX(), lastWy = 0.0f, lastGy = 0.0f;
    // The anchor: the newest COMPLETE bucket lands here the frame it
    // completes (`bucketX`). It is the left boundary of the last plot column,
    // which is why the lead-out below runs on past it to the clip edge.
    const float right = area.getX() + area.getWidth() - 1.0f;

    // Clip to the plot area's COLUMNS (the rows keep their overhang: the
    // 1.4 px stroke at zero reduction straddles `area.getY()` and must go on
    // doing so). The oldest drawn bucket now sits ON or up to a pitch OFF the
    // left edge (`buckets`), and drawing it there lets its segment cross the
    // edge exactly — the alternative was the flat lead-in, which walked and
    // jumped at bucket rate. What the clip costs is the 0.7 px of end-cap
    // the stroke used to spill into the 10 px margin, which is the margin's,
    // not the trace's.
    const juce::Graphics::ScopedSaveState clipState (g);
    g.reduceClipRegion (juce::Rectangle<float> (area.getX(), 0.0f,
                                                area.getWidth(), (float) getHeight())
                            .toNearestInt());

    // The UNMEASURED region (0.1.2 item 3): everything left of the oldest
    // bucket has no history behind it — the ring has not lived that long —
    // and it is drawn as ZERO data (waveform at the floor, GR at its zero
    // line), never estimated, interpolated or stretched over. The step where
    // the zero region meets the first bucket is the honest boundary between
    // "not measured" and "measured". This also covers the one-bucket first
    // frames after a reset (the newest bucket sits at the right edge by
    // anchoring, and this region is the rest), which the stretch draw needed
    // a special case for.
    //
    // ONLY while the ring is still filling, and that guard is the 0.1.3
    // left-edge fix. A FULL scrolling window reaches this branch too:
    // `kFirst` alternates between the width bound and the window bound as
    // buckets expire (stride-boundary phase), so `xFirst` oscillates between
    // the left edge and one pitch inside it — and whenever it sat one pitch
    // in, this branch drew the GR zero line along the top to `xFirst` and the
    // bucket loop's first point then dropped VERTICALLY to the real trace
    // value at the same x. With any reduction on screen that rendered as an
    // accent-coloured vertical bar at the left edge, flashing at the
    // bucket-expiry rate (the owner's 0.1.3 item 4 report). Left of the
    // oldest bucket of a full window is EXPIRED history, not unmeasured data,
    // so the zero-data presentation does not apply there — the bucket loop
    // below extends the oldest bucket's values to the edge instead.
    //
    // The predicate lives in the header (`drawsZeroRegion`) rather than here,
    // for the reason `windowEntries` and `buckets` do: a rule reachable only
    // from `paint` is a rule no test can pin, which is how this defect
    // survived the round that introduced it.
    //
    // The paths START in this branch whatever `xFirst` is (0.2.8): the zero
    // line runs from the left edge — or from `xFirst` itself once that has
    // scrolled past the edge — and the loop's first vertex then drops to the
    // measured value at the same x, so the boundary bar slides out UNDER the
    // clip over the last frames of the fill instead of vanishing whole the
    // frame `xFirst` crossed a half-pixel guard. The window the predicate
    // reads is the ALIGNED one (`nb.window`), so the filling and scrolling
    // cases agree about which frame is the changeover.
    const float zeroWy = area.getBottom() - 0.5f;       // == the wh floor below
    const float zeroGy = grY (0.0f, area.getY(), area.getHeight());   // the GR zero line
    if (drawsZeroRegion (head, nb.window))
    {
        // Until the very first bucket completes (`count == 0`, the first
        // `stride − 1` blocks after a reset) there is no vertex to drop into:
        // the zero line runs to the anchor and the lead-out carries it on to
        // the edge, so a just-reset ring shows the honest zero line across
        // the whole panel rather than nothing.
        const float xFirst = nb.count > 0 ? bucketX (nb, nb.kFirst, area.getX(), area.getWidth())
                                          : right;
        const float xZero  = juce::jmin (area.getX(), xFirst);
        wave.startNewSubPath (xZero, zeroWy);
        gr.startNewSubPath (xZero, zeroGy);
        if (xFirst > xZero + 0.01f)
        {
            wave.lineTo (xFirst, zeroWy);
            gr.lineTo (xFirst, zeroGy);
        }
        lastX  = xFirst;
        lastWy = zeroWy;
        lastGy = zeroGy;
        started = true;
    }

    // Complete buckets only (`kLast`, 0.2.11): every vertex this loop emits
    // reads its bucket's whole span and will read the same span on every
    // later frame, so nothing drawn here is ever revised — the half-collected
    // newest bucket, which used to be drawn live at the edge, waits.
    for (int64_t k = nb.kFirst; k <= nb.kLast; ++k)
    {
        const auto [e0, e1] = bucketReads (nb, k, first, head);
        if (e0 >= e1)
            continue;                               // unreachable by construction (see the
                                                    // header); kept because an empty bucket
                                                    // would plot a false zero, not nothing
        float peak = 0.0f, grDb = 0.0f;
        for (int64_t e = e0; e < e1; ++e)
        {
            const auto entry = ring.peek (e);
            peak = juce::jmax (peak, entry.peak);
            grDb = juce::jmin (grDb, entry.grDb);
        }
        const float x  = bucketX (nb, k, area.getX(), area.getWidth());
        const float wh = juce::jmax (0.5f, area.getHeight() * juce::jlimit (0.0f, 1.0f, peak));
        const float wy = area.getBottom() - wh;
        // 0.1.6 item 1: `grY`, which spends the WHOLE panel height on
        // `abgui::meters::grSpanDb` — see the header. The expression that used
        // to sit here stopped at half the height and went flat past 12 dB.
        const float gy = grY (grDb, area.getY(), area.getHeight());
        if (! started)
        {
            // A full scrolling window (the filling case starts its paths in
            // the zero-region branch above). Since 0.2.8 the oldest drawn
            // bucket sits on or beyond the left edge whenever the read window
            // is bucket-aligned (`buckets`), so `x ≤ xEdge` here, the path
            // simply starts at the vertex and the clip does the rest. The
            // lead-in below — the oldest bucket's values extended flat to the
            // edge — is now reached only by the unaligned fallback (a
            // saturated ring whose stride does not divide `kSize − 1`, which
            // neither of this product's plot widths produces), where it is
            // still the 0.1.3 answer to the strip of expired history the
            // truncated bucket leaves: bounded by the same one bucket of
            // truncation ADR-0023 item 6 accepts at this edge.
            const float xEdge = area.getX();
            wave.startNewSubPath (juce::jmin (x, xEdge), wy);
            gr.startNewSubPath (juce::jmin (x, xEdge), gy);
            if (x > xEdge + 0.01f)
            {
                wave.lineTo (x, wy);
                gr.lineTo (x, gy);
            }
            started = true;
        }
        else
        {
            wave.lineTo (x, wy);
            gr.lineTo (x, gy);
        }
        lastX  = x;
        lastWy = wy;
        lastGy = gy;
    }

    // THE BATCH LOST A RACE — either half of the reader contract (banner).
    // A moved epoch means a clear ran through it; a producer that has advanced
    // past this batch's floor means the audio thread has lapped the oldest
    // slots the batch peeked, so those values may have been overwritten while
    // they were read. Both discard the frame and let the next tick re-derive;
    // one dropped frame is the seqlock bargain the ring's own header states.
    // `batchIntact` FIRST: it carries the acquire fence that orders every
    // peek above before either re-read. Written as two SEQUENCED STATEMENTS
    // rather than one `||`, deliberately. `||`'s left-to-right sequencing is
    // guaranteed and the previous form was correct, but the ordering is now
    // load-bearing for the LAP check as well as the epoch — since 0.2.8's
    // round 6 the producer release-fences before its payload stores, and the
    // whole guarantee is that the peeks are sequenced before this fence and
    // the `available()` re-read after it. A future edit that reordered the
    // operands, or hoisted the re-read for tidiness, would silently delete
    // that guarantee and no test could see it. One always-evaluated atomic
    // load on the paint path is the price of making the requirement visible.
    const bool    intact = ring.batchIntact (epoch0);   // carries the acquire fence
    const int64_t live2  = ring.available();            // sequenced after it
    if (! intact || first < readFloor (live2))
        return;
    if (! started)
        return;

    // The LEAD-OUT, mirror of the left edge's lead-in: the strip between the
    // newest drawn vertex and the edge — up to a pitch plus one entry-pitch
    // wide now that a bucket is drawn only once complete — holds no data yet,
    // so the last value extends flat across it rather than leaving the line
    // short of the edge and breathing at the block rate. It runs to the CLIP
    // edge, not to the anchor `right` (0.2.11): `right` is the LEFT boundary
    // of the last plot column, and a butt-capped horizontal stroke ending
    // there lit nothing in that column while a steep segment ending at the
    // same x spilled half its width into it — so the last column blinked at
    // bucket rate (measured dark on 52 % of frames), the very breathing this
    // lead-out was written to prevent. Ending at the clip edge makes the
    // column part of the lead-out on every frame; the clip trims the cap.
    const float edge = area.getRight();
    if (lastX < edge - 0.01f)
    {
        wave.lineTo (edge, lastWy);
        gr.lineTo (edge, lastGy);
        lastX = edge;
    }

    // Close the waveform's top edge down to the baseline and fill.
    juce::Path waveFill (wave);
    waveFill.lineTo (lastX, area.getBottom());
    waveFill.lineTo (wave.getBounds().getX(), area.getBottom());
    waveFill.closeSubPath();
    g.setColour (colours::textDim.withAlpha (0.35f));
    g.fillPath (waveFill);
    g.setColour (colours::accent);
    g.strokePath (gr, juce::PathStrokeType (1.4f));
}
