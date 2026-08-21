// ============================================================================
//  createAnabasisLayout — the 50 parameters of the surface: DESIGN §4.2's 49,
//  values verbatim, plus ADR-0019's compStereoLink (0.1.1).
//
//  Copy-and-adapt provenance (ADR-0009): the Raw* exact-normalised discrete
//  classes, the formatter/parser shapes and the log-range helper are adapted
//  from Anamorph:src/PluginParameters.cpp:11-124 [Verified] — the pluginval
//  state-restoration contract they exist for is identical here. Upstream is
//  READ-ONLY; changes happen in this copy only.
// ============================================================================

#include "PluginParameters.h"
#include <atomic>
#include <cmath>

using juce::AudioParameterFloat;
using juce::NormalisableRange;
using juce::ParameterID;
using juce::StringArray;

namespace
{
// Bump when the parameter set changes so hosts re-scan automation
// (PARAMETER_COMPATIBILITY_POLICY rule 4: any surface change is a kVersion bump).
constexpr int kVersion = 1;

// --- Raw* discrete classes (Anamorph:src/PluginParameters.cpp:11-89) --------
// pluginval's state-restoration check sets a RAW normalised value and expects
// getValue() back within 0.1; stock discrete parameters snap. These keep the
// exact raw value while host text and the DSP see the snapped one.
struct RawChoice : juce::RangedAudioParameter
{
    RawChoice (const ParameterID& id, const juce::String& nm, StringArray c, int defIndex,
               bool isAutomatableIn = true)
        : juce::RangedAudioParameter (id, nm), choices (std::move (c)),
          range (0.0f, (float) juce::jmax (1, choices.size() - 1), 1.0f),
          normValue (toNorm (defIndex)), defaultNorm (toNorm (defIndex)), autom (isAutomatableIn) {}

    float getValue() const override        { return normValue.load(); }
    void  setValue (float v) override      { normValue.store (v); }
    float getDefaultValue() const override { return defaultNorm; }
    int   getNumSteps() const override     { return choices.size(); }
    bool  isDiscrete() const override      { return true; }
    bool  isAutomatable() const override   { return autom; }
    juce::String getText (float v, int) const override { return choices[indexFor (v)]; }
    float getValueForText (const juce::String& t) const override
    { const int i = choices.indexOf (t); return toNorm (i < 0 ? 0 : i); }
    const NormalisableRange<float>& getNormalisableRange() const override { return range; }

    int   indexFor (float v) const { return juce::jlimit (0, choices.size() - 1, juce::roundToInt (v * (float) juce::jmax (1, choices.size() - 1))); }
    float toNorm (int i) const     { return choices.size() > 1 ? (float) juce::jlimit (0, choices.size() - 1, i) / (float) (choices.size() - 1) : 0.0f; }

    StringArray choices;
    NormalisableRange<float> range;
    std::atomic<float> normValue;
    float defaultNorm;
    bool  autom;
};

struct RawBool : juce::RangedAudioParameter
{
    RawBool (const ParameterID& id, const juce::String& nm, bool def, bool isAutomatableIn = true)
        : juce::RangedAudioParameter (id, nm), range (0.0f, 1.0f, 1.0f),
          normValue (def ? 1.0f : 0.0f), defaultNorm (def ? 1.0f : 0.0f), autom (isAutomatableIn) {}

    float getValue() const override        { return normValue.load(); }
    void  setValue (float v) override      { normValue.store (v); }
    float getDefaultValue() const override { return defaultNorm; }
    int   getNumSteps() const override     { return 2; }
    bool  isDiscrete() const override      { return true; }
    bool  isBoolean() const override       { return true; }
    bool  isAutomatable() const override   { return autom; }
    juce::String getText (float v, int) const override { return v >= 0.5f ? "On" : "Off"; }
    float getValueForText (const juce::String& t) const override
    { return (t.equalsIgnoreCase ("on") || t.equalsIgnoreCase ("true") || t.getIntValue() != 0) ? 1.0f : 0.0f; }
    const NormalisableRange<float>& getNormalisableRange() const override { return range; }

