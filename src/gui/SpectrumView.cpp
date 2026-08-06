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
    // Fires over the corner chip only (`hitTest` narrows the pointer claim),
    // so the hint names the chip's ACTION, not this view. R2 item-11 wording ⊕.
    setTooltip ("Switch to the gain-reduction history");
}

void SpectrumView::visibilityChanged()
{
    if (isVisible())
        clock.start (*this, [this] (double dt) { tick (dt); });
    else
        clock.stop();
}

// The mode chip's hit-area — the top-right corner affordance that flips the
// graph well to the GR history. (It was the spectrum's dismiss × until
// 2026-08-05; same corner, same rectangle, different meaning — the well now
// always shows one of the two views, so there is nothing to dismiss TO.) ONE
// definition, because `hitTest` and `mouseDown` both key on it and a click this
// view accepts but then ignores is exactly the swallowing the hitTest below
// removes — reintroduced one pixel at a time if the two rectangles were
// computed separately.
//
// Deliberately LARGER than the drawn chip (`paint` puts "GR" at
// `getWidth() - 26, 4, 22 × 16`): the surplus is the touch target.
juce::Rectangle<int> SpectrumView::chipHitArea() const noexcept
{
    return { getWidth() - 27, 0, 27, 24 };
}

// ONLY the chip is interactive. Leaving JUCE's default (hit-test true
// everywhere) made this view consume every click in the metering strip and
// do nothing with it — the one region of the editor that took a click with no
// affordance and no effect. `CurveView` opts out wholesale with
// `setInterceptsMouseClicks (false, false)`; this view cannot, because it owns
// the mode chip, so it opts out per-pixel instead, which is what `hitTest` is
// for — and `GrHistoryView` now does exactly the same for its mirrored "SPEC"
// chip. `LoudnessMeterView` stays intercepting because its WHOLE surface is the
// affordance (click = meter reset).
//
// TWO CONSEQUENCES, both inseparable from the fix rather than additions to it —
// `hitTest` is what decides membership of JUCE's "under the mouse" set, so
// declining a region declines EVERYTHING about it, not just the click.
//
//   1. THE TOOLTIP NARROWS to the chip. That is now the RIGHT scope rather
//      than a cost: the tooltip names the chip's action ("Switch to the
//      gain-reduction history", set in the constructor), so it belongs over the
//      chip and nowhere else. It was written the other way round when the
//      string was the identifier `"Spectrum"` and the wording was still an
//      open C8 owner TODO — the R2 item-11 directive discharged that TODO and
//      supplied the action wording, so there is no longer a brand-pass question
//      about widening it back. Widening would mean intercepting everywhere
//      again, i.e. re-accepting consequence 2 below, for a hint that would then
//      be wrong over the trace.
//
//   2. CLICKS OVER THE TRACE REACH WHATEVER IS BENEATH — today the editor
//      itself, which is the correct outcome and the point of the change, but it
//      is a live routing decision rather than a void. The editor installs no
//      tooltip on its background and no click handler under the graph well, so
//      nothing happens there now. The thing to know before ADDING one: anything
//      placed under this view's footprint becomes reachable through the trace
//      while this mode is showing. If a future affordance lands there and must
//      NOT be clickable through the trace, the answer is to widen this
//      hit-area — not to revert to intercepting everywhere, which would restore
//      the swallow this removed.
bool SpectrumView::hitTest (int x, int y)
{
    return chipHitArea().contains (x, y);
}

void SpectrumView::mouseDown (const juce::MouseEvent& e)
{
    // The test is unreachable-false — `hitTest` already refused every click
    // outside the area — and is kept rather than trimmed so this function is
    // correct standing alone instead of correct because of what another
    // function happens to return.
    if (chipHitArea().contains (e.getPosition()))
        // Since 2026-08-05 this is a MODE SWITCH, not a dismissal: the well
        // flips to the GR history (the chip names what you switch TO), and the
        // GR view carries the mirrored chip back. Same corner, same hit-area
        // discipline (clicks over the trace still pass through).
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

    // Column reads: the two-regime rule adapted from Anamorph's SpectrumImager
    // (ADR-0009 provenance: Anamorph src/gui/SpectrumImager.cpp, `magCubic` /
    // `magForColumn`). On a log-f axis the low end packs many pixel columns
    // into few FFT bins, so the nearest-bin read this replaces quantised the
    // LF trace into a staircase. Instead, each column spans [cx−½, cx+½]:
    // where that covers fewer than 1.5 bins, the value is a Catmull-Rom
    // interpolation across the four surrounding bins; where it covers 1.5 bins
    // or more (the HF end, many bins per column), it averages every covered
    // bin. Adapted to this analyser's state: Anamorph interpolates LINEAR
    // magnitudes before its dB conversion, but here the smoothed arrays
    // already hold dB (the EMA runs in dB — see `analyse`), so both regimes
    // read dB directly. The clamp floor stays at bin 1 — the nearest-bin read
    // never showed DC and this port keeps that exclusion.
    const float binHz = (float) (sr / (double) kSize);
    auto binAt = [&] (const std::vector<float>& bins, int j)
    {
        return bins[(size_t) juce::jlimit (1, kBins - 1, j)];
    };
    auto dbCubic = [&] (const std::vector<float>& bins, float binPos)
    {
        const int   i = (int) std::floor (binPos);
        const float u = binPos - (float) i;
        const float m0 = binAt (bins, i - 1), m1 = binAt (bins, i);
        const float m2 = binAt (bins, i + 1), m3 = binAt (bins, i + 2);
        return 0.5f * ((2.0f * m1) + (-m0 + m2) * u
                       + (2.0f * m0 - 5.0f * m1 + 4.0f * m2 - m3) * u * u
                       + (-m0 + 3.0f * m1 - 3.0f * m2 + m3) * u * u * u);
    };
    auto dbForColumn = [&] (const std::vector<float>& bins, float fa, float fb)
    {
        const float span = (fb - fa) / binHz;
        if (span < 1.5f)
            return dbCubic (bins, 0.5f * (fa + fb) / binHz);
        const int ka = juce::jlimit (1, kBins - 1, (int) std::floor (fa / binHz));
        const int kb = juce::jlimit (1, kBins - 1, (int) std::ceil  (fb / binHz));
        float sum = 0.0f;
        for (int k = ka; k <= kb; ++k)
            sum += bins[(size_t) k];
        return sum / (float) (kb - ka + 1);
    };

    auto traceOf = [&] (const std::vector<float>& bins, juce::Path& path)
    {
        bool started = false;
        const int cols = juce::jmax (1, (int) area.getWidth());
        auto freqAt = [&] (float cx)
        {
            return fLo * std::pow (fHi / fLo, cx / (float) cols);
        };
        for (int cx = 0; cx <= cols; ++cx)
        {
            const float t  = (float) cx / (float) cols;
            const float fa = freqAt ((float) cx - 0.5f);
            const float fb = freqAt ((float) cx + 0.5f);
            const float db = juce::jlimit (dbLo, dbHi, dbForColumn (bins, fa, fb));
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

    // Corner mode chip (top-right): names the view you switch TO.
    g.setColour (colours::textDim.withAlpha (0.7f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)).withExtraKerningFactor (0.1f));
    g.drawText ("GR", getWidth() - 26, 4, 22, 16, juce::Justification::centred);
}
