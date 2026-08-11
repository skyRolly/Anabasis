// ============================================================================
//  AnabasisChannelProbe — KI-009's runtime probe, against the SHIPPED BINARY.
//
//  WHY THIS EXISTS, and why the existing suites do not cover it.
//
//  `AnabasisStateTests` drives the real `AnabasisAudioProcessor::processBlock`,
//  but it does so by COMPILING the plugin sources into a console app: no LTO
//  (ADR-0008 keeps `juce_recommended_lto_flags` on the plugin target alone), no
//  VST3/AU wrapper, and — on macOS — a thin binary for whichever slice the test
//  app was built as. The artefact a user installs is none of those things: it is
//  LTO'd, universal, and reached through a format wrapper.
//
//  So the configuration KI-009 is reported in has never been asserted on. This
//  probe closes exactly that gap: it LOADS the built bundle the way a host does
//  and measures per-channel output. Everything between `host -> wrapper ->
//  processBlock -> engine -> output buffer` is the real code path, including
//  whatever the optimiser did to it.
//
//  It is deliberately NOT another unit test. Its oracle is the one the field
//  report uses — "is there audio on both channels" — and its configurations are
//  the ones the owner reports, not synthetic extremes: Clip Mix non-zero with a
//  pushed chain, both stereo links at 0.
//
//  Usage:  AnabasisChannelProbe <bundle path> [--format vst3|au] [--verbose]
//  Exit:   0 = every configuration kept both channels; 1 = a channel was lost;
//          2 = the plugin could not be loaded (an environment failure, which is
//          deliberately a DIFFERENT exit code from a real defect).
// ============================================================================

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

