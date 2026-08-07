#include "GrHistoryView.h"
#include "../PluginProcessor.h"

using namespace abgui;

GrHistoryView::GrHistoryView (AnabasisAudioProcessor& p) : processor (p)
{
    // This view used to opt out of the mouse WHOLESALE
    // (`setInterceptsMouseClicks (false, false)`) — it had no affordance. Since
    // the combined graph well (2026-08-05) it owns the "SPEC" mode chip, so it
    // opts out per-pixel through `hitTest` instead, exactly as `SpectrumView`
    // does for its chip: JUCE's default interception stays on, and `hitTest`
    // declines every point outside the chip.
    //
    // The tooltip therefore fires over the mode switch only — the same string
    // as `SpectrumView`'s, because it is one control drawn twice.
    setTooltip ("Switch the graph between the spectrum and the GR history");
}

// The mode switch's hit-area — the shared SPEC|GR pill (`abgui::graph_switch`,
// 0.1.1; ONE geometry for both views, expanded 2 px for the touch target).
// `hitTest` and `mouseDown` key on this one rectangle — computed separately,
// a click the view accepts but then ignores creeps back in one pixel at a time.
juce::Rectangle<int> GrHistoryView::chipHitArea() const noexcept
{
    return graph_switch::bounds (getWidth()).expanded (2);
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
    // Segment semantics — see `SpectrumView::mouseDown`: the click selects the
    // side of the divider it falls on; the already-active GR segment is a no-op.
    if (e.getPosition().getX() < graph_switch::bounds (getWidth()).getCentreX())
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

    // The shared SPEC|GR mode switch — translucent, so the GR zero-line it can
    // overlap stays readable beneath it (the single-name chip this replaces sat
    // exactly on that line and was masked by it).
    graph_switch::paint (g, getWidth(), false);
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
    const int64_t first = juce::jmax<int64_t> (0, head - want);
    const int64_t count = head - first;
    if (count <= 0)
        return;

    auto area = getLocalBounds().toFloat().reduced (10.0f, 8.0f);
    const float grSpan = 12.0f;                     // dB of visible reduction
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
        const float gy = area.getY()
                       + area.getHeight() * juce::jlimit (0.0f, 1.0f, -grDb / grSpan) * 0.5f;
        if (! started)
        {
            wave.startNewSubPath (x, wy);
            gr.startNewSubPath (x, gy);
            started = true;
        }
        else
        {
            wave.lineTo (x, wy);
            gr.lineTo (x, gy);
        }
        lastX = x;

        // ONE bucket is the whole history, and it has no second vertex to
        // stretch to: `bucketX` degenerates to the left edge, a one-point
        // polyline strokes NOTHING and a one-point fill closes a zero-width
        // shape, so the panel went blank for the first few blocks after every
        // reset and every transport start (three blocks ≈ 32 ms at 48 kHz/512,
        // where `stride` is 3). The limit of the stretch as the bucket count
        // falls to one is a CONSTANT trace, so the single reading is emitted at
        // BOTH edges — the same two x's the two-bucket case already uses, and
        // continuous with it. The rectangles the pre-0.1.1 draw used are not
        // coming back: a filled path under a polyline is the design, and this
        // is that design's own degenerate case handled inside it.
        if (nb.count == 1)
        {
            const float xEnd = area.getRight() - 1.0f;   // == bucketX's right edge
            wave.lineTo (xEnd, wy);
            gr.lineTo (xEnd, gy);
            lastX = xEnd;
        }
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
