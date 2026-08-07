#include "LoudnessMeterView.h"
#include "../PluginProcessor.h"

using namespace abgui;

juce::String LoudnessMeterView::tooltipText()
{
    return "Waveform statistics off the output. Click to reset the integrated "
           "measurement, the loudness range and both peak holds.";
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
    const auto& ist = processor.internalState.state();
    // The §4.4 read fallback is the SHIPPED DEFAULT for both, never `var()`'s
    // 0-by-accident: they happen to agree here (0 is the default of each), and
    // the constants are still written out because "correct because the default
    // is zero" stops being true the first time a default moves.
    const bool ungated = (int) ist.getProperty (iid::integratedStd, 0) == 1;
    const bool aes17   = (int) ist.getProperty (iid::rmsRef, 0) == 0;

    const float m   = processor.meterLufsM();
    const float s   = processor.meterLufsS();
    const float i   = ungated ? processor.meterLufsIUngated() : processor.meterLufsI();
    const float tp  = processor.meterDbTpMax();
    const float plr = processor.meterPlr();
    const float pk  = processor.meterPeakMaxDb();
    const float lra = processor.meterLra();
    // AES-17 references a full-scale SINE to 0 dBFS, which is +3.01 dB on the
    // mathematical RMS the meter publishes (a sine's RMS is 1/√2 of its peak).
    // The sentinel passes through UNTOUCHED — offsetting it would print a
    // nonsense −140.99 where the row means "nothing measured yet" — and the
    // test is on the RMS reading alone. It briefly also consulted the sample
    // peak, which is incoherent whatever the values do: whether this row has
    // a reading is a fact about this row.
    const float rawRms = processor.meterRmsDb();
    const float rms = (aes17 && rawRms > anabasis::RmsMeter::kSilentDb + 1.0f)
                        ? rawRms + 3.0103f
                        : rawRms;
    const float ceil = processor.apvts.getRawParameterValue (pid::ceiling)->load();

    // Bitwise compares, so even a NaN transition still repaints once.
    const bool changed = std::memcmp (&m, &shownM, 4) != 0
                      || std::memcmp (&s, &shownS, 4) != 0
                      || std::memcmp (&i, &shownI, 4) != 0
                      || std::memcmp (&tp, &shownTp, 4) != 0
                      || std::memcmp (&plr, &shownPlr, 4) != 0
                      || std::memcmp (&ceil, &shownCeiling, 4) != 0
                      || std::memcmp (&pk, &shownPeak, 4) != 0
                      || std::memcmp (&rms, &shownRms, 4) != 0
                      || std::memcmp (&lra, &shownLra, 4) != 0;
    if (! changed)
        return;
    shownM = m; shownS = s; shownI = i; shownTp = tp; shownPlr = plr;
    shownCeiling = ceil;
    shownPeak = pk; shownRms = rms; shownLra = lra;
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
    g.drawText ("STATISTICS", area.removeFromTop (16), juce::Justification::centredLeft);
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

    // The numeric statistics rows. ONE writer for all five (ADR-0020): the
    // rows differ only in tag, text and warn colour, and five hand-rolled
    // copies is how the TP row ended up with a hard-coded threshold nothing
    // else shared.
    auto statRow = [&] (const char* tag, const juce::String& text, bool warn)
    {
        auto row = area.removeFromTop (20);
        g.setColour (colours::textDim);
        g.setFont (juce::Font (juce::FontOptions (11.5f)));
        g.drawText (tag, row.removeFromLeft (34), juce::Justification::centredLeft);
        g.setColour (warn ? colours::warn : colours::text);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawText (text, row, juce::Justification::centredLeft);
    };

    // TRUE PEAK. Shown unconditionally since ADR-0020 — the Settings toggle
    // that used to hide it (`int_tpMeterOn`) is gone with the same record: a
    // delivery-critical reading is not something to make optional.
    //
    // Over-ceiling reads in `warn` — the §6.1 colour-blind-safe pairing (gold
    // accent vs desaturated red survives as a luminance difference). Against
    // the USER's ceiling, not a literal: the threshold was hard-coded to −1.0,
    // which is merely the ceiling's DEFAULT, so any other setting warned at
    // the wrong level — silent while genuinely over at a −6 ceiling, red while
    // legal at a raised one. The ceiling is the user's to move, so a
    // non-default value is the ordinary case.
    //
    // THE COMPARISON IS DELIBERATELY MODE-BLIND, and at the shipped defaults
    // that makes it warn often. This row always measures TRUE peak; the
    // ceiling since ADR-0015 is a SAMPLE-peak limit until the user engages TP,
    // and it sits at −0.1 dB — so a genuine inter-sample over of roughly
    // 0.5–1.5 dB is the ordinary reading, not the alarming one, and the colour
    // then says "you are not in TP mode" more often than "you did something
    // wrong". That is the honest arithmetic: the number really is above the
    // ceiling. Making it quieter means choosing a different comparand — the
    // sample peak, or the ceiling only while TP is engaged — which trades an
    // over-warning for an under-warning and is a product call, not a bug fix.
    // Recorded as an open fine-review question in ADR-0015 §Consequences
    // rather than decided here. ADR-0020 adds the SAMPLE-peak row beside it,
    // which is what lets a user read the difference directly instead of
    // inferring it from the warn colour.
    statRow ("TP", fmt (shownTp, 2) + " dBTP",
             shownTp > shownCeiling && shownTp > silent);
    // SAMPLE PEAK against the same ceiling, and here the comparison is exact
    // rather than mode-blind: with true-peak mode off the ceiling IS a
    // sample-peak limit, and with it on the clamp is stricter still, so this
    // row warning means the clamp was genuinely exceeded either way.
    statRow ("SP", fmt (shownPeak, 2) + " dBFS",
             shownPeak > shownCeiling && shownPeak > silent);
    statRow ("RMS", fmt (shownRms, 1) + " dBFS", false);
    // LRA's "nothing measured yet" is a NEGATIVE sentinel rather than the
    // −99 the levels use: 0 LU is a legitimate reading (a perfectly steady
    // programme), so the usual floor test would print "-" for a real answer.
    statRow ("LRA", shownLra < 0.0f ? juce::String ("-")
                                    : juce::String (shownLra, 1) + " LU", false);
    statRow ("PLR", shownI <= silent ? juce::String ("-")
                                     : juce::String (shownPlr, 1), false);
    // Nothing follows. A trailing `area.removeFromTop (6)` stood here as the
    // gap before the §6.4 penalty rows; those left with the streaming-target
    // display (ADR-0015) and the statement became a reserved gap reserving
    // nothing — no later statement reads `area`. The 6 px gap ABOVE the first
    // numeric row is a different one and is still live.
}