    NormalisableRange<float> range;
    std::atomic<float> normValue;
    float defaultNorm;
    bool  autom;
};

// --- Formatters / suffix-tolerant parsers (Anamorph :97-103,153-194) --------
auto dbText  = [] (float v, int) { return juce::String (v, 1) + " dB"; };
auto msText  = [] (float v, int) { return juce::String (v, 1) + " ms"; };
// PERCENT, and since 0.1.6 the DISPLAY PRECISION FOLLOWS THE VALUE: a whole
// percent prints as an integer — "50 %", exactly as it always has — while a
// value that is not a whole percent prints one decimal.
//
// Not cosmetic. `pctFrom` below reads an explicit "0.1 %" as a literal tenth
// of a percent (the owner's 0.1.6 item 2), and an integer-only formatter
// showed that value back as "0 %": the box denied the existence of the value
// the user had just typed into it, which is the one thing a value box must
// never do. The same shape as the ceiling's two decimals (`twoDecimalRange`
// below) — a knob that reads one number while holding another lies about the
// thing it exists to show.
//
// One decimal rather than two, and the same one `dbText`/`msText` already
// print unconditionally, so a percent box carrying a decimal is the surface's
// existing idiom rather than a new one. The residual bound is stated instead
// of hidden: a value below 0.05 % reads "0.0 %" — the tenth it is finer than,
// not the "0 %" that only an exact zero prints, so the two remain
// distinguishable — and closing that last gap would mean quantising the RANGE
// the way the ceiling does, which is a parameter-surface change
// (PARAMETER_COMPATIBILITY_POLICY rule 3) this fix neither needs nor is
// licensed to make.
auto pctText = [] (float v, int)
{
    const float whole = std::round (v);
    return (juce::approximatelyEqual (v, whole) ? juce::String ((int) whole)
                                                : juce::String (v, 1)) + " %";
};
auto hzText  = [] (float v, int) { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz"
                                                       : juce::String (juce::roundToInt (v)) + " Hz"; };

auto dbFrom  = [] (const juce::String& t) { return t.removeCharacters ("dB ").getFloatValue(); };
auto msFrom  = [] (const juce::String& t) { return t.removeCharacters ("ms ").getFloatValue(); };
// PERCENT ENTRY, with the FRACTION rule (0.1.6 item 2, owner directive) —
// the percent boxes' equivalent of the kHz shorthand below. Two spellings
// reach these boxes and they mean different things:
//
//   * a bare number in (0, 1] is a FRACTION of full scale — "0.5" is 50 %,
//     "1" is 100 %. All seven percent parameters span 0…100, so a user who
//     types "0.5" meaning "half" would otherwise land on half of ONE percent:
//     a value indistinguishable from zero at the knob, in the readout and in
//     the sound.
//   * anything carrying an explicit '%' is LITERAL — "0.1 %" is a tenth of a
//     percent. That is the escape hatch the fraction rule requires, because
//     without it the whole sub-1 % region would be untypeable.
//
// The discontinuity at 1 is real and deliberate: "1" is 100 % while "1.5" is
// 1.5 %. One of the two readings has to win at the boundary and the owner's
// example pins which ("1 → 100 %"); "1 %" reaches the other one.
//
// ROUND-TRIP STAYS EXACT, and `pctText` is what guarantees it rather than
// luck: the formatter ALWAYS emits the '%' suffix, so every string this
// product displays takes the literal branch and `getValueForText(getText(v))`
// returns v for every v in range — the (0, 1] region included. A formatter
// that dropped the suffix would multiply small values by 100 on every host
// round-trip, which is why the suffix is a contract here and not decoration.
auto pctFrom = [] (const juce::String& t)
{
    const bool literal = t.containsChar ('%');
    const float v = t.removeCharacters ("% ").getFloatValue();
    return (! literal && v > 0.0f && v <= 1.0f) ? v * 100.0f : v;
};
auto hzFrom  = [] (const juce::String& t)
{
    auto s = t.toLowerCase().trim();
    const bool k = s.containsChar ('k');
    const float v = s.removeCharacters ("khz ").getFloatValue();
    return k ? v * 1000.0f : v;
};
// kHz-biased parsers (0.1.1, owner directive; the sibling's `khzFrom` idea
// classed per knob range). Plain `hzFrom` reads every bare number as Hz, so
// on wide-range knobs the natural mastering shorthand went dead: "8" on the
// 1–20 kHz high shelf clamped to the 1 kHz floor instead of landing 8 kHz.
// Two range classes, two pivots:
//
// `khzFrom` — knobs whose WHOLE range is ≥ 1 kHz (high shelf). The sibling's
// rule verbatim: a bare number ≤ 20 is kHz (the entire legal range expressed
// in kHz units is 1–20, so nothing is lost); above 20 it is Hz. "8" → 8 kHz,
// "8000" → 8 kHz, "8k" → 8 kHz, "5570" → 5.57 kHz.
auto khzFrom = [] (const juce::String& t)
{
    auto s = t.toLowerCase().trim();
    const bool k = s.containsChar ('k');
    const float v = s.removeCharacters ("khz ").getFloatValue();
    if (k) return v * 1000.0f;
    return (v <= 20.0f) ? v * 1000.0f : v;
};
// `hzKhzFrom` — full-range 20 Hz–20 kHz knobs (the bells). The pivot is the
// knob's own 20 Hz FLOOR, exclusive: a bare number STRICTLY below 20 cannot
// be a legal Hz value (it would only clamp to the floor), so it is read as
// kHz — "19" → 19 kHz, "2.38" → 2.38 kHz, "8" → 8 kHz — while every legal
// Hz value stays Hz: "20" → 20 Hz, "155" → 155 Hz, "5570" → 5.57 kHz shown.
// (The owner's 0.1.1 examples pin both sides of the pivot: 19 → kHz,
// 20 → Hz — hence `<`, not the sibling's `<=`.)
auto hzKhzFrom = [] (const juce::String& t)
{
    auto s = t.toLowerCase().trim();
    const bool k = s.containsChar ('k');
    const float v = s.removeCharacters ("khz ").getFloatValue();
    if (k) return v * 1000.0f;
    return (v < 20.0f) ? v * 1000.0f : v;
};

// TWO-DECIMAL RANGE — the quantisation lives in the RANGE, which is what makes
// it true of the VALUE rather than of the label.
//
// `interval` is the whole mechanism, and it is enough because of a detail worth
// naming: the snap is applied by `RangedAudioParameter`, not by the caller.
// `AudioParameterFloat::setValue` — the entry point a host uses for every
// automation write, and the one `AudioProcessorValueTreeState` drives on state
// restore — is `value = convertFrom0to1 (newValue)`, and that
// `convertFrom0to1` is the PARAMETER's, which is
// `range.snapToLegalValue (range.convertFrom0to1 (…))`
// (`juce_RangedAudioParameter.cpp:54-58`; its header says so — "Denormalises and
// snaps"). The same wrapper is what `AudioProcessorValueTreeState`'s adapter
// calls to publish the atomic the DSP reads, so `EngineParameters::ceilingDbTp`
// carries the snapped value and not a rounded display of an unsnapped one.
// `getText` formats `convertFrom0to1 (v)` through the same wrapper, so the label
// cannot disagree with the value it names; `SliderParameterAttachment` copies the
// range's functions AND its interval into the knob; `getValueForText` snaps on
// the way in; and `PresetManager` already routes overrides through
// `snapToLegalValue`. Every path is covered by the one property.
//
// (An earlier draft of this helper supplied custom convertFrom0to1 /
// convertTo0to1 / snapToLegalValue lambdas, on the belief that `setValue` reached
// the RANGE's raw `convertFrom0to1` and so bypassed the interval. It does not.
// The lambdas were redundant, and a mutation run proved it: the parameter cannot
// tell the two implementations apart. Recorded because the wrong belief is the
// natural reading of `juce_AudioParameterFloat.cpp:98` on its own.)
//
// The tie rule is JUCE's: `start + interval * floor ((v - start) / interval + 0.5)`
// rounds a halfway value toward +infinity, so -0.125 lands on -0.12. The owner's
// directive leaves the tie open ("-0.12 or -0.13 depending on the chosen rounding
// rule"); every non-tie case rounds to nearest, so -0.123 is -0.12 and -0.129 is
// -0.13. In float the result is the nearest representable neighbour of the
// two-decimal value, which is what "two decimal places" means in binary floating
// point — no THIRD-decimal step is reachable, and that is the requirement.
//
// It does NOT make the parameter discrete to a host:
// `AudioParameterFloat::getNumSteps()` returns the base default rather than
// deriving from the range (`juce_AudioParameterFloat.cpp:100`), so the automation
// surface stays continuous while every value it can land on is on the grid — and
// `tests/fixtures/parameter_registry.snapshot`, which pins the step count, is
// unchanged.
NormalisableRange<float> twoDecimalRange (float lo, float hi)
{
    return { lo, hi, 0.01f };
}

// True logarithmic (octave-even) range for every ⊕(log) row in §4.2
// (Anamorph:src/PluginParameters.cpp:107-113).
NormalisableRange<float> logRange (float lo, float hi)
{
    return { lo, hi,
        [] (float s, float e, float v) { return s * std::pow (e / s, v); },
        [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
        [] (float s, float e, float v) { return juce::jlimit (s, e, v); } };
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout
createAnabasisLayout (const CeilingUnitSource* ceilingUnit)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto floatParam = [&] (const char* id, const juce::String& name,
                           NormalisableRange<float> range, float def,
                           std::function<juce::String (float, int)> toText,
                           std::function<float (const juce::String&)> fromText,
                           bool automatable = true)
    {
        auto attr = juce::AudioParameterFloatAttributes()
                        .withStringFromValueFunction (std::move (toText))
                        .withValueFromStringFunction (std::move (fromText));
        if (! automatable)
            attr = attr.withAutomatable (false);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, kVersion },
                                                           name, range, def, attr));
    };
    auto boolParam = [&] (const char* id, const juce::String& name, bool def, bool automatable = true)
    {
        layout.add (std::make_unique<RawBool> (ParameterID { id, kVersion }, name, def, automatable));
    };
    auto choiceParam = [&] (const char* id, const juce::String& name, StringArray choices,
                            int def, bool automatable = true)
    {
        layout.add (std::make_unique<RawChoice> (ParameterID { id, kVersion }, name,
                                                 std::move (choices), def, automatable));
    };

    // Rows 1-9 (view / macro / shared / adaptive / monitor). Footnotes ¹²:
    // advancedMode is non-automatable (X11 editor-resize crash path) and the
    // three macros are non-automatable by §5.2 — the managed Advanced
    // parameters are the automation surface.
    boolParam  (pid::bypass,       "Bypass",   false);
    boolParam  (pid::advancedMode, "Advanced", false, false);
    floatParam (pid::loudness,  "Loudness",  { 0.0f, 100.0f }, 0.0f, pctText, pctFrom, false);
    floatParam (pid::character, "Character", { 0.0f, 1.0f },   0.0f,
                [] (float v, int) { return juce::String (v, 2); },
                [] (const juce::String& t) { return t.getFloatValue(); }, false);
    floatParam (pid::tone, "Tone", { -1.0f, 1.0f }, 0.0f,
                [] (float v, int) { return juce::String (v, 2); },
                [] (const juce::String& t) { return t.getFloatValue(); }, false);
    // The UNIT FOLLOWS THE MODE (ADR-0015, `CeilingUnitSource` in the header):
    // " dBTP" only while true-peak mode is engaged, " dB" otherwise — the
    // ceiling is a sample-peak limit with it off (ADR-0006 item 3), so an
    // unconditional dBTP suffix advertised an inter-sample guarantee the
    // default configuration does not make. `dbFrom` parses the leading float,
    // so both spellings round-trip through `getValueForText` identically.
    //
    // TWO DECIMAL PLACES, and the range is what enforces it (see
    // `twoDecimalRange`): the ceiling is the one control a user dials to a
    // precise number against a delivery spec, and a knob that reads "-0.1 dB"
    // while holding -0.14 is a knob that lies about the thing it exists to
    // guarantee. The display width follows the value's precision rather than
    // leading it — `String (v, 2)` prints a value that IS two-decimal, so the
    // two can never disagree.
    floatParam (pid::ceiling, "Ceiling", twoDecimalRange (-20.0f, 0.0f), -0.1f,
                [ceilingUnit] (float v, int)
                {
                    const bool tp = ceilingUnit != nullptr && ceilingUnit->truePeakEngaged();
                    return juce::String (v, 2) + (tp ? " dBTP" : " dB");
                }, dbFrom);
    boolParam  (pid::freeze,       "Freeze",        false, false);
    boolParam  (pid::loudnessComp, "Loudness Comp", false);
    boolParam  (pid::deltaMonitor, "Delta",         false);

    // Rows 10-11 (input / detectors)
    floatParam (pid::inputGain, "Input Gain", { -12.0f, 24.0f }, 0.0f, dbText, dbFrom);
    floatParam (pid::scHpfFreq, "SC HPF", logRange (20.0f, 300.0f), 20.0f, hzText, hzFrom);

    // Rows 12-19 (comp)
    floatParam (pid::compRatio, "Comp Ratio", { 1.1f, 4.0f }, 1.5f,
                [] (float v, int) { return juce::String (v, 2) + ":1"; },
                [] (const juce::String& t) { return t.upToFirstOccurrenceOf (":", false, false).getFloatValue(); });
    floatParam (pid::compThreshold, "Comp Threshold", { -40.0f, 0.0f }, 0.0f, dbText, dbFrom);
    floatParam (pid::compAttack,  "Comp Attack",  logRange (5.0f, 100.0f),   30.0f,  msText, msFrom);
    floatParam (pid::compRelease, "Comp Release", logRange (50.0f, 1000.0f), 200.0f, msText, msFrom);
    boolParam  (pid::compAutoRelease, "Comp Auto Rel", true);
    floatParam (pid::compKnee, "Comp Knee", { 0.0f, 12.0f }, 6.0f, dbText, dbFrom);
    choiceParam (pid::compDetector, "Comp Detector", { "RMS", "Peak" }, 0);
    floatParam (pid::compMix, "Comp Mix", { 0.0f, 100.0f }, 100.0f, pctText, pctFrom);
    // ADR-0019 (0.1.1): the comp's own stereo link, named apart from the
    // limiter's "Stereo Link" so the two automation lanes cannot be confused.
    // Default 100 % = the fully linked detector the comp always had — the
    // addition is backwards-inert (an old session simply loads the default).
    floatParam (pid::compStereoLink, "Comp Stereo Link", { 0.0f, 100.0f }, 100.0f, pctText, pctFrom);

    // Rows 20-25, 48-49 (clip / colour). colourModel defaults to Tape, NOT
    // Clean — Clean is the null model and would make the Character macro inert
    // in the factory patch (§4.2 footnote ⁵); colourDepth 0 keeps the default
    // patch bit-identical either way.
    floatParam (pid::clipShape, "Clip Shape", { 0.0f, 1.0f }, 0.5f,
                [] (float v, int) { return juce::String (v, 2); },
                [] (const juce::String& t) { return t.getFloatValue(); });
    floatParam (pid::clipDrive, "Clip Drive", { 0.0f, 24.0f }, 0.0f, dbText, dbFrom);
    floatParam (pid::clipMix, "Clip Mix", { 0.0f, 100.0f }, 100.0f, pctText, pctFrom);
    // "Color", US spelling, in every user-facing NAME since 0.1.3 (owner
    // directive, item 6) — the IDs keep their historical `colour*` spelling:
    // an ID is a compatibility key, never shown to anyone, and renaming one
    // is the hard-stop PARAMETER_COMPATIBILITY_POLICY rule 1 forbids. A NAME
    // is rule 2's legal move: snapshot re-frozen, same as 0.1.2's "Limiter
    // Stereo Link".
    choiceParam (pid::colourModel, "Color", { "Clean", "Tape", "Tube", "Transistor" }, 1);
    floatParam (pid::colourBalance, "Odd/Even", { -1.0f, 1.0f }, 0.0f,
                [] (float v, int) { return juce::String (v, 2); },
                [] (const juce::String& t) { return t.getFloatValue(); });
    floatParam (pid::colourTone, "Color Tone", { -1.0f, 1.0f }, 0.0f,
                [] (float v, int) { return juce::String (v, 2); },
                [] (const juce::String& t) { return t.getFloatValue(); });
    floatParam (pid::dynTilt, "Dynamic Tame", { 0.0f, 2.0f }, 0.0f, dbText, dbFrom);
    floatParam (pid::colourDepth, "Color Depth", { 0.0f, 100.0f }, 0.0f, pctText, pctFrom);

    // Rows 26-33 (limiter). lookahead: log taper, NO zero/off position
    // (§3.4 / OQ-010 — a 0 ms limiter degenerates into a clipper); it is
    // non-automatable because the engaged value is a live read offset
    // (§4.2 footnote ³ — NOT for latency reasons, the reported figure is
    // constant in it per ADR-0004). truePeakMode: conservative v1 freeze
    // (footnote ⁴).
    floatParam (pid::limGain, "Limiter Gain", { 0.0f, 18.0f }, 0.0f, dbText, dbFrom);
    floatParam (pid::lookahead, "Lookahead", logRange (0.5f, 10.0f), 2.0f, msText, msFrom, false);
    floatParam (pid::limRelease, "Lim Release", logRange (1.0f, 1000.0f), 100.0f, msText, msFrom);
    boolParam  (pid::limAutoRelease, "Lim Auto Rel", true);
    choiceParam (pid::limStyle, "Style", { "Transparent", "Punchy", "Loud" }, 0);
    // "Limiter Stereo Link" since 0.1.2 (a display-name rename, ID unchanged;
    // PARAMETER_COMPATIBILITY_POLICY rule 2, snapshot re-frozen): with the
    // ADR-0019 "Comp Stereo Link" beside it, a bare "Stereo Link" left the
    // limiter's automation lane the ambiguous one of the pair. Both modules'
    // EDITOR captions read "Stereo Link" — the panel says which stage.
    floatParam (pid::stereoLink, "Limiter Stereo Link", { 0.0f, 100.0f }, 100.0f, pctText, pctFrom);
    floatParam (pid::transientPreserve, "Transients", { 0.0f, 100.0f }, 50.0f, pctText, pctFrom);
    boolParam  (pid::truePeakMode, "True Peak", false, false);

    // Rows 34-45 (eq)
    floatParam (pid::eqTilt, "Tilt", { -3.0f, 3.0f }, 0.0f, dbText, dbFrom);
    floatParam (pid::eqLowShelfFreq,  "LS Freq", logRange (20.0f, 500.0f),     100.0f,  hzText, hzFrom);
    floatParam (pid::eqLowShelfGain,  "LS Gain", { -12.0f, 12.0f },            0.0f,    dbText, dbFrom);
    floatParam (pid::eqHighShelfFreq, "HS Freq", logRange (1000.0f, 20000.0f), 8000.0f, hzText, khzFrom);
    floatParam (pid::eqHighShelfGain, "HS Gain", { -12.0f, 12.0f },            0.0f,    dbText, dbFrom);
    floatParam (pid::eqBell1Freq, "Bell 1 Freq", logRange (20.0f, 20000.0f),   300.0f,  hzText, hzKhzFrom);
    floatParam (pid::eqBell1Gain, "Bell 1 Gain", { -12.0f, 12.0f },            0.0f,    dbText, dbFrom);
    floatParam (pid::eqBell1Q,    "Bell 1 Q",    logRange (0.3f, 8.0f),        1.0f,
                [] (float v, int) { return juce::String (v, 2); },
                [] (const juce::String& t) { return t.getFloatValue(); });
    floatParam (pid::eqBell2Freq, "Bell 2 Freq", logRange (20.0f, 20000.0f),   3000.0f, hzText, hzKhzFrom);
    floatParam (pid::eqBell2Gain, "Bell 2 Gain", { -12.0f, 12.0f },            0.0f,    dbText, dbFrom);
    floatParam (pid::eqBell2Q,    "Bell 2 Q",    logRange (0.3f, 8.0f),        1.0f,
                [] (float v, int) { return juce::String (v, 2); },
                [] (const juce::String& t) { return t.getFloatValue(); });
    choiceParam (pid::eqPosition, "EQ Position", { "Pre", "Post" }, 0);

    // Rows 46-47 (output). Both ⊕ non-automatable — conservative v1 freezes;
    // loosening later is a kVersion bump + ADR (§11 risk 4).
    choiceParam (pid::dither, "Dither", { "Off", "16-bit", "24-bit" }, 0, false);
    boolParam   (pid::ditherShaping, "Noise Shaping", false, false);

    return layout;
}

