#include "CurveView.h"
#include "../PluginProcessor.h"

using namespace abgui;

CurveView::CurveView (AnabasisAudioProcessor& p, Mode m) : processor (p), mode (m)
{
    setInterceptsMouseClicks (false, false);
    scratchEq.prepare (48000.0);   // display-only; re-prepared per refresh below
}

// A cheap fingerprint of the parameters this curve depends on: recompute only
// when one moved (the editor timer calls this at 24 Hz).
void CurveView::refresh()
{
    auto& apvts = processor.apvts;
    auto rawBits = [&apvts] (const char* id) -> juce::uint64
    {
        const float v = apvts.getRawParameterValue (id)->load();
        juce::uint32 u;
        std::memcpy (&u, &v, 4);
        return u;
    };

    // The SAMPLE RATE is part of the fingerprint because paint() re-prepares
    // its scratch EQ at `processor.getSampleRate()`, and the RBJ coefficients —
    // so the drawn response, most visibly for the shelves and bells near
    // Nyquist — depend on it. Hashing only the parameters meant a host rate
    // change with no knob movement left the well showing the old rate's curve
    // until some unrelated repaint. Rounded to an int: the rate is integral in
    // practice, and hashing a double's bits would repaint on a value that
    // cannot change the curve.
    juce::uint64 fp = (juce::uint64) juce::roundToInt (processor.getSampleRate());
    if (mode == Mode::clipTransfer)
        for (const char* id : { pid::clipShape, pid::clipDrive })
            fp = fp * 1099511628211ull + rawBits (id);
    else
        for (const char* id : { pid::eqTilt, pid::eqLowShelfFreq, pid::eqLowShelfGain,
                                pid::eqHighShelfFreq, pid::eqHighShelfGain,
                                pid::eqBell1Freq, pid::eqBell1Gain, pid::eqBell1Q,
                                pid::eqBell2Freq, pid::eqBell2Gain, pid::eqBell2Q })
            fp = fp * 1099511628211ull + rawBits (id);

    if (fp != shownFingerprint)
    {
        shownFingerprint = fp;
        repaint();
    }
}

