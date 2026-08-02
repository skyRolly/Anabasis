#include "SpectrumView.h"
#include "../PluginProcessor.h"

using namespace abgui;

SpectrumView::SpectrumView (AnabasisAudioProcessor& p) : processor (p)
{
    scratchL.resize (kSize);
    scratchR.resize (kSize);
    fftData.resize ((size_t) kSize * 2);
    inDb.assign (kBins, -120.0f);
    outDb.assign (kBins, -120.0f);
    setTooltip ("Spectrum");   // owner wording TODO (C8); identifier only
}

void SpectrumView::visibilityChanged()
{
    if (isVisible())
        clock.start (*this, [this] (double dt) { tick (dt); });
    else
        clock.stop();
}

void SpectrumView::mouseDown (const juce::MouseEvent& e)
{
    // The dismiss affordance (brief §6 "visible until dismissed"): the ×
    // hit-area in the top-right corner. Re-enabled from Settings, which owns
    // the same int_spectrumOn field.
    if (e.getPosition().getX() > getWidth() - 26 && e.getPosition().getY() < 24)
        processor.internalState.state().setProperty (iid::spectrumOn, false, nullptr);
}

void SpectrumView::analyse (const anabasis::ScopeBuffer& ring,
                            std::vector<float>& smoothedDb, double dt)
{
    const int got = ring.readLatest (scratchL.data(), scratchR.data(), kSize);
    if (got <= 0)
        return;

    std::fill (fftData.begin(), fftData.end(), 0.0f);
    const int off = kSize - got;                          // zero-pad a short read
    for (int i = 0; i < got; ++i)
        fftData[(size_t) (off + i)] = 0.5f * (scratchL[(size_t) i] + scratchR[(size_t) i]);
    window.multiplyWithWindowingTable (fftData.data(), kSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    // Hann coherent gain 0.5; normalise so a full-scale sine reads ~0 dB.
    const float norm = 2.0f / ((float) kSize * 0.5f);
    // EMA per bin, dt-corrected (~120 ms), so the trace is readable without
    // hiding programme changes. Attack instant, decay smoothed — peaks show.
    const float decay = 1.0f - std::exp ((float) (-dt / 0.12));
    for (int b = 0; b < kBins; ++b)
    {
        const float mag = fftData[(size_t) b] * norm;
        const float db  = juce::Decibels::gainToDecibels (mag, -120.0f);
        float& s = smoothedDb[(size_t) b];
        s = db > s ? db : s + (db - s) * decay;
    }
}

void SpectrumView::tick (double dt)
{
    const anabasis::ScopeBuffer& in  = processor.spectrumInRing();
    const anabasis::ScopeBuffer& out = processor.spectrumOutRing();
    const auto ci = in.writeCount();
    const auto co = out.writeCount();
    if (ci == shownInCount && co == shownOutCount)
        return;                                           // idle: nothing new
    shownInCount = ci;
    shownOutCount = co;
    analyse (in, inDb, dt);
    analyse (out, outDb, dt);
    repaint();
}

void SpectrumView::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (10.0f, 8.0f);
    const double sr = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
    const float fLo = 20.0f, fHi = 20000.0f;
    const float dbLo = -90.0f, dbHi = 0.0f;

    auto traceOf = [&] (const std::vector<float>& bins, juce::Path& path)
    {
        bool started = false;
        const int cols = juce::jmax (1, (int) area.getWidth());
        for (int cx = 0; cx <= cols; ++cx)
        {
            const float t = (float) cx / (float) cols;
            const float f = fLo * std::pow (fHi / fLo, t);
            const int bin = juce::jlimit (1, kBins - 1,
                                          (int) std::round (f * (double) kSize / sr));
            const float db = juce::jlimit (dbLo, dbHi, bins[(size_t) bin]);
            const float x = area.getX() + t * area.getWidth();
            const float y = area.getBottom()
                          - (db - dbLo) / (dbHi - dbLo) * area.getHeight();
            if (! started) { path.startNewSubPath (x, y); started = true; }
            else           path.lineTo (x, y);
        }
    };

    juce::Path pin, pout;
    traceOf (inDb, pin);
    traceOf (outDb, pout);
    g.setColour (colours::textDim.withAlpha (0.55f));
    g.strokePath (pin, juce::PathStrokeType (1.0f));
    g.setColour (colours::accent);
    g.strokePath (pout, juce::PathStrokeType (1.3f));

    // Dismiss × (top-right).
    g.setColour (colours::textDim.withAlpha (0.7f));
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawText (juce::String::charToString ((juce::juce_wchar) 0x00D7),
                getWidth() - 24, 4, 18, 18, juce::Justification::centred);
}