// ----------------------------------------------------------------------------
// The A/B-travel exclusion. `advancedMode` LEFT this set in ADR-0018: it now
// travels with an UNDO step (the owner's 0.1.1 directive — an ADV toggle is
// a user action the user can take back) while staying pinned across A/B
// switches and Copies (`applySlotToLive`'s adoptAdvanced flag), because an
// A/B compare is a SOUND compare and must not resize the editor (the half of
// ADR-0010 option E that survives). It also stays out of PRESETS — explicitly,
// below — and stays non-automatable (the X11 editor-resize crash path).
bool isViewTierParam (const juce::String& paramID)
{
    return paramID == pid::bypass || paramID == pid::loudnessComp
        || paramID == pid::deltaMonitor;
}

// A NEW view-tier or monitor parameter MUST be added above (or here), and this
// is the sentence that says why rather than leaving it to be rediscovered: the
// factory-preset apply resets every non-excluded PARAM to its default before
// laying the table's overrides on top (`PresetManager::applyFactoryPreset`).
// Everything this predicate does not name is therefore reset by BROWSING
// presets — a future "monitor solo" or a second view toggle would silently
// flip off every time the user auditioned a preset, and the defaults pass would
// look innocent while doing it. The exclusion set is the only thing standing
// between that pass and the view state.
//
// `advancedMode` is named HERE rather than inherited from the view tier since
// ADR-0018 (it undoes, so it is no longer view-tier), for exactly the hazard
// this comment describes: without it, browsing a preset would slam the editor
// back to Simple on every audition.
bool isPresetExcludedParam (const juce::String& paramID)
{
    return isViewTierParam (paramID) || paramID == pid::freeze
        || paramID == pid::advancedMode;
}

