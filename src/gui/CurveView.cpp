#include "CurveView.h"
#include "../PluginProcessor.h"

using namespace abgui;

CurveView::CurveView (AnabasisAudioProcessor& p, Mode m) : processor (p), mode (m)
{
    setInterceptsMouseClicks (false, false);
    scratchEq.prepare (48000.0);   // display-only; re-prepared per refresh below
}

// Reads the inputs this mode's curve is built from and fingerprints them IN
// THE SAME PASS. That coupling is the point: a value cannot enter the drawn
// curve without entering the hash, and the hash cannot describe a value the
// caller did not receive, so no separate "these are the ids the fingerprint
// covers" list exists to fall out of step with the build below.
//
// The SAMPLE RATE is part of the fingerprint because paint() prepares its
// scratch EQ at this rate, and the RBJ coefficients — so the drawn response,
// most visibly for the shelves and bells near Nyquist — depend on it. Hashing
// only the parameters meant a host rate change with no knob movement left the
// well showing the old rate's curve until some unrelated repaint. Rounded to
// an int: the rate is integral in practice, and hashing a double's bits would
// repaint on a value that cannot change the curve. The unprepared 0 is folded
// to the same 48 kHz fallback the build uses, so the pre-prepare picture and a
// subsequent prepare at 48 kHz hash alike — they ARE the same curve.
CurveView::Inputs CurveView::readInputs() const
{
    Inputs in;
    // KI-017: the published pair, not `getSampleRate()`'s plain member. This
    // one is reached from BOTH threads — `paint` and the editor's 24 Hz
    // `timerCallback` through `refresh()` — so on Linux, where no GL context
    // attaches, it was still a race with the host's reconfiguring thread.
    // Read ONCE, for the reason the accessor states.
    const double prepared = processor.preparedSampleRate();
    in.sampleRate = prepared > 0.0 ? prepared : 48000.0;
    in.fingerprint = (juce::uint64) juce::roundToInt (in.sampleRate);

    auto& apvts = processor.apvts;
    auto take = [&apvts, &in] (const char* id) -> float
    {
        const float v = apvts.getRawParameterValue (id)->load();
        juce::uint32 u;
        std::memcpy (&u, &v, 4);
        in.fingerprint = in.fingerprint * 1099511628211ull + u;
        return v;
    };

    if (mode == Mode::clipTransfer)
    {
        in.clipShape = take (pid::clipShape);
        in.clipDrive = take (pid::clipDrive);
    }
    else
    {
        in.eq.eqTiltDb          = take (pid::eqTilt);
        in.eq.eqLowShelfFreqHz  = take (pid::eqLowShelfFreq);
        in.eq.eqLowShelfGainDb  = take (pid::eqLowShelfGain);
        in.eq.eqHighShelfFreqHz = take (pid::eqHighShelfFreq);
        in.eq.eqHighShelfGainDb = take (pid::eqHighShelfGain);
        in.eq.eqBell1FreqHz     = take (pid::eqBell1Freq);
        in.eq.eqBell1GainDb     = take (pid::eqBell1Gain);
        in.eq.eqBell1Q          = take (pid::eqBell1Q);
        in.eq.eqBell2FreqHz     = take (pid::eqBell2Freq);
        in.eq.eqBell2GainDb     = take (pid::eqBell2Gain);
        in.eq.eqBell2Q          = take (pid::eqBell2Q);
    }
    return in;
}

// Recompute + repaint only when an input moved (the editor timer calls this at
// 24 Hz). The values themselves are discarded here — this is an edge detector,
// and paint() takes its own read.
void CurveView::refresh()
{
    const auto fp = readInputs().fingerprint;
    if (fp != shownFingerprint)
    {
        shownFingerprint = fp;
        repaint();
    }
}

void CurveView::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (6.0f, 4.0f);

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
    // nothing about the DSP changed. No visual change from the caching itself:
    // the rebuild produces the same path it always did.
    //
    // THE LABEL IS THE READ, which is what makes the sentence above true rather
    // than nearly true. The key used to be `shownFingerprint` — a member only
    // `refresh()` advances, and the editor ticks `refresh()` only while Advanced
    // is showing. A repaint no `refresh()` preceded (a host expose, a
    // `resized()` between ticks) therefore built the path from the CURRENT
    // parameter values and stamped it with the fingerprint of the values
    // `refresh()` last saw: a cache entry labelled with a state it was not built
    // from. It self-corrected on the next tick, so no stale curve survived long,
    // but "the label describes the geometry" was not an invariant — it was an
    // outcome that happened to hold at 24 Hz, and a caching rule that is only
    // eventually true is the kind that breaks when the tick that repairs it
    // stops running.
    //
    // Now paint() takes its own `readInputs()` and both COMPARES and STAMPS with
    // that, then builds the path from the very values it hashed. The label is
    // exact by construction, at every entry to paint(), with or without a
    // preceding refresh. `shownFingerprint` keeps only the job it can actually
    // do: telling `refresh()` when to ask for a repaint.
    const auto bounds = getLocalBounds();
    const auto in = readInputs();
    if (pathFingerprint == in.fingerprint && pathBounds == bounds && ! cachedPath.isEmpty())
    {
        paintStatic (g, area);
        g.setColour (colours::accent);
        g.strokePath (cachedPath, juce::PathStrokeType (1.4f));
        return;
    }
    pathFingerprint = in.fingerprint;
    pathBounds      = bounds;

    juce::Path path;

    if (mode == Mode::clipTransfer)
    {
        // The audio path's exact arithmetic: y = f(x·g, w) / g (level-
        // compensated drive), through ClipSat::transfer itself.
        const float w = in.clipShape;
        const float gLin = std::pow (10.0f, in.clipDrive * (1.0f / 20.0f));
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
        scratchEq.prepare (in.sampleRate);   // reset → unprimed → setTargets snaps
        scratchEq.setTargets (in.eq);

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
