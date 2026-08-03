#include "LoudnessMeterView.h"
#include "../PluginProcessor.h"

using namespace abgui;

juce::String LoudnessMeterView::tooltipText()
{
    juce::String targets;
    for (int t = 0; t < kNumTargets; ++t)
        targets << (t == 0 ? "" : ", ")
                << kTargets[t].fullName << " " << juce::String (kTargets[t].lufs, 0);
    return "Targets: " + targets + " LUFS (as of " + kTargetsAsOf
         + "). Click to reset integrated / TP hold.";
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
    const int  mask = (int) processor.internalState.state()
                          .getProperty (iid::meterTargets, ~0);
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
                      || mask != shownMask || tpOn != shownTpOn;
    if (! changed)
        return;
    shownM = m; shownS = s; shownI = i; shownTp = tp; shownPlr = plr;
    shownCeiling = ceil;
    shownMask = mask; shownTpOn = tpOn;
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

    // M / S / I rows: tag, numeric, bar (−36..0 LUFS) with target ticks.
    const float lo = -36.0f, hi = 0.0f;
    auto toX = [lo, hi] (juce::Rectangle<int> bar, float lufs)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (lufs - lo) / (hi - lo));
        return (float) bar.getX() + t * (float) bar.getWidth();
    };

    const char* tags[] = { "M", "S", "I" };
    const float vals[] = { shownM, shownS, shownI };
    // One rectangle per row, kept for the target ticks below: they used to
    // RE-DERIVE the row origin from `getLocalBounds()` (`… + 18 + r * 26 + 6`),
    // a second copy of arithmetic this loop already performs. It happened to
    // agree — the tick spans `bar.getY() - 2` to `bar.getBottom() + 2`, the
    // deliberate 2 px overhang on an 8 px bar — but a change to the header
    // height, the 24 px row or the 2 px gap would have moved one copy and not
    // the other. There is now one source for both.
    juce::Rectangle<int> bars[3];
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
        bars[r] = bar;
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

    // Target lines (bitmask-gated) drawn as ticks over the bar column, with
    // the §6.2 tags underneath.
    auto tagRow = area.removeFromTop (14);
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    for (int t = 0; t < kNumTargets; ++t)
    {
        if ((shownMask & (1 << t)) == 0)
            continue;
        const float x = toX (bars[2], kTargets[t].lufs);   // all three share a width
        g.setColour (colours::textDim.withAlpha (0.8f));
        for (int r = 0; r < 3; ++r)
            g.drawLine (x, (float) bars[r].getY() - 2.0f,
                        x, (float) bars[r].getBottom() + 2.0f, 1.0f);
        g.drawText (kTargets[t].shortName,
                    juce::Rectangle<int> ((int) x - 10, tagRow.getY(), 20, 12),
                    juce::Justification::centred);
    }
    area.removeFromTop (4);

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
        // dBTP, and a factory preset already ships a moved ceiling (EDM Club
        // sets −0.5), so a non-default ceiling is the ordinary case.
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

    // Penalty rows: platformTarget − integrated, shown only while the
    // integrated figure exists (DESIGN §2.9 — pure display arithmetic).
    if (shownI > silent)
    {
        g.setFont (juce::Font (juce::FontOptions (11.0f)).withExtraKerningFactor (0.22f));
        g.setColour (colours::textDim);
        g.drawText ("PENALTY", area.removeFromTop (14), juce::Justification::centredLeft);
        for (int t = 0; t < kNumTargets; ++t)
        {
            if ((shownMask & (1 << t)) == 0)
                continue;
            auto row = area.removeFromTop (17);
            const float penalty = kTargets[t].lufs - shownI;
            g.setColour (colours::textDim);
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawText (kTargets[t].shortName, row.removeFromLeft (26),
                        juce::Justification::centredLeft);
            g.setColour (penalty < 0.0f ? colours::warn : colours::text);
            g.setFont (juce::Font (juce::FontOptions (12.0f)));
            g.drawText ((penalty > 0.0f ? "+" : "") + juce::String (penalty, 1) + " dB",
                        row, juce::Justification::centredLeft);
        }
    }
}