// ----------------------------------------------------------------------------
namespace
{
// One canonical order for the cache slots; toEngine() indexes it.
constexpr const char* kCacheOrder[] = {
    pid::bypass, pid::loudnessComp, pid::deltaMonitor,
    pid::inputGain, pid::scHpfFreq,
    pid::compRatio, pid::compThreshold, pid::compAttack, pid::compRelease,
    pid::compAutoRelease, pid::compKnee, pid::compDetector, pid::compMix,
    pid::compStereoLink,
    pid::clipShape, pid::clipDrive, pid::clipMix, pid::colourModel,
    pid::colourBalance, pid::colourTone, pid::dynTilt, pid::colourDepth,
    pid::limGain, pid::lookahead, pid::limRelease, pid::limAutoRelease,
    pid::limStyle, pid::stereoLink, pid::transientPreserve, pid::truePeakMode,
    pid::eqTilt, pid::eqLowShelfFreq, pid::eqLowShelfGain,
    pid::eqHighShelfFreq, pid::eqHighShelfGain,
    pid::eqBell1Freq, pid::eqBell1Gain, pid::eqBell1Q,
    pid::eqBell2Freq, pid::eqBell2Gain, pid::eqBell2Q, pid::eqPosition,
    pid::ceiling, pid::dither, pid::ditherShaping,
    pid::freeze,
};
static_assert (std::size (kCacheOrder) == (size_t) kCachedParamCount,
               "kCacheOrder and CachedParams::raw must stay the same length — the "
               "cache order and toEngine's assignment sequence are positionally coupled");
} // namespace