namespace
{

struct Config
{
    const char* name;
    float clipMixPercent;      // < 0 leaves the default
    float clipDriveDb;
    float colourDepthPercent;
    float dynTiltDb;
    float limGainDb;
    float compLinkPercent;
    float limLinkPercent;
};

// The reported field configuration and its neighbourhood. Clip Mix 0 is included
// as the CONTROL: the owner reports it as the setting that makes the fault go
// away, so a run where every case fails EXCEPT that one is a very different
// result from a run where everything passes.
const Config kConfigs[] = {
    // name                       mix    drive  colour  tame  limGain compLink limLink
    { "defaults",                 -1.0f,  0.0f,   0.0f, 0.0f,   0.0f,  100.0f, 100.0f },
    { "field: mix 100, links 0",  100.0f, 15.4f, 70.0f, 2.0f,  12.6f,    0.0f,   0.0f },
    { "field: mix 50,  links 0",   50.0f, 15.4f, 70.0f, 2.0f,  12.6f,    0.0f,   0.0f },
    { "field: mix 25,  links 0",   25.0f, 15.4f, 70.0f, 2.0f,  12.6f,    0.0f,   0.0f },
    { "field: mix 1,   links 0",    1.0f, 15.4f, 70.0f, 2.0f,  12.6f,    0.0f,   0.0f },
    { "control: mix 0, links 0",    0.0f, 15.4f, 70.0f, 2.0f,  12.6f,    0.0f,   0.0f },
    { "field: mix 100, links 100",100.0f, 15.4f, 70.0f, 2.0f,  12.6f,  100.0f, 100.0f },
    { "pushed, colour off",       100.0f, 15.4f,  0.0f, 0.0f,  12.6f,    0.0f,   0.0f },
};

// MATCHED BY DISPLAY NAME, NOT BY PARAMETER ID, and the difference is the whole
// reason this function has a comment. Across a FORMAT WRAPPER the ids are not
// ours: JUCE's VST3 client hashes each `ParameterID` into a Steinberg
// `ParamID`, so a hosted instance reports `id='773352680'` where the source says
// `"bypass"`. Matching on the id therefore found nothing, and the first version
// of this probe ran all eight configurations at their DEFAULTS while reporting
// them by name — the same vacuity this repository has now found three times.
// The display name is what survives the wrapper, and it is also what the owner
// reads off the UI when describing a reproduction.
static bool setParam (juce::AudioPluginInstance& p, const char* displayName, float denormalised,
               float rangeLo, float rangeHi)
{
    for (int i = 0; i < p.getParameters().size(); ++i)
    {
        if (auto* hosted = p.getHostedParameter (i))
        {
            if (hosted->getName (64).trim() == juce::String (displayName))
            {
                // The linear map is correct for every name this probe writes —
                // all eight are registered with plain ranges — but a HOSTED
                // parameter exposes no `NormalisableRange`, so the probe cannot
                // ask; it can only assume. That assumption is exactly the kind
                // this file exists to distrust: point it at a `logRange` control
                // (`lookahead`, `scHpfFreq`, every EQ frequency) and it would
                // push a different value than the row name claims while still
                // printing the row name, which is the "the case is not the case
                // it says it is" failure the probe was written after.
                //
                // So it is VERIFIED rather than assumed, and verified through
                // the one channel the wrapper does carry: the parameter's own
                // display text. `getText` formats what the plugin actually
                // holds, through the plugin's own range and formatter, so a
                // taper mismatch shows up as a number that is not the number
                // asked for. Fatal, not a warning — a configuration whose
                // parameters landed somewhere else is a different test wearing
                // this one's name.
                const float norm = juce::jlimit (0.0f, 1.0f,
                                                 (denormalised - rangeLo) / (rangeHi - rangeLo));
                hosted->setValueNotifyingHost (norm);

                const auto shown = hosted->getText (hosted->getValue(), 32).trim();
                const float readBack = shown.getFloatValue();
                // The tolerance is a display tolerance, not a DSP one: the text
                // is rounded for humans (percent to the integer, dB to one or
                // two places), so 0.51 absolute covers the coarsest of those
                // while still being far tighter than any taper error — a log
                // range over these spans misplaces a mid-scale value by whole
                // units, not by half of one.
                if (std::abs (readBack - denormalised) > 0.51f)
                {
                    std::printf ("  PARAMETER '%s' DID NOT LAND: asked %.3f, plugin reads '%s'"
                                 " -- the linear map in setParam does not match this"
                                 " parameter's range\n",
                                 displayName, denormalised, shown.toRawUTF8());
                    return false;
                }
                return true;
            }
        }
    }
    std::printf ("  MISSING PARAMETER '%s' on the hosted instance\n", displayName);
    return false;
}

// A configuration whose parameters did not apply is not a weaker test, it is a
// DIFFERENT test wearing the same name — it silently re-runs the defaults. That
// exact vacuity has now been found twice in this repository's own suites, so
// here it is fatal rather than a warning.
static void dumpParameterIds (juce::AudioPluginInstance& p)
{
    std::printf ("  hosted parameters (%d):\n", p.getParameters().size());
    for (int i = 0; i < p.getParameters().size(); ++i)
        if (auto* hosted = p.getHostedParameter (i))
            std::printf ("    [%2d] id='%s'  name='%s'\n", i,
                         hosted->getParameterID().toRawUTF8(),
                         hosted->getName (40).toRawUTF8());
}

struct Result { double rmsL = 0.0, rmsR = 0.0; bool nonFinite = false; int paramsApplied = 7; };

static Result run (juce::AudioPluginFormatManager& fm, const juce::PluginDescription& desc,
            const Config& c, double sr, int blockSize)
{
    // A FRESH INSTANCE PER CONFIGURATION. The first version reused one instance
    // and skipped the parameter writes for the "defaults" row (its mix is < 0),
    // so that row silently inherited whatever the previous row had pushed — it
    // reported 0.58 RMS where a defaulted plugin gives 0.176. Configurations
    // that leak into each other are not eight tests, they are one test with a
    // history, and the history is invisible in the output.
    juce::String err;
    auto owned = fm.createPluginInstance (desc, sr, blockSize, err);
    if (owned == nullptr)
    {
        Result bad; bad.paramsApplied = -1; return bad;
    }
    auto& p = *owned;
    p.setNonRealtime (false);
    p.prepareToPlay (sr, blockSize);

    Result r;
    if (c.clipMixPercent >= 0.0f)
    {
        int applied = 0;
        r.paramsApplied = 0;
        applied += setParam (p, "Clip Mix",            c.clipMixPercent,     0.0f, 100.0f) ? 1 : 0;
        applied += setParam (p, "Clip Drive",          c.clipDriveDb,        0.0f,  24.0f) ? 1 : 0;
        applied += setParam (p, "Color Depth",         c.colourDepthPercent, 0.0f, 100.0f) ? 1 : 0;
        applied += setParam (p, "Dynamic Tame",        c.dynTiltDb,          0.0f,   2.0f) ? 1 : 0;
        applied += setParam (p, "Limiter Gain",        c.limGainDb,          0.0f,  18.0f) ? 1 : 0;
        applied += setParam (p, "Comp Stereo Link",    c.compLinkPercent,    0.0f, 100.0f) ? 1 : 0;
        applied += setParam (p, "Limiter Stereo Link", c.limLinkPercent,     0.0f, 100.0f) ? 1 : 0;
        r.paramsApplied = applied;
    }

    juce::AudioBuffer<float> buf (juce::jmax (2, juce::jmax (p.getTotalNumInputChannels(),
                                                             p.getTotalNumOutputChannels())),
                                  blockSize);
    juce::MidiBuffer midi;
    double sumSq[2] = { 0.0, 0.0 };

    // ~1.3 s: latency, the 20 ms control smoothers and the adaptive engine's
    // slow features are all well clear before the measured half begins.
    const int blocks = 120;
    for (int b = 0; b < blocks; ++b)
    {
        buf.clear();
        for (int n = 0; n < blockSize; ++n)
        {
            const double t = (double) (b * blockSize + n) / sr;
            // DIFFERENT frequencies per channel so a swap or a sum-into-one
            // cannot masquerade as health, at a level a master actually sits at.
            buf.setSample (0, n, 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * t));
            buf.setSample (1, n, 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 330.0 * t));
        }
        p.processBlock (buf, midi);

