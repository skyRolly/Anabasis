#include "LoudnessMeterView.h"
#include "../PluginProcessor.h"

using namespace abgui;

juce::String LoudnessMeterView::tooltipText()
{
    return "Waveform statistics off the output. Click to reset the integrated "
           "measurement, the loudness range and both peak holds.";
}

float LoudnessMeterView::plrFromShown (float tpDb, float integratedLufs) noexcept
{
    return integratedLufs > anabasis::LoudnessMeter::kSilentLufs + 1.0f
             ? tpDb - integratedLufs
             : 0.0f;
}

bool LoudnessMeterView::shouldAdoptRms (float rawDb, bool holdingAReading,
                                        double sinceAdoptSecs) noexcept
{
    // Three ways to adopt, and only the third is the cadence:
    //   * the incoming value is the meter's "nothing measured yet" SENTINEL —
    //     a meter reset must blank the row on the next frame, never after a
    //     third of a second of a stale number that no longer describes
    //     anything (`requestMeterReset` publishes `RmsMeter::kSilentDb`);
    //   * we are not currently holding a reading — so the FIRST real value
    //     after a reset (or after the editor opens) lands immediately rather
    //     than waiting out an interval it was never part of;
    //   * the hold has expired, which is the ordinary readable cadence.
    // Everything the reference choice affects is applied downstream of this,
    // so a Settings flip is never gated by the hold.
    return rawDb < anabasis::RmsMeter::kFloorDb
        || ! holdingAReading
        || sinceAdoptSecs >= kRmsReadoutHoldSecs;
}

float LoudnessMeterView::rmsWithReference (float rawDb, bool aes17) noexcept
{
    // AES-17 references a full-scale SINE to 0 dBFS, which is +3.01 dB on the
    // mathematical RMS the meter publishes (a sine's RMS is 1/√2 of its peak).
    // The sentinel passes through UNTOUCHED — offsetting it would print a
    // nonsense −140.99 where the row means "nothing measured yet".
    //
    // `>= kFloorDb` is that test EXACTLY, because the meter guarantees every
    // reading it computes lands at or above the floor and the sentinel sits
    // strictly below it. An earlier `> kSilentDb + 1.0f` was a tolerance
    // around the sentinel, needed only while the two ranges could overlap; a
    // genuine reading in that band was silently denied its offset.
    return (aes17 && rawDb >= anabasis::RmsMeter::kFloorDb) ? rawDb + 3.0103f : rawDb;
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

void LoudnessMeterView::tick (double dt)
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
    // NOT `processor.meterPlr()`, which is TP minus the GATED integrated
    // figure and therefore the wrong reference the moment `int_integratedStd`
    // selects BS.1770-1: the row would disagree with the two rows above it by
    // however far the two standards have diverged. `plrFromShown` takes the
    // difference of the values THIS panel prints, so the three agree under
    // either standard and reproduce the published figure exactly under the
    // gated one. The published value is unchanged — the audio thread keeps
    // publishing the canonical gated PLR, which is what the suite pins.
    const float plr = plrFromShown (tp, i);
    const float pk  = processor.meterPeakMaxDb();
    const float lra = processor.meterLra();
    // The RMS READOUT cadence (0.1.3 item 1). The judgement call, recorded:
    // the fast-moving number is NOT a measurement defect — the 50 ms Hann
    // window is ADR-0020's owner-directed spec, and a 50 ms RMS of programme
    // material genuinely swings several dB at syllable rate — it is a DISPLAY
    // property: this tick re-printed that correct, volatile measurement at
    // the FULL FRAME RATE, so the digits churned faster than they could be
    // read. That rate is the display's, not a fixed one — `FrameClock` paces
    // on vblank and divides to an even cadence of at most ~125 Hz, so the
    // churn was worse on a fast panel than on a 60 Hz one, which is itself a
    // reason the cadence belongs here rather than in the meter. The fix
    // therefore lives here and not in `RmsMeter`
    // (whose reading the suite pins and the bars/holds do not consume): the
    // numeric row adopts a fresh value at a readable ~3 Hz and holds it
    // between adoptions — every printed number is still a real, current
    // 50 ms measurement, just sampled at reading pace.
    //
    // WHAT IS HELD IS THE RAW METER VALUE, NOT THE DISPLAYED ONE, and that
    // distinction is the 0.1.3 review fix. Holding the offset value made the
    // AES-17 / Mathematical reference choice wait out the hold too — up to a
    // third of a second of dead Settings toggle — because the flip changed
    // only the value the hold was suppressing. The hold belongs to the
    // MEASUREMENT (which is what churns); the reference is a pure function of
    // whatever measurement is currently shown, so it is applied AFTER the
    // hold, on every tick. `shownRms` therefore still holds the value after
    // the offset exactly as this class's header states, and a Settings flip
    // moves the snapshot and repaints with no extra flag — the invariant is
    // restored rather than re-worded.
    const float rawRms = processor.meterRmsDb();
    rmsSinceAdopt += dt;
    if (shouldAdoptRms (rawRms, shownRmsValid, rmsSinceAdopt))
    {
        heldRawRms    = rawRms;
        rmsSinceAdopt = 0.0;
        shownRmsValid = rawRms >= anabasis::RmsMeter::kFloorDb;
    }
    const float rms = rmsWithReference (heldRawRms, aes17);
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
    // SAMPLE PEAK against the same ceiling, and here the REFERENCE is exact
    // rather than mode-blind: with true-peak mode off the ceiling IS a
    // sample-peak limit, and with it on the clamp is stricter still, so this
    // row warning means the clamp was genuinely exceeded either way.
    //
    // The COMPARISON needs a tolerance to make that sentence true, because the
    // exactness lives in the linear domain and this row compares in dB.
    // `CeilingClamp` guarantees |x| ≤ ceilingLinear on every clamped sample,
    // but `ceilingLinear` is `dbToGain(ceiling)` and `shownPeak` is
    // `gainToDecibels(|x|max)` — a round trip the parameter itself never makes,
    // and float loses a few ULPs each way, so a fully limited master can read
    // back −0.09999997 against a −0.1 dB ceiling and test greater. The
    // tolerance is HALF this row's own print resolution (it prints 2 dp), which
    // is ~4 orders of magnitude above that error and still below anything the
    // row can show: an exceedance the user can read still warns. It also
    // absorbs the LSB-scale noise the dither stage adds AFTER the clamp — near
    // the ceiling 0.005 dB is ≈ 18 LSB at 16-bit, so flat and shaped TPDF both
    // fit inside it, and neither is a limiter failure. The TP row keeps its
    // exact test deliberately: its over-warning is the documented one above,
    // and it measures a quantity the clamp does not bound.
    static constexpr float kCeilingWarnSlackDb = 0.005f;
    statRow ("SP", fmt (shownPeak, 2) + " dBFS",
             shownPeak > shownCeiling + kCeilingWarnSlackDb && shownPeak > silent);
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
