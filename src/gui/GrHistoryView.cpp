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

void GrHistoryView::tick (double)
{
    const auto& ring = processor.grHistory();
    const auto head = ring.available();
    if (head != shownHead)
    {
        shownHead = head;
        repaint();
    }
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
    const int64_t head = ring.available();

    // The clamp lives in `windowEntries` (header) so it is testable without a
    // graphics context; `kSize - 1` and the reason for it are stated there.
    // Reachable at ordinary block sizes: 20 s at 48 kHz / 64 samples is 15000
    // entries, so `want` saturates for anything up to ~234 samples per block.
    const int64_t want  = windowEntries (processor.getSampleRate(), processor.getBlockSize());
    const int64_t first = juce::jmax ((int64_t) 0, head - want);
    const int64_t count = head - first;
    if (count <= 0)
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
    // display scrolls instead of re-bucketing. Only the newest (still-filling)
    // bucket changes between shifts, and its max can only grow while it fills.
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
    const auto nb = buckets (head, want, cols);
    juce::Path wave, gr;
    bool started = false;
    float lastX = area.getX();

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
    const float zeroWy = area.getBottom() - 0.5f;       // == the wh floor below
    const float zeroGy = grY (0.0f, area.getY(), area.getHeight());   // the GR zero line
    if (drawsZeroRegion (head, want))
    {
        const float xFirst = bucketX (nb, nb.kFirst, area.getX(), area.getWidth());
        if (xFirst > area.getX() + 0.5f)
        {
            wave.startNewSubPath (area.getX(), zeroWy);
            gr.startNewSubPath (area.getX(), zeroGy);
            wave.lineTo (xFirst, zeroWy);
            gr.lineTo (xFirst, zeroGy);
            started = true;
        }
    }

    for (int64_t k = nb.kFirst; k <= nb.kHead; ++k)
    {
        const int64_t e0 = juce::jmax (first, k * nb.stride);
        const int64_t e1 = juce::jmin (head, (k + 1) * nb.stride);
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
            // the zero-region branch above): the sub-pitch strip left of the
            // truncated oldest bucket holds expired history, so the oldest
            // bucket's own values extend to the panel edge — a horizontal
            // ≤ one-pitch lead-in that scrolls seamlessly, where the previous
            // zero-line start drew the flashing vertical bar the banner above
            // describes. The lie is bounded by the same one bucket of
            // truncation ADR-0023 item 6 already accepts at this edge.
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
        lastX = x;
    }

    // The batch raced a reset: throw the frame away, the next tick re-derives.
    if (ring.resetEpoch() != epoch0)
        return;
    if (! started)
        return;

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
