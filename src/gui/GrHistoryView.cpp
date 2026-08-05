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
}

// The mode chip's hit-area (top-right corner). ONE definition, because
// `hitTest` and `mouseDown` both key on it — computed separately, a click the
// view accepts but then ignores creeps back in one pixel at a time. Larger
// than the drawn chip (`paint` puts "SPEC" at `getWidth() - 40, 4, 36 × 16`):
// the surplus is the touch target, the same proportions as `SpectrumView`'s.
juce::Rectangle<int> GrHistoryView::chipHitArea() const noexcept
{
    return { getWidth() - 41, 0, 41, 24 };
}

bool GrHistoryView::hitTest (int x, int y)
{
    return chipHitArea().contains (x, y);
}

void GrHistoryView::mouseDown (const juce::MouseEvent& e)
{
    // Unreachable-false today (`hitTest` already refused everything outside the
    // chip) and kept so this function is correct standing alone — the same
    // reasoning `SpectrumView::mouseDown` records.
    if (chipHitArea().contains (e.getPosition()))
        // MODE SWITCH: the chip names what you switch TO. The spectrum view
        // carries the mirrored "GR" chip back; Settings no longer owns this
        // field (the toggle was removed with the combined well).
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
    // Corner mode chip first, BEFORE the data guards below: the way back to the
    // spectrum must not disappear just because the ring is empty or a clear is
    // in flight. Same face as `SpectrumView`'s "GR" chip.
    g.setColour (colours::textDim.withAlpha (0.7f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)).withExtraKerningFactor (0.1f));
    g.drawText ("SPEC", getWidth() - 40, 4, 36, 16, juce::Justification::centred);

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

    // One pass per pixel column, decimating entries to (peakMax, grMin).
    const int cols = juce::jmax (1, (int) area.getWidth());
    juce::Path wave, gr;
    bool grStarted = false;

    for (int cx = 0; cx < cols; ++cx)
    {
        const int64_t e0 = first + (count * cx) / cols;
        const int64_t e1 = juce::jmax (e0 + 1, first + (count * (cx + 1)) / cols);
        float peak = 0.0f, grDb = 0.0f;
        for (int64_t e = e0; e < e1; ++e)
        {
            const auto entry = ring.peek (e);
            peak = juce::jmax (peak, entry.peak);
            grDb = juce::jmin (grDb, entry.grDb);
        }
        const float x = area.getX() + (float) cx;
        const float wh = area.getHeight() * juce::jlimit (0.0f, 1.0f, peak);
        wave.addRectangle (x, area.getBottom() - wh, 1.0f, juce::jmax (wh, 0.5f));

        const float gy = area.getY()
                       + area.getHeight() * juce::jlimit (0.0f, 1.0f, -grDb / grSpan) * 0.5f;
        if (! grStarted) { gr.startNewSubPath (x, gy); grStarted = true; }
        else             gr.lineTo (x, gy);
    }

    // The batch raced a reset: throw the frame away, the next tick re-derives.
    if (ring.resetEpoch() != epoch0)
        return;

    g.setColour (colours::textDim.withAlpha (0.35f));
    g.fillPath (wave);
    g.setColour (colours::accent);
    g.strokePath (gr, juce::PathStrokeType (1.4f));
}
