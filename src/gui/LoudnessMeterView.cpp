#include "LoudnessMeterView.h"
#include "../PluginProcessor.h"

using namespace abgui;

juce::String LoudnessMeterView::tooltipText()
{
    return "Output loudness, BS.1770-4. Click to reset the integrated measurement and the TP hold.";
}

LoudnessMeterView::LoudnessMeterView (AnabasisAudioProcessor& p) : processor (p)
{
    setInterceptsMouseClicks (true, false);
    setTooltip (tooltipText());
}

void LoudnessMeterView::visibilityChanged()
{
    if (isVisible())
        clock.start (*this, [this] (double dt) { tick (dt); });
    else
        clock.stop();
}

void LoudnessMeterView::mouseDown (const juce::MouseEvent&)
{
    processor.requestMeterReset();   // §2.9 momentary-request row
}

void LoudnessMeterView::tick (double)
{
    const float m   = processor.meterLufsM();
    const float s   = processor.meterLufsS();
    const float i   = processor.meterLufsI();
    const float tp  = processor.meterDbTpMax();
    const float plr = processor.meterPlr();
    const bool tpOn = (bool) processor.internalState.state()
                          .getProperty (iid::tpMeterOn, true);
    const float ceil = processor.apvts.getRawParameterValue (pid::ceiling)->load();

    // Bitwise compares, so even a NaN transition still repaints once.
    const bool changed = std::memcmp (&m, &shownM, 4) != 0
                      || std::memcmp (&s, &shownS, 4) != 0
                      || std::memcmp (&i, &shownI, 4) != 0
                      || std::memcmp (&tp, &shownTp, 4) != 0
                      || std::memcmp (&plr, &shownPlr, 4) != 0
                      || std::memcmp (&ceil, &shownCeiling, 4) != 0
                      || tpOn != shownTpOn;
    if (! changed)
        return;
    shownM = m; shownS = s; shownI = i; shownTp = tp; shownPlr = plr;
    shownCeiling = ceil;
    shownTpOn = tpOn;
    repaint();
}

void LoudnessMeterView::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().reduced (12, 10);
    const auto silent = -99.0f;
    auto fmt = [silent] (float v, int dp = 1)
    { return v <= silent ? juce::String ("-") : juce::String (v, dp); };

    // Header
    g.setColour (colours::textDim);
    g.setFont (juce::Font (juce::FontOptions (11.0f)).withExtraKerningFactor (0.22f));
    g.drawText ("LUFS", area.removeFromTop (16), juce::Justification::centredLeft);
    area.removeFromTop (2);

    // M / S / I rows: tag, numeric, bar (−36..0 LUFS).
    const float lo = -36.0f, hi = 0.0f;
    auto toX = [lo, hi] (juce::Rectangle<int> bar, float lufs)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (lufs - lo) / (hi - lo));
        return (float) bar.getX() + t * (float) bar.getWidth();
    };

    const char* tags[] = { "M", "S", "I" };
    const float vals[] = { shownM, shownS, shownI };
    for (int r = 0; r < 3; ++r)
    {
        auto row = area.removeFromTop (24);
        g.setColour (colours::textDim);
        g.setFont (juce::Font (juce::FontOptions (11.5f)));
        g.drawText (tags[r], row.removeFromLeft (16), juce::Justification::centredLeft);
        g.setColour (colours::text);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawText (fmt (vals[r]), row.removeFromLeft (48), juce::Justification::centredRight);
        row.removeFromLeft (8);
        auto bar = row.reduced (0, 8);
        g.setColour (colours::bgRaised);
        g.fillRoundedRectangle (bar.toFloat(), 3.0f);
        if (vals[r] > silent)
        {
            auto fill = bar.toFloat().withRight (toX (bar, vals[r]));
            juce::ColourGradient gr (colours::accent2, fill.getX(), 0,
                                     colours::accent, fill.getRight(), 0, false);
            g.setGradientFill (gr);
            g.fillRoundedRectangle (fill, 3.0f);
        }
        area.removeFromTop (2);
    }

    area.removeFromTop (6);

    // TP / PLR rows.
    if (shownTpOn)
    {
        auto row = area.removeFromTop (20);
        g.setColour (colours::textDim);
        g.setFont (juce::Font (juce::FontOptions (11.5f)));
        g.drawText ("TP", row.removeFromLeft (30), juce::Justification::centredLeft);
        // Over-ceiling reads in `warn` — the §6.1 colour-blind-safe pairing
        // (gold accent vs desaturated red survives as a luminance difference).
        // Against the USER's ceiling, not a literal: the threshold was
        // hard-coded to −1.0, which is merely the ceiling's DEFAULT, so any
        // other setting warned at the wrong level — silent while genuinely
        // over at a −6 ceiling, red while legal at a raised one. Both are in
        // dBTP; the ceiling is the user's to move, so a non-default value
        // is the ordinary case.
        g.setColour (shownTp > shownCeiling && shownTp > -99.0f ? colours::warn : colours::text);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawText (fmt (shownTp, 2) + " dBTP", row, juce::Justification::centredLeft);
    }
    {
        auto row = area.removeFromTop (20);
        g.setColour (colours::textDim);
        g.setFont (juce::Font (juce::FontOptions (11.5f)));
        g.drawText ("PLR", row.removeFromLeft (30), juce::Justification::centredLeft);
        g.setColour (colours::text);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawText (shownI <= silent ? juce::String ("-") : juce::String (shownPlr, 1),
                    row, juce::Justification::centredLeft);
    }
    area.removeFromTop (6);

}