        if (b >= blocks / 2)
            for (int n = 0; n < blockSize; ++n)
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float v = buf.getSample (ch, n);
                    if (! std::isfinite (v)) r.nonFinite = true;
                    sumSq[ch] += (double) v * v;
                }
    }
    const double settled = (double) (blocks - blocks / 2) * (double) blockSize;
    r.rmsL = std::sqrt (sumSq[0] / settled);
    r.rmsR = std::sqrt (sumSq[1] / settled);
    p.releaseResources();
    return r;
}

} // namespace

// THE MACRO PATH, which nothing automated has ever run.
//
// `MacroEngine` maps the Loudness/Character/Tone knobs onto the nine managed
// parameters on a 30 ms `juce::Timer`. A headless console app runs no message
// loop, so that tick never fires: setting Loudness moves the knob and NOTHING
// downstream of it (round 6 found the whole channel battery had been vacuous
// for this reason). Every configuration above therefore sets the nine
// parameters DIRECTLY — correct for reaching the DSP, but it is not how a user
// operates the plugin, and it never exercises the mapping itself.
//
// This scenario turns the knob the way a user does and PUMPS THE MESSAGE LOOP
// between audio blocks so the timer actually runs, with the editor alive
// because that is the state the report describes (the owner is reading GR
// lanes). It asserts the premise — that the macro really did move a managed
// parameter — so it cannot quietly become another defaults run.
static int runMacroScenario (juce::AudioPluginFormatManager& fm, const juce::PluginDescription& desc,
                      double sr, int blockSize)
{
    juce::String err;
    auto owned = fm.createPluginInstance (desc, sr, blockSize, err);
    if (owned == nullptr)
    {
        std::printf ("PROBE ENVIRONMENT FAILURE: macro scenario could not instantiate: %s\n",
                     err.toRawUTF8());
        return 2;
    }
    auto& p = *owned;

    // THE EDITOR IS BEST-EFFORT, and the scenario is still valid without it.
    // MacroEngine's drain timer belongs to the PROCESSOR (`startDraining()` in
    // the constructor), not to the editor, so the mapping runs either way; the
    // editor is here only because it is the state the report describes. The
    // Windows CI runner cannot host a plugin editor at all (KI-007) and a Linux
    // runner needs a display, so a failure to create one is reported and
    // stepped over rather than allowed to fail the probe for a reason that has
    // nothing to do with channels.
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    if (p.hasEditor())
        editor.reset (p.createEditorAndMakeActive());
    if (editor != nullptr)
        editor->setVisible (true);
    else
        std::printf ("  (no editor in this environment -- the macro scenario runs without one; "
                     "the 30 ms mapping timer is the processor's and still fires)\n");

    p.setNonRealtime (false);
    p.prepareToPlay (sr, blockSize);

    // Turn ONLY the macro. Character/Tone stay put; Loudness is the one the
    // report's configuration is reached with.
    if (! setParam (p, "Loudness", 85.0f, 0.0f, 100.0f))
    {
        std::printf ("PROBE ENVIRONMENT FAILURE: macro scenario could not find 'Loudness'\n");
        return 2;
    }

    auto readManaged = [&p] (const char* name) -> float
    {
        for (int i = 0; i < p.getParameters().size(); ++i)
            if (auto* h = p.getHostedParameter (i))
                if (h->getName (64).trim() == juce::String (name))
                    return h->getValue();
        return -1.0f;
    };
    const float clipDriveBefore = readManaged ("Clip Drive");

    juce::AudioBuffer<float> buf (juce::jmax (2, juce::jmax (p.getTotalNumInputChannels(),
                                                             p.getTotalNumOutputChannels())),
                                  blockSize);
    juce::MidiBuffer midi;
    double sumSq[2] = { 0.0, 0.0 };
    bool nonFinite = false;
    const int blocks = 200;
    for (int b = 0; b < blocks; ++b)
    {
        // Let the 30 ms mapping timer run. Without this the whole scenario is
        // the defaults case under another name.
        juce::MessageManager::getInstance()->runDispatchLoopUntil (5);

        buf.clear();
        for (int n = 0; n < blockSize; ++n)
        {
            const double t = (double) (b * blockSize + n) / sr;
            buf.setSample (0, n, 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * t));
            buf.setSample (1, n, 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 330.0 * t));
        }
        p.processBlock (buf, midi);

        if (b >= blocks / 2)
            for (int n = 0; n < blockSize; ++n)
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float v = buf.getSample (ch, n);
                    if (! std::isfinite (v)) nonFinite = true;
                    sumSq[ch] += (double) v * v;
                }
    }
    const double settled = (double) (blocks - blocks / 2) * (double) blockSize;
    const double rmsL = std::sqrt (sumSq[0] / settled);
    const double rmsR = std::sqrt (sumSq[1] / settled);
    const float clipDriveAfter = readManaged ("Clip Drive");

    if (editor != nullptr)
    {
        p.editorBeingDeleted (editor.get());
        editor.reset();
    }
    p.releaseResources();

    const bool mapped = clipDriveAfter > clipDriveBefore + 1.0e-4f;
    const bool lost   = (rmsL < 1.0e-4 || rmsR < 1.0e-4);
    const bool skewed = ! lost && (rmsL > rmsR * 2.0 || rmsR > rmsL * 2.0);

    std::printf ("  [%s] %5.0f Hz / %4d  %-28s  L=%.9f  R=%.9f%s%s\n",
                 (! mapped || lost || skewed || nonFinite) ? "FAIL" : " ok ",
                 sr, blockSize, "macro: Loudness 85 + editor", rmsL, rmsR,
                 nonFinite ? "  NON-FINITE" : "",
                 lost ? "  CHANNEL LOST" : (skewed ? "  >6 dB APART" : ""));
    if (! mapped)
    {
        // A vacuous macro run is worse than no macro run: it looks like coverage.
        std::printf ("PROBE ENVIRONMENT FAILURE: the macro never mapped (Clip Drive %.6f -> "
                     "%.6f). The 30 ms MacroEngine timer did not fire, so this scenario was the "
                     "defaults case under another name.\n", clipDriveBefore, clipDriveAfter);
        return 2;
    }
    return (lost || skewed || nonFinite) ? 1 : 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc < 2)
    {
        std::printf ("usage: AnabasisChannelProbe <bundle> [--format vst3|au] [--verbose]\n");
        return 2;
    }
    const juce::String path (argv[1]);
    juce::String wanted ("vst3");
    for (int i = 2; i < argc; ++i)
        if (juce::String (argv[i]) == "--format" && i + 1 < argc)
            wanted = juce::String (argv[++i]).toLowerCase();

    juce::AudioPluginFormatManager fm;
    fm.addFormat (std::make_unique<juce::VST3PluginFormat>());
   #if JUCE_PLUGINHOST_AU && JUCE_MAC
    fm.addFormat (std::make_unique<juce::AudioUnitPluginFormat>());
   #endif

    juce::OwnedArray<juce::PluginDescription> found;
    for (auto* f : fm.getFormats())
        if (f->getName().toLowerCase().contains (wanted == "au" ? "audiounit" : "vst3"))
            f->findAllTypesForFile (found, path);

    if (found.isEmpty())
    {
        // Exit 2, NOT 1: "the host could not see the plugin" is an environment
        // problem (an unregistered AU, a wrong path) and must never be reported
        // as "the plugin loses a channel".
        std::printf ("PROBE ENVIRONMENT FAILURE: no %s plugin types found at %s\n",
                     wanted.toRawUTF8(), path.toRawUTF8());
        return 2;
    }

    juce::String err;
    auto instance = fm.createPluginInstance (*found[0], 48000.0, 512, err);
    if (instance == nullptr)
    {
        std::printf ("PROBE ENVIRONMENT FAILURE: could not instantiate: %s\n", err.toRawUTF8());
        return 2;
    }

    dumpParameterIds (*instance);
    std::printf ("AnabasisChannelProbe -- %s (%s), %d in / %d out\n",
                 found[0]->name.toRawUTF8(), wanted.toRawUTF8(),
                 instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels());
   #if JUCE_ARM
    std::printf ("  running slice: arm64\n");
   #elif JUCE_INTEL
    std::printf ("  running slice: x86_64\n");
   #endif

    int failures = 0;
    // Two rates and two block sizes: the field report is not tied to either, and
    // a per-channel state that only breaks at one buffer size would otherwise be
    // invisible. 512 @ 48k is the reference; 64 @ 44.1k stresses the smallest
    // block the chain's latency allowance has to survive.
    for (const auto sr : { 48000.0, 44100.0 })
        for (const auto blockSize : { 512, 64 })
            for (const auto& c : kConfigs)
            {
                const auto r = run (fm, *found[0], c, sr, blockSize);
                if (r.paramsApplied == -1)
                {
                    std::printf ("PROBE ENVIRONMENT FAILURE: could not instantiate for '%s'\n", c.name);
                    return 2;
                }
                if (r.paramsApplied != 7)
                {
                    // NOT a channel-loss failure: the probe could not put the
                    // plugin into the configuration it claims to be testing, so
                    // its result means nothing either way. Exit 2, loudly.
                    std::printf ("PROBE ENVIRONMENT FAILURE: only %d of 7 parameters applied for "
                                 "'%s' -- refusing to report a result for a configuration that "
                                 "was never set up\n", r.paramsApplied, c.name);
                    return 2;
                }
                const bool lost = (r.rmsL < 1.0e-4 || r.rmsR < 1.0e-4);
                const bool skewed = ! lost && (r.rmsL > r.rmsR * 2.0 || r.rmsR > r.rmsL * 2.0);
                const bool bad = lost || skewed || r.nonFinite;
                failures += bad ? 1 : 0;
                std::printf ("  [%s] %5.0f Hz / %4d  %-28s  L=%.9f  R=%.9f%s%s\n",
                             bad ? "FAIL" : " ok ", sr, blockSize, c.name, r.rmsL, r.rmsR,
                             r.nonFinite ? "  NON-FINITE" : "",
                             lost ? "  CHANNEL LOST" : (skewed ? "  >6 dB APART" : ""));
                std::fflush (stdout);
            }

    instance->releaseResources();
    instance.reset();

    // The macro scenario runs last: it is the only one that needs a message loop
    // and an editor, and keeping it separate means a failure names itself rather
    // than being one row among thirty-two.
    const int macroRc = runMacroScenario (fm, *found[0], 48000.0, 512);
    if (macroRc == 2)
        return 2;
    failures += macroRc;

    std::printf (failures == 0 ? "PROBE PASSED: every configuration kept both channels\n"
                               : "PROBE FAILED: %d configuration(s) lost or skewed a channel\n",
                 failures);
    return failures == 0 ? 0 : 1;
}
