// ============================================================================
//  createAnabasisLayout — the 49 parameters of DESIGN §4.2, values verbatim.
//
//  Copy-and-adapt provenance (ADR-0009): the Raw* exact-normalised discrete
//  classes, the formatter/parser shapes and the log-range helper are adapted
//  from Anamorph:src/PluginParameters.cpp:11-124 [Verified] — the pluginval
//  state-restoration contract they exist for is identical here. Upstream is
//  READ-ONLY; changes happen in this copy only.
// ============================================================================

#include "PluginParameters.h"
#include <atomic>

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
auto pctText = [] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; };
auto hzText  = [] (float v, int) { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz"
                                                       : juce::String (juce::roundToInt (v)) + " Hz"; };

auto dbFrom  = [] (const juce::String& t) { return t.removeCharacters ("dB ").getFloatValue(); };
auto msFrom  = [] (const juce::String& t) { return t.removeCharacters ("ms ").getFloatValue(); };
auto pctFrom = [] (const juce::String& t) { return t.removeCharacters ("% ").getFloatValue(); };
auto hzFrom  = [] (const juce::String& t)
{
    auto s = t.toLowerCase().trim();
    const bool k = s.containsChar ('k');
    const float v = s.removeCharacters ("khz ").getFloatValue();
    return k ? v * 1000.0f : v;
};

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

juce::AudioProcessorValueTreeState::ParameterLayout createAnabasisLayout()
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
    floatParam (pid::ceiling, "Ceiling", { -20.0f, 0.0f }, -1.0f,
                [] (float v, int) { return juce::String (v, 1) + " dBTP"; }, dbFrom);
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

    // Rows 20-25, 48-49 (clip / colour). colourModel defaults to Tape, NOT
    // Clean — Clean is the null model and would make the Character macro inert
    // in the factory patch (§4.2 footnote ⁵); colourDepth 0 keeps the default
    // patch bit-identical either way.
    floatParam (pid::clipShape, "Clip Shape", { 0.0f, 1.0f }, 0.5f,
                [] (float v, int) { return juce::String (v, 2); },
                [] (const juce::String& t) { return t.getFloatValue(); });
    floatParam (pid::clipDrive, "Clip Drive", { 0.0f, 24.0f }, 0.0f, dbText, dbFrom);
    floatParam (pid::clipMix, "Clip Mix", { 0.0f, 100.0f }, 100.0f, pctText, pctFrom);
    choiceParam (pid::colourModel, "Colour", { "Clean", "Tape", "Tube", "Transistor" }, 1);
    floatParam (pid::colourBalance, "Odd/Even", { -1.0f, 1.0f }, 0.0f,
                [] (float v, int) { return juce::String (v, 2); },
                [] (const juce::String& t) { return t.getFloatValue(); });
    floatParam (pid::colourTone, "Colour Tone", { -1.0f, 1.0f }, 0.0f,
                [] (float v, int) { return juce::String (v, 2); },
                [] (const juce::String& t) { return t.getFloatValue(); });
    floatParam (pid::dynTilt, "Dynamic Tame", { 0.0f, 2.0f }, 0.0f, dbText, dbFrom);
    floatParam (pid::colourDepth, "Colour Depth", { 0.0f, 100.0f }, 0.0f, pctText, pctFrom);

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
    floatParam (pid::stereoLink, "Stereo Link", { 0.0f, 100.0f }, 100.0f, pctText, pctFrom);
    floatParam (pid::transientPreserve, "Transients", { 0.0f, 100.0f }, 50.0f, pctText, pctFrom);
    boolParam  (pid::truePeakMode, "True Peak", true, false);

    // Rows 34-45 (eq)
    floatParam (pid::eqTilt, "Tilt", { -3.0f, 3.0f }, 0.0f, dbText, dbFrom);
    floatParam (pid::eqLowShelfFreq,  "LS Freq", logRange (20.0f, 500.0f),     100.0f,  hzText, hzFrom);
    floatParam (pid::eqLowShelfGain,  "LS Gain", { -12.0f, 12.0f },            0.0f,    dbText, dbFrom);
    floatParam (pid::eqHighShelfFreq, "HS Freq", logRange (1000.0f, 20000.0f), 8000.0f, hzText, hzFrom);
    floatParam (pid::eqHighShelfGain, "HS Gain", { -12.0f, 12.0f },            0.0f,    dbText, dbFrom);
    floatParam (pid::eqBell1Freq, "Bell 1 Freq", logRange (20.0f, 20000.0f),   300.0f,  hzText, hzFrom);
    floatParam (pid::eqBell1Gain, "Bell 1 Gain", { -12.0f, 12.0f },            0.0f,    dbText, dbFrom);
    floatParam (pid::eqBell1Q,    "Bell 1 Q",    logRange (0.3f, 8.0f),        1.0f,
                [] (float v, int) { return juce::String (v, 2); },
                [] (const juce::String& t) { return t.getFloatValue(); });
    floatParam (pid::eqBell2Freq, "Bell 2 Freq", logRange (20.0f, 20000.0f),   3000.0f, hzText, hzFrom);
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
bool isViewTierParam (const juce::String& paramID)
{
    return paramID == pid::bypass || paramID == pid::loudnessComp
        || paramID == pid::deltaMonitor || paramID == pid::advancedMode;
}

bool isPresetExcludedParam (const juce::String& paramID)
{
    return isViewTierParam (paramID) || paramID == pid::freeze;
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
    pid::clipShape, pid::clipDrive, pid::clipMix, pid::colourModel,
    pid::colourBalance, pid::colourTone, pid::dynTilt, pid::colourDepth,
    pid::limGain, pid::lookahead, pid::limRelease, pid::limAutoRelease,
    pid::limStyle, pid::stereoLink, pid::transientPreserve, pid::truePeakMode,
    pid::eqTilt, pid::eqLowShelfFreq, pid::eqLowShelfGain,
    pid::eqHighShelfFreq, pid::eqHighShelfGain,
    pid::eqBell1Freq, pid::eqBell1Gain, pid::eqBell1Q,
    pid::eqBell2Freq, pid::eqBell2Gain, pid::eqBell2Q, pid::eqPosition,
    pid::ceiling, pid::dither, pid::ditherShaping,
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
}
