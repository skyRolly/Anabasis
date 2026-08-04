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
    // THE RESET IS ANNOUNCED, NOT INFERRED. `AnabasisEngine::prepare` rewinds
    // both rings so frames captured at the previous sample rate become
    // unreachable; making them unreachable is only half of it, because
    // `analyse` returns immediately when `readLatest` yields nothing — exactly
    // the post-rewind state — so `inDb`/`outDb` would keep the PREVIOUS
    // lifecycle's EMA and go on being drawn, the old analysis rendered against
    // the new rate's bin mapping, which is the artefact the rewind exists to
    // remove. The reader owns its smoothed copy, so the reader must drop it;
    // the ring cannot do it from the other side.
    //
    // This used to key on `writeCount()` going BACKWARDS, and that predicate is
    // weaker than the guarantee the comment claimed. It holds only while the
    // observed count is still below the one the last tick stored: let the
    // producer republish past that value between two ticks — one tick delayed
    // past ~1 s of audio, a suspended message thread, a debugger stop — and the
    // rewind is missed OUTRIGHT, permanently, with no later tick able to notice,
    // because every subsequent count is larger again. The failure is silent and
    // its symptom is the exact artefact this code exists to prevent. Ordering
    // two counters cannot express "a reset happened"; a generation can, and
    // `GrHistoryBuffer` already answered the same question with an epoch rather
    // than a counter comparison. The rings now carry one too.
    //
    // Sampled on BOTH sides of the analysis batch, which is `resetEpoch()`'s
    // documented reader contract and closes the remaining skew: the pre-batch
    // sample catches a reset that landed since the last tick, the post-batch one
    // catches a reset that landed DURING this tick's reads, whose frames may
    // straddle two configurations. Either way the answer is the same — drop to
    // the floor and re-anchor — so a straddling batch costs no display frame
    // rather than one.
    const auto gi0 = in.resetGeneration();
    const auto go0 = out.resetGeneration();
    const auto ci  = in.writeCount();
    const auto co  = out.writeCount();

    // The generations join the idle test, or a reset landing on a tick with no
    // new frames would early-return past the clear below.
    if (ci == shownInCount && co == shownOutCount
        && gi0 == shownInGen && go0 == shownOutGen)
        return;                                           // idle: nothing new

    const bool resetIn  = gi0 != shownInGen;
    const bool resetOut = go0 != shownOutGen;

    // Only on the reset EDGE, deliberately. This is not the "should an idle
    // analyser decay to the floor?" question — that is the early return above,
    // it is a listening-pass call, and it stays exactly as it was
    // (`KNOWN_ISSUES` KI-007 item 6).
    if (resetIn)  std::fill (inDb.begin(),  inDb.end(),  -120.0f);
    if (resetOut) std::fill (outDb.begin(), outDb.end(), -120.0f);

    analyse (in, inDb, dt);
    analyse (out, outDb, dt);

    // The second sample. A generation that moved while the batch ran means the
    // frames just folded into the EMA may span the rewind, so the EMA is not a
    // description of either configuration — the post-reset state is the floor,
    // which is what a reset leaves in any case.
    const auto gi1 = in.resetGeneration();
    const auto go1 = out.resetGeneration();
    if (gi1 != gi0) std::fill (inDb.begin(),  inDb.end(),  -120.0f);
    if (go1 != go0) std::fill (outDb.begin(), outDb.end(), -120.0f);

    shownInGen   = gi1;
    shownOutGen  = go1;
    shownInCount = ci;
    shownOutCount = co;
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