void CurveView::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (6.0f, 4.0f);
    auto& apvts = processor.apvts;
    auto raw = [&apvts] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    // The curve is REBUILT only when one of its two inputs moved, and painted
    // every time. `refresh()` already computes the fingerprint that decides
    // whether the parameters or the sample rate changed and repaints on it — but
    // the build ran unconditionally in here, so any repaint the fingerprint did
    // NOT ask for paid for it anyway: a host-driven expose, an overlay being
    // dismissed, a window move, a `resized()`. For the EQ well that is a full
    // `prepare()` (reset plus five filters' worth of RBJ coefficients) followed
    // by one `magnitudeDbAt` per pixel column — transcendental-heavy work on the
    // message thread, for a picture identical to the one already on screen.
    //
    // The bounds join the fingerprint because they are the other input: the
    // path is in component coordinates, so a resize must rebuild it even when
    // nothing about the DSP changed. Caching against the SAME fingerprint
    // `refresh()` gates the repaint with is what makes the two consistent —
    // there is no state in which a repaint is requested and the cache is not
    // rebuilt, which is what would have turned this optimisation into a stale
    // curve. No visual change: the rebuild produces the same path it always did.
    //
    // BE PRECISE ABOUT WHAT THAT GUARANTEES, because the sentence above is
    // stronger than the code. `shownFingerprint` is advanced only by
    // `refresh()`, which the editor ticks only while Advanced is showing — so a
    // repaint can arrive that no `refresh()` preceded (a host expose, a
    // `resized()` between ticks). The path built then reads the CURRENT
    // parameter values while being stamped with the fingerprint `refresh()` last
    // computed, so the cache can be LABELLED with a fingerprint it was not built
    // from. It is self-correcting — the next `refresh()` sees a different
    // fingerprint and rebuilds — so no stale curve survives a tick, and in
    // Simple mode the views are hidden and the mode switch changes the bounds,
    // which invalidates on the other half of the key. What actually holds is
    // therefore: "no stale curve persists beyond one refresh", not "every
    // repaint rebuilds". Recorded rather than fixed: the cache strategy is a
    // deferred item, and this is a comment that overstated it.
    const auto bounds = getLocalBounds();
    if (pathFingerprint == shownFingerprint && pathBounds == bounds && ! cachedPath.isEmpty())
    {
        paintStatic (g, area);
        g.setColour (colours::accent);
        g.strokePath (cachedPath, juce::PathStrokeType (1.4f));
        return;
    }
    pathFingerprint = shownFingerprint;
    pathBounds      = bounds;

    juce::Path path;

    if (mode == Mode::clipTransfer)
    {
        // The audio path's exact arithmetic: y = f(x·g, w) / g (level-
        // compensated drive), through ClipSat::transfer itself.
        const float w = raw (pid::clipShape);
        const float driveDb = raw (pid::clipDrive);
        const float gLin = std::pow (10.0f, driveDb * (1.0f / 20.0f));
        const int cols = juce::jmax (2, (int) area.getWidth());
        for (int cx = 0; cx <= cols; ++cx)
        {
            const float x = (float) cx / (float) cols;             // 0..1 input
            const float y = anabasis::ClipSat::transfer (x * gLin, w) / gLin;
            const float px = area.getX() + x * area.getWidth();
            const float py = area.getBottom()
                           - juce::jlimit (0.0f, 1.0f, y) * area.getHeight();
            if (cx == 0) path.startNewSubPath (px, py);
            else         path.lineTo (px, py);
        }
    }
    else
    {
        // The audio recompute itself, on the scratch instance: reset() drops
        // `primed`, so setTargets SNAPS and recomputes — the display always
        // shows the coefficients the audio side would land on.
        anabasis::EngineParameters snap;
        snap.eqTiltDb          = raw (pid::eqTilt);
        snap.eqLowShelfFreqHz  = raw (pid::eqLowShelfFreq);
        snap.eqLowShelfGainDb  = raw (pid::eqLowShelfGain);
        snap.eqHighShelfFreqHz = raw (pid::eqHighShelfFreq);
        snap.eqHighShelfGainDb = raw (pid::eqHighShelfGain);
        snap.eqBell1FreqHz     = raw (pid::eqBell1Freq);
        snap.eqBell1GainDb     = raw (pid::eqBell1Gain);
        snap.eqBell1Q          = raw (pid::eqBell1Q);
        snap.eqBell2FreqHz     = raw (pid::eqBell2Freq);
        snap.eqBell2GainDb     = raw (pid::eqBell2Gain);
        snap.eqBell2Q          = raw (pid::eqBell2Q);

        const double sr = processor.getSampleRate() > 0.0 ? processor.getSampleRate()
                                                          : 48000.0;
        scratchEq.prepare (sr);        // reset → unprimed → setTargets snaps
        scratchEq.setTargets (snap);

        const float dbSpan = 15.0f;    // ±15 dB display window
        const float fLo = 20.0f, fHi = 20000.0f;
        const int cols = juce::jmax (2, (int) area.getWidth());
        for (int cx = 0; cx <= cols; ++cx)
        {
            const float t = (float) cx / (float) cols;
            const float f = fLo * std::pow (fHi / fLo, t);
            const float db = juce::jlimit (-dbSpan, dbSpan, scratchEq.magnitudeDbAt (f));
            const float px = area.getX() + t * area.getWidth();
            const float py = area.getCentreY() - db / dbSpan * area.getHeight() * 0.5f;
            if (cx == 0) path.startNewSubPath (px, py);
            else         path.lineTo (px, py);
        }
    }

    cachedPath = path;
    paintStatic (g, area);
    g.setColour (colours::accent);
    g.strokePath (path, juce::PathStrokeType (1.4f));
}

// The mode's reference line — the clip well's unity diagonal, the EQ well's
// 0 dB horizontal. Split out because it is the one part of the picture that is
// NOT in the cached path: both are pure functions of the bounds, they cost two
// `drawLine`s, and leaving them inside the rebuilt branch would have made the
// cached frame silently different from the uncached one — which is exactly the
// class of defect a paint cache introduces if the split is drawn in the wrong
// place.
void CurveView::paintStatic (juce::Graphics& g, juce::Rectangle<float> area) const
{
    g.setColour (colours::outline);
    if (mode == Mode::clipTransfer)
        g.drawLine (area.getX(), area.getBottom(), area.getRight(), area.getY(), 1.0f);
    else
        g.drawLine (area.getX(), area.getCentreY(), area.getRight(), area.getCentreY(), 1.0f);
}
