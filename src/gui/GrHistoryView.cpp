#include "GrHistoryView.h"
#include "../PluginProcessor.h"

using namespace abgui;

GrHistoryView::GrHistoryView (AnabasisAudioProcessor& p) : processor (p)
{
    setInterceptsMouseClicks (false, false);
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
    const auto& ring = processor.grHistory();

    // Epoch-guarded batch (the decided reader contract — header banner).
    const auto epoch0 = ring.resetEpoch();
    if ((epoch0 & 1u) != 0u)
        return;                                     // clear in flight: skip the frame
    const int64_t head = ring.available();

    const double sr    = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
    const int    block = juce::jmax (1, processor.getBlockSize());
    const int64_t want = juce::jmin<int64_t> (
        anabasis::GrHistoryBuffer::kSize,
        (int64_t) std::ceil (kWindowSeconds * sr / (double) block));
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
