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
        // Unity reference diagonal, dim.
        g.setColour (colours::outline);
        g.drawLine (area.getX(), area.getBottom(), area.getRight(), area.getY(), 1.0f);
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
        // 0 dB reference, dim.
        g.setColour (colours::outline);
        g.drawLine (area.getX(), area.getCentreY(), area.getRight(), area.getCentreY(), 1.0f);
    }

    g.setColour (colours::accent);
    g.strokePath (path, juce::PathStrokeType (1.4f));
}