void CachedParams::resolve (juce::AudioProcessorValueTreeState& apvts)
{
    for (size_t i = 0; i < std::size (kCacheOrder); ++i)
    {
        raw[i] = apvts.getRawParameterValue (kCacheOrder[i]);
        // A null slot is otherwise SILENT: toEngine's f() substitutes 0.0f, so
        // a typo or a removed ID lands compRatio = 0 or scHpfFreqHz = 0 in the
        // snapshot and nothing trips. testCachedParamsMapping checks the fields
        // it sets; the ones it does not (compAttack, clipMix, limStyle,
        // truePeakMode, dither…) would pass a null through. The null substitute
        // stays — it keeps toEngine noexcept and branch-predictable on the audio
        // thread — but resolving is a message-thread act and is checked here.
        jassert (raw[i] != nullptr);
    }
}

bool CachedParams::allResolved() const noexcept
{
    for (auto* p : raw)
        if (p == nullptr)
            return false;
    return true;
}

void CachedParams::toEngine (anabasis::EngineParameters& out) const noexcept
{
    size_t i = 0;
    auto f = [&]() noexcept { return raw[i] != nullptr ? raw[i++]->load() : (i++, 0.0f); };
    auto b = [&]() noexcept { return f() >= 0.5f; };
    auto c = [&]() noexcept { return (int) juce::roundToInt (f()); };

    out.bypass            = b();
    out.loudnessComp      = b();
    out.deltaMonitor      = b();
    out.inputGainDb       = f();
    out.scHpfFreqHz       = f();
    out.compRatio         = f();
    out.compThresholdDb   = f();
    out.compAttackMs      = f();
    out.compReleaseMs     = f();
    out.compAutoRelease   = b();
    out.compKneeDb        = f();
    out.compDetector      = c();
    out.compMix           = f() * 0.01f;
    out.compStereoLink    = f() * 0.01f;
    out.clipShape         = f();
    out.clipDriveDb       = f();
    out.clipMix           = f() * 0.01f;
    out.colourModel       = c();
    out.colourBalance     = f();
    out.colourTone        = f();
    out.dynTiltDb         = f();
    out.colourDepth       = f() * 0.01f;
    out.limGainDb         = f();
    out.lookaheadMs       = f();
    out.limReleaseMs      = f();
    out.limAutoRelease    = b();
    out.limStyle          = c();
    out.stereoLink        = f() * 0.01f;
    out.transientPreserve = f() * 0.01f;
    out.truePeakMode      = b();
    out.eqTiltDb          = f();
    out.eqLowShelfFreqHz  = f();
    out.eqLowShelfGainDb  = f();
    out.eqHighShelfFreqHz = f();
    out.eqHighShelfGainDb = f();
    out.eqBell1FreqHz     = f();
    out.eqBell1GainDb     = f();
    out.eqBell1Q          = f();
    out.eqBell2FreqHz     = f();
    out.eqBell2GainDb     = f();
    out.eqBell2Q          = f();
    out.eqPosition        = c();
    out.ceilingDbTp       = f();
    out.ditherMode        = c();
    out.ditherShaping     = b();
    out.freeze            = b();
}
