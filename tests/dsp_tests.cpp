// ============================================================================
//  AnabasisTests — DSP acceptance suite (DSP_POLICY invariant→test map).
//
//  Compiles the AnabasisDSP INTERFACE sources DIRECTLY, with no wrapper and no
//  GUI: this target existing and linking IS the build-level format-agnosticism
//  test of DSP_POLICY invariant 13 / ADR-0001.
//
//  Harness per docs/procedures/TESTING.md: a check(cond, what) counter,
//  main() runs every test, exit non-zero on any failure. No framework.
// ============================================================================

#include <AnabasisEngine.h>
#include <GrHistoryBuffer.h>
#include <LoudnessMeter.h>
#include <RmsMeter.h>
#include <Latency.h>
#include <juce_dsp/juce_dsp.h>
#include "AllocationGuard.h"

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <type_traits>
#include <memory>

static int failures = 0;
static int checks   = 0;

static void check (bool condition, const char* what)
{
    ++checks;
    if (! condition)
    {
        ++failures;
        std::printf ("FAIL: %s\n", what);
    }
}

// ---------------------------------------------------------------------------
// inv 7: with defaults and no processing engaged, output is a bit-exact
// delay-aligned copy of the input — FOR ANY SUB-CEILING INPUT since 0.1.2
// (ADR-0023). This stimulus is deliberately hostile to the old curves: a
// 30 Hz square at 0.93 plus noise (peak ≤ 0.96, under the −0.1 dB ceiling's
// 0.989 linear) whose detector RMS sits near −0.6 dBFS. Under the pre-0.1.2
// code this drew real gain reduction two ways at factory defaults — the
// comp's centred knee computed gain from −3 dBFS up, and the §5.4 scHpf trim
// engaged the LIMITER's detector HPF, whose edge overshoot (≈ 2× on a −A→+A
// step) pushed detection over the ceiling on samples that never crossed it —
// which is exactly the 0.1.2 item-2 field report ("GR at the default preset
// on legal material"). With the knee above the threshold, the limiter
// detector unfiltered and the comp's filtered magnitude clamped to the raw
// one, every stage is inert below its engagement level and the null holds.
// The adaptive trims stay LIVE throughout — inertness is a property of the
// stages, not of the trims being zero, and the premise that makes that true
// for the one trim which could reach a stage (`dynTilt` → ClipSat's Dynamic
// Tame) is asserted in the body rather than assumed.
//
// The second pass asserts the same property through the DELTA monitor: at a
// bit-exact null the difference leg is exactly `delayedDry − processed = 0`
// once the delta fade lands on its exact endpoint, so Delta at defaults on
// sub-ceiling material is digital silence — the owner's item-2 observation
// ("Delta carries a GR-flavoured residue on the default preset") is pinned
// out by construction.
static void testNullWithDefaults()
{
    const double sr = 48000.0;
    const int block = 512, blocks = 40;

    auto stimulus = [] (uint32_t& rng, int t)
    {
        rng = rng * 1664525u + 1013904223u;                           // deterministic
        const float noise  = ((float) (rng >> 8) / 8388608.0f - 1.0f) * 0.03f;
        const float square = (std::sin (2.0f * juce::MathConstants<float>::pi
                                        * 30.0f * (float) t / 48000.0f) >= 0.0f)
                                 ? 0.93f : -0.93f;
        return square + noise;                                        // peak ≤ 0.96
    };

    {   // Pass 1: the render null, bit-exact.
        anabasis::AnabasisEngine engine;
        engine.prepare (sr, block, 2);
        const int delay = engine.groupDelaySamples();
        check (delay == anabasis::maxLookaheadSamples (sr),
               "null: engine group delay equals the constant allowance");

        anabasis::EngineParameters p;   // POD defaults == §4.2 defaults
        std::vector<float> inL, outL;
        juce::AudioBuffer<float> buf (2, block);
        uint32_t rng = 0x12345678u;

        for (int b = 0; b < blocks; ++b)
        {
            for (int n = 0; n < block; ++n)
            {
                const float v = stimulus (rng, b * block + n);
                buf.setSample (0, n, v);
                buf.setSample (1, n, -v);
                inL.push_back (v);
            }
            engine.process (buf, p);
            for (int n = 0; n < block; ++n)
                outL.push_back (buf.getSample (0, n));
        }

        // Self-enforcing stimulus constraints: sub-ceiling (the null's only
        // remaining engagement bound), and hot enough that the OLD centred
        // knee would have engaged — so this test fails against the pre-0.1.2
        // curve rather than merely not exercising it.
        float stimPeak = 0.0f;
        for (float v : inL) stimPeak = juce::jmax (stimPeak, std::abs (v));
        check (stimPeak < 0.9885f, "null: the stimulus stays below the -0.1 dB ceiling");
        check (stimPeak > 0.708f,
               "null: …and above the old centred knee's -3 dBFS bottom (the negative control)");

        // THE ADAPTATION PREMISE, asserted rather than assumed. The §5.4 trims
        // run LIVE here, and exactly ONE of them can reach a stage able to
        // break a bit-exact null: `dynTilt`, whose host is ClipSat's Dynamic
        // Tame. (The other three are inert while their stages are, which the
        // two zero-reduction assertions below establish directly.) That stage
        // is "EXACTLY idle (skipped) when nothing clips OR dynTilt is 0"
        // (`ClipSat.h` §3), and `clipOn` is an exact-zero test on the DRIVE —
        // so at the factory drive of 0 the tame is skipped WHATEVER the trim
        // maps, and the null does not rest on the trim's value at all.
        //
        // Asserting the DRIVE rather than the trim is the point: the trim
        // mapping is ⊕ listening material and must stay free to move, while
        // this default is the property the inertness argument actually uses.
        // Pin the trim instead and a legitimate re-tuning fails a null test
        // for a reason that is not a defect. If this default ever leaves 0 the
        // argument is gone, and this says WHICH premise died instead of
        // leaving a bit mismatch 40 blocks downstream to be bisected.
        // ClipSat's own `activityEnvelope()` invariant-7 tripwire — the same
        // argument's other half, "nothing clips" — is asserted at the stage
        // level by `testClipDynamicTame`.
        check (juce::exactlyEqual (p.clipDriveDb, 0.0f),
               "null: (premise) the factory drive is 0, so the Dynamic Tame — the only stage "
               "an adaptive trim can reach — is skipped whatever the trims map");

        bool exact = true;
        for (size_t n = (size_t) delay; n < outL.size(); ++n)
            if (! juce::exactlyEqual (outL[n], inL[n - (size_t) delay])) { exact = false; break; }
        check (exact, "null: output is a bit-exact copy delayed by the allowance");
        check (juce::exactlyEqual (engine.lastBlockMinGain(), 1.0f),
               "null: the limiter computed no reduction on sub-ceiling material");
        check (juce::exactlyEqual (engine.lastCompGrDb(), 0.0f),
               "null: the compressor computed no reduction at or below its 0 dBFS threshold");
    }

    {   // Pass 2: Delta at defaults is digital silence on the same material.
        anabasis::AnabasisEngine engine;
        engine.prepare (sr, block, 2);
        anabasis::EngineParameters p;
        p.deltaMonitor = true;
        juce::AudioBuffer<float> buf (2, block);
        uint32_t rng = 0x9e3779b9u;
        float maxAfterFade = 0.0f;
        for (int b = 0; b < blocks; ++b)
        {
            for (int n = 0; n < block; ++n)
            {
                const float v = stimulus (rng, b * block + n);
                buf.setSample (0, n, v);
                buf.setSample (1, n, -v);
            }
            engine.process (buf, p);
            if (b >= 4)   // past the ~10 ms delta fade and the delay pipeline
                for (int ch = 0; ch < 2; ++ch)
                    for (int n = 0; n < block; ++n)
                        maxAfterFade = juce::jmax (maxAfterFade,
                                                   std::abs (buf.getSample (ch, n)));
        }
        check (juce::exactlyEqual (maxAfterFade, 0.0f),
               "null: Delta at defaults on sub-ceiling material is exact digital silence");
    }
}

// ---------------------------------------------------------------------------
// ADR-0004 / inv 2 (P1 form of testReportedLatencyMatchesImpulse): the
// impulse lands at EXACTLY maxLookahead for every lookahead value, not just
// the range ends — the constant-allowance contract is what makes a padding
// bug a test failure.
static void testReportedLatencyMatchesImpulse()
{
    const double sr = 48000.0;
    for (const float lookMs : { 0.5f, 2.0f, 5.0f, 10.0f })
    {
        anabasis::AnabasisEngine engine;
        engine.prepare (sr, 512, 2);
        anabasis::EngineParameters p;
        p.lookaheadMs = lookMs;

        juce::AudioBuffer<float> buf (2, 512);
        int   peakAt  = -1;
        float peakVal = 0.0f;
        for (int b = 0; b < 4; ++b)
        {
            buf.clear();
            if (b == 0) { buf.setSample (0, 0, 0.5f); buf.setSample (1, 0, 0.5f); }
            engine.process (buf, p);
            for (int n = 0; n < 512; ++n)
                if (std::abs (buf.getSample (0, n)) > peakVal)
                { peakVal = std::abs (buf.getSample (0, n)); peakAt = b * 512 + n; }
        }
        check (peakAt == anabasis::maxLookaheadSamples (sr),
               "latency: impulse lands at the constant allowance for every lookahead");
        check (anabasis::predictLatencySamples (p, sr) == anabasis::maxLookaheadSamples (sr),
               "latency: predictor agrees with the allowance while OS is Off");
    }
}

// ---------------------------------------------------------------------------
// CODE_STYLE §Real-time discipline + DSP_POLICY invariant 8: every parameter
// reaching the DSP is smoothed. `ceiling` is host-automatable, so an
// automation lane exercises this on every session; taking it per block used to
// step the limiter's gain and the clamp together. The bound below is what a
// 20 ms glide allows per sample; a per-block step fails it by ~2 orders.
static void testCeilingIsSmoothed()
{
    anabasis::AnabasisEngine engine;
    const double sr = 48000.0;
    const int block = 256;
    engine.prepare (sr, block, 2);

    anabasis::EngineParameters p;
    p.ceilingDbTp = 0.0f;               // start wide open
    p.limGainDb   = 0.0f;

    juce::AudioBuffer<float> buf (2, block);
    auto runBlock = [&] (std::vector<float>& out)
    {
        for (int n = 0; n < block; ++n)
        {
            const float v = 0.9f;       // DC-ish: any gain change is visible directly
            buf.setSample (0, n, v);
            buf.setSample (1, n, v);
        }
        engine.process (buf, p);
        for (int n = 0; n < block; ++n)
            out.push_back (buf.getSample (0, n));
    };

    std::vector<float> out;
    for (int b = 0; b < 8; ++b) runBlock (out);      // settle at ceiling 0 dB
    const size_t stepAt = out.size();
    p.ceilingDbTp = -12.0f;                          // a big automation jump
    for (int b = 0; b < 8; ++b) runBlock (out);

    float maxDelta = 0.0f;
    for (size_t n = stepAt; n < out.size(); ++n)
        maxDelta = juce::jmax (maxDelta, std::abs (out[n] - out[n - 1]));

    // 0.9 -> ~0.25 over 20 ms at 48 kHz is < 0.001 per sample; a block-boundary
    // step would show the whole ~0.65 jump in one sample.
    check (maxDelta < 0.01f, "smoothing: a ceiling automation jump glides, never steps");
    check (out.back() < 0.30f, "smoothing: the ceiling change does arrive at its target");
}

// ---------------------------------------------------------------------------
// The first block after prepare() ADOPTS its control values instead of gliding
// from the constructor defaults — a ramp there is an artefact of construction,
// not a user move. Observable only with a ceiling far from the -1 dBTP default:
// unprimed, the first audible samples sit well above the target while the
// smoother is still travelling.
static void testControlsPrimedOnPrepare()
{
    anabasis::AnabasisEngine engine;
    const double sr = 48000.0;
    const int block = 256;
    engine.prepare (sr, block, 2);

    anabasis::EngineParameters p;
    p.ceilingDbTp = -12.0f;                 // 0.251 linear, far from the 0.989 default
    // This test pins the PRIMING of the engine's control smoothers, so the
    // detector must be memoryless and the attack instant: the true-peak
    // estimator honestly reports inter-sample overshoot at the 0→0.9 step
    // edge and the envelope then releases back at the configured rate, which
    // puts out[500] legitimately below the ceiling — correct limiting, wrong
    // measurement for THIS property.
    p.truePeakMode      = false;
    p.transientPreserve = 0.0f;
    const float target = std::pow (10.0f, p.ceilingDbTp / 20.0f);

    juce::AudioBuffer<float> buf (2, block);
    std::vector<float> out;
    for (int b = 0; b < 4; ++b)             // 1024 samples > the 480 delay
    {
        for (int n = 0; n < block; ++n) { buf.setSample (0, n, 0.9f); buf.setSample (1, n, 0.9f); }
        engine.process (buf, p);
        for (int n = 0; n < block; ++n) out.push_back (buf.getSample (0, n));
    }

    // Sample 500 is the first fully-emerged output; primed it is already at the
    // target ceiling, unprimed the smoother would still be gliding down to it.
    check (std::abs (out[500] - target) < 1.0e-3f,
           "priming: the ceiling is adopted on the first block, not glided into");
}

// ---------------------------------------------------------------------------
// The gain smoothers prime too, not just the ceiling and the window. An
// earlier revision primed only two of the four, so after loading a session
// with limGain at +18 dB the first 20 ms of audio played up to 18 dB low and
// slid up — audible at transport start and at the head of a bounce.
// The three quantities `prepare` takes from the host: `maxBlockSize` and
// `numChannels` have always been railed, the SAMPLE RATE was not. It is the one
// that reaches an allocation -- `wetRing` and `dryRingSize` both size from
// `delaySamples`, which is a ceil of a product with the rate, so a negative rate
// made it negative and `setSize` threw out of `prepareToPlay`, across the
// wrapper's C ABI where an exception is a crash rather than a diagnostic.
//
// UNREACHABLE FROM A CONFORMING HOST (VST3's `ProcessSetup::sampleRate` and AU's
// `Float64` are both specified positive), so this pins a rail, not a behaviour:
// the second half asserts the valid-rate arithmetic is byte-for-byte what it was.
static void testAnInvalidSampleRateCannotSizeABuffer()
{
    for (const double bad : { -48000.0, -1.0e6 })
        for (const int block : { 1, 512 })
        {
            anabasis::AnabasisEngine engine;
            bool threw = false;
            try                   { engine.prepare (bad, block, 2); }
            catch (const std::exception&) { threw = true; }
            check (! threw,
                   "sampleRateRail: a negative rate cannot reach an allocation as a negative length");

            juce::AudioBuffer<float> buf (2, block);
            anabasis::EngineParameters p;
            for (int n = 0; n < block; ++n) { buf.setSample (0, n, 0.25f); buf.setSample (1, n, 0.25f); }
            engine.process (buf, p);
            bool finite = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 0; n < block; ++n)
                    if (! std::isfinite (buf.getSample (ch, n))) finite = false;
            check (finite, "sampleRateRail: ...and the engine still renders finite audio after one");
        }

    // The rail is invisible to every rate a host can actually supply: 10 ms of
    // allowance, unchanged, at the rates the project ships against.
    check (anabasis::maxLookaheadSamples (44100.0) == 441
        && anabasis::maxLookaheadSamples (48000.0) == 480
        && anabasis::maxLookaheadSamples (96000.0) == 960
        && anabasis::maxLookaheadSamples (192000.0) == 1920,
           "sampleRateRail: the 10 ms allowance at valid rates is exactly what it always was");
}

static void testGainsPrimedOnPrepare()
{
    anabasis::AnabasisEngine engine;
    const double sr = 48000.0;
    const int block = 256;
    engine.prepare (sr, block, 2);

    anabasis::EngineParameters p;
    p.limGainDb = 18.0f;                     // as restored from a session
    const float in = 0.01f;                  // small: 18 dB of it stays far below the ceiling
    const float expected = in * juce::Decibels::decibelsToGain (18.0f);

    juce::AudioBuffer<float> buf (2, block);
    std::vector<float> out;
    for (int b = 0; b < 4; ++b)              // 1024 samples > the 480 delay
    {
        for (int n = 0; n < block; ++n) { buf.setSample (0, n, in); buf.setSample (1, n, in); }
        engine.process (buf, p);
        for (int n = 0; n < block; ++n) out.push_back (buf.getSample (0, n));
    }

    // Sample 500 carries input sample 20 — 0.4 ms in. Unprimed the gain would
    // still be ~1.1x there instead of 7.94x.
    check (std::abs (out[500] - expected) < 1.0e-4f,
           "priming: the gain smoothers adopt on the first block, no ramp from unity");
}

// ---------------------------------------------------------------------------
// invariant 8 names the lookahead as "the one switchable path with neither a
// duck nor a latch ... the path most likely to be skipped at P1", requiring
// its move to be a smooth control signal. The detector tap offset is
// delaySamples - W, so an unsmoothed W jumps the tap by hundreds of samples at
// one block boundary.
static void testLookaheadIsSmoothed()
{
    // Tests the PROPERTY, not a proxy. An earlier version of this test watched
    // the output for a discontinuity and passed against deliberately
    // unsmoothed code (verified by mutation): enlarging W cannot retroactively
    // add samples the wedge already dropped, and shrinking it only relaxes the
    // gain, which goes through the slow release — so an unsmoothed W steps the
    // detector tap without stepping the output. The smoothing is still
    // required (invariant 8: the lookahead move must be "a smooth,
    // band-limited control signal"), so the assertion is made where the
    // property actually lives: the engaged window the engine hands the
    // detector each sample.
    anabasis::AnabasisEngine engine;
    const double sr = 48000.0;
    const int block = 64;
    engine.prepare (sr, block, 2);

    anabasis::EngineParameters p;
    p.lookaheadMs = 0.5f;
    juce::AudioBuffer<float> buf (2, block);
    buf.clear();
    engine.process (buf, p);
    check (engine.engagedWindowSamples() == 24,
           "lookahead: the first block adopts its window without a glide");

    p.lookaheadMs = 10.0f;                       // 24 -> 480 samples
    engine.process (buf, p);
    const int afterOne = engine.engagedWindowSamples();
    check (afterOne > 24 && afterOne < 480,
           "lookahead: one block into a move the window is between the two values");

    int prev = afterOne, maxStep = 0;
    for (int b = 0; b < 40; ++b)                 // 40 * 64 = 2560 samples > 20 ms
    {
        engine.process (buf, p);
        const int now = engine.engagedWindowSamples();
        maxStep = juce::jmax (maxStep, std::abs (now - prev));
        prev = now;
    }
    check (prev == 480, "lookahead: the glide reaches the target window");
    check (maxStep <= block, "lookahead: no block moves the tap more than a block's worth");
}

// ---------------------------------------------------------------------------
// inv 4 (P1 sample-level form; the dBTP matrix arrives with TruePeak at P2):
// hot material pushed +12 dB never exceeds the ceiling after the clamp.
// SCOPE: the programme path only. The two monitor-only audition legs sit
// outside the invariant by design (DSP_POLICY inv 4's scope note): bypass
// carries the unclamped dry, and delta plays dry − processed, which can
// reach ~2× full scale on decorrelated material. Both are inert offline, so
// no RENDER can exceed the ceiling — this test deliberately runs with both
// monitor functions off.
static void testOutputNeverExceedsCeiling()
{
    // ADR-0002's mandated stimulus (docs/procedures/TESTING.md): BOTH EQ
    // positions, and the Post case with a +12 dB shelf AFTER the limiter —
    // the exact signal the clamp placement exists to survive. A clamp wired
    // upstream of the post EQ passes the Pre case and fails the Post one.
    const double sr = 48000.0;
    for (const int eqPos : { 0, 1 })
    {
        anabasis::AnabasisEngine engine;
        engine.prepare (sr, 512, 2);
        anabasis::EngineParameters p;
        p.limGainDb          = 12.0f;
        p.eqPosition         = eqPos;
        p.eqHighShelfGainDb  = 12.0f;    // in Post position this boosts the LIMITED signal
        p.eqHighShelfFreqHz  = 1000.0f;  // low corner so the 5 kHz probe sits in the boost
        // BOTH PINNED, not inherited. This test is about the CLAMP — that it
        // is downstream of the Post EQ and holds under a +12 dB shelf — not
        // about whatever the product currently ships as its default. Left
        // inheriting, a default move silently re-calibrates the guard: the
        // ceiling going −1 → −0.1 (ADR-0015) raises the bar the clamp has to
        // hold and makes both assertions easier, and `truePeakMode` going
        // on → off changes which mechanism is under test. −1 dB is the value
        // this stimulus was written against, and FALSE is the harder case for
        // the sample-level backstop: with true-peak mode on, the TP-driven
        // gain keeps the signal further from the clamp, so the clamp itself
        // does less of the work this test exists to prove it does.
        p.ceilingDbTp        = -1.0f;
        p.truePeakMode       = false;
        const float ceilingLin = std::pow (10.0f, p.ceilingDbTp / 20.0f);

        juce::AudioBuffer<float> buf (2, 512);
        float maxOut = 0.0f;
        for (int b = 0; b < 100; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                // Two components so both shelf band and low band are hot.
                const double t = (b * 512 + n) / sr;
                const float v = 0.6f * (float) std::sin (2.0 * juce::MathConstants<double>::pi *   97.0 * t)
                              + 0.6f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 5000.0 * t);
                buf.setSample (0, n, v);
                buf.setSample (1, n, v);
            }
            engine.process (buf, p);
            maxOut = juce::jmax (maxOut, buf.getMagnitude (0, 512));
        }
        check (maxOut <= ceilingLin * 1.0001f,
               eqPos == 0 ? "ceiling: never exceeded with Pre EQ"
                          : "ceiling: never exceeded with a +12 dB shelf AFTER the limiter (Post)");
        check (maxOut > 0.5f * ceilingLin, "ceiling: the limiter is actually engaged");
    }
}

// ---------------------------------------------------------------------------
// §2.2 frequency response, measured — a formula transcription error in the
// RBJ tables shows up here as a wrong magnitude, not as a crash. Sine RMS in
// the settled tail vs the known input RMS.
static float eqResponseDb (anabasis::MasteringEQ& eq, const anabasis::EngineParameters& p,
                           double sr, float freqHz)
{
    eq.prepare (sr);          // fresh state per probe; setTargets primes (adopts)
    eq.setTargets (p);
    const int total = (int) sr;             // 1 s
    const int tail  = total / 2;
    double sumSq = 0.0;
    for (int n = 0; n < total; ++n)
    {
        eq.tick();
        const float x = std::sin (2.0f * juce::MathConstants<float>::pi * freqHz * (float) n / (float) sr);
        const float y = eq.processSample (0, x);
        if (n >= total - tail)
            sumSq += (double) y * y;
    }
    const double rms = std::sqrt (sumSq / tail);
    return (float) (20.0 * std::log10 (rms / 0.7071067811865476));
}

static void testEqFrequencyResponse()
{
    const double sr = 48000.0;
    anabasis::MasteringEQ eq;
    auto near = [] (float a, float b, float tol) { return std::abs (a - b) <= tol; };

    {   // Bell: +6 dB at 1 kHz, Q 2 — peak at centre, flat far away
        anabasis::EngineParameters p;
        p.eqBell1FreqHz = 1000.0f; p.eqBell1GainDb = 6.0f; p.eqBell1Q = 2.0f;
        check (near (eqResponseDb (eq, p, sr, 1000.0f), 6.0f, 0.2f), "eq: bell gain lands at its centre");
        check (near (eqResponseDb (eq, p, sr,  100.0f), 0.0f, 0.2f), "eq: bell is flat two decades below");
        check (near (eqResponseDb (eq, p, sr, 10000.0f), 0.0f, 0.3f), "eq: bell is flat a decade above");
    }
    {   // Low shelf: −6 dB at 100 Hz — full cut deep below, flat far above
        anabasis::EngineParameters p;
        p.eqLowShelfFreqHz = 100.0f; p.eqLowShelfGainDb = -6.0f;
        check (near (eqResponseDb (eq, p, sr,   20.0f), -6.0f, 0.4f), "eq: low shelf reaches its gain below the corner");
        check (near (eqResponseDb (eq, p, sr, 5000.0f),  0.0f, 0.2f), "eq: low shelf is flat far above the corner");
    }
    {   // High shelf: +6 dB at 8 kHz
        anabasis::EngineParameters p;
        p.eqHighShelfFreqHz = 8000.0f; p.eqHighShelfGainDb = 6.0f;
        check (near (eqResponseDb (eq, p, sr, 18000.0f), 6.0f, 0.4f), "eq: high shelf reaches its gain above the corner");
        check (near (eqResponseDb (eq, p, sr,   200.0f), 0.0f, 0.2f), "eq: high shelf is flat far below the corner");
    }
    {   // Tilt +3: −3 dB deep low, +3 dB high, ~0 at the 700 Hz pivot.
        // SIGN IS THE CONTRACT here: positive tilt BRIGHTENS.
        anabasis::EngineParameters p;
        p.eqTiltDb = 3.0f;
        check (near (eqResponseDb (eq, p, sr,    30.0f), -3.0f, 0.5f), "eq: +tilt cuts the lows by the tilt amount");
        check (near (eqResponseDb (eq, p, sr, 16000.0f),  3.0f, 0.5f), "eq: +tilt boosts the highs by the tilt amount");
        check (near (eqResponseDb (eq, p, sr,   700.0f),  0.0f, 0.4f), "eq: tilt is ~flat at the 700 Hz pivot");
    }
}

// ---------------------------------------------------------------------------
// CODE_STYLE §Real-time discipline for the EQ's own smoothers: a 12 dB shelf
// jump glides over 20 ms instead of stepping the output in one sample. DC
// through a low shelf receives the full shelf gain, so the gain trajectory is
// read directly off the output.
static void testEqGainIsSmoothed()
{
    const double sr = 48000.0;
    anabasis::MasteringEQ eq;
    eq.prepare (sr);

    anabasis::EngineParameters p;      // flat
    eq.setTargets (p);                 // primes at transparent

    float last = 0.0f, maxDelta = 0.0f, out = 0.0f;
    for (int n = 0; n < 4800; ++n)     // 100 ms: jump at 10 ms, settle after
    {
        if (n == 480)
        {
            p.eqLowShelfGainDb = 12.0f;
            p.eqLowShelfFreqHz = 500.0f;
            eq.setTargets (p);         // primed: this must GLIDE
        }
        eq.tick();
        out = eq.processSample (0, 0.5f);   // DC probe
        if (n > 480)
            maxDelta = juce::jmax (maxDelta, std::abs (out - last));
        last = out;
    }
    // 0.5 → ~2.0 over 20 ms at 48 kHz is ~0.002/sample; an unsmoothed jump
    // puts most of the 1.5 step into one sample.
    check (maxDelta < 0.02f, "eq: a shelf-gain jump glides, never steps");
    check (out > 1.8f,       "eq: the glide does arrive at the target gain");
}

// ---------------------------------------------------------------------------
// Pre and Post are genuinely different circuits: a low shelf BEFORE the
// limiter drives it into deeper gain reduction; the same shelf AFTER the
// limiter boosts an already-limited signal into the clamp. If the position
// switch does not rewire, these outputs coincide.
static void testEqPositionsAreDistinct()
{
    const double sr = 48000.0;
    auto renderRms = [&] (int eqPos)
    {
        anabasis::AnabasisEngine engine;
        engine.prepare (sr, 512, 2);
        anabasis::EngineParameters p;
        p.limGainDb         = 6.0f;
        p.eqPosition        = eqPos;
        p.eqLowShelfGainDb  = 12.0f;
        p.eqLowShelfFreqHz  = 400.0f;
        juce::AudioBuffer<float> buf (2, 512);
        double sumSq = 0.0; int counted = 0;
        for (int b = 0; b < 40; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                const float v = 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                 * 100.0f * (float) (b * 512 + n) / (float) sr);
                buf.setSample (0, n, v); buf.setSample (1, n, v);
            }
            engine.process (buf, p);
            if (b >= 20)
                for (int n = 0; n < 512; ++n)
                { const double s = buf.getSample (0, n); sumSq += s * s; ++counted; }
        }
        return std::sqrt (sumSq / counted);
    };

    const double pre = renderRms (0), post = renderRms (1);
    check (std::abs (pre - post) > 0.01, "eq: Pre and Post positions produce distinct output");
}

// ---------------------------------------------------------------------------
// §2.3 static curve, measured in the RMS-detector mode where a sine's level
// is deterministic (RMS = peak − 3.01 dB; the 10 ms integrator barely ripples
// at 1 kHz). A transcription error in the knee quadratic or the ratio term is
// a wrong output level here, not a crash.
static float compOutputRmsDb (const anabasis::EngineParameters& p, float amp, double sr,
                              anabasis::MasteringComp* grTapOut = nullptr)
{
    anabasis::MasteringComp comp;
    comp.prepare (sr);
    comp.setPerBlock (p);
    const int total = (int) sr;          // 1 s, measure the settled second half
    double sumSq = 0.0; int counted = 0;
    for (int n = 0; n < total; ++n)
    {
        const float v = amp * std::sin (2.0f * juce::MathConstants<float>::pi
                                        * 1000.0f * (float) n / (float) sr);
        float frame[2] = { v, v };
        comp.processSample (frame, 2);
        if (n >= total / 2) { sumSq += (double) frame[0] * frame[0]; ++counted; }
    }
    juce::ignoreUnused (grTapOut);
    return (float) (20.0 * std::log10 (std::sqrt (sumSq / counted)));
}

static void testCompStaticCurve()
{
    const double sr = 48000.0;
    auto near = [] (float a, float b, float tol) { return std::abs (a - b) <= tol; };

    anabasis::EngineParameters p;
    p.compThresholdDb = -20.0f;
    p.compRatio       = 4.0f;
    p.compKneeDb      = 0.0f;            // hard knee: the pure ratio line
    p.compAttackMs    = 5.0f;
    p.compAutoRelease = false;
    p.compReleaseMs   = 50.0f;
    p.compDetector    = 0;               // RMS
    p.compMix         = 100.0f;          // POD carries 0..1 — set below

    // The POD stores mix 0..1 (toEngine divides); tests build the POD directly.
    p.compMix = 1.0f;

    {   // 7 dB over threshold at 4:1 → 5.25 dB of reduction
        const float inDb  = -13.01f;                    // amp 0.3162 → RMS −13.01
        const float outDb = compOutputRmsDb (p, 0.31623f, sr);
        check (near (outDb, inDb + (7.0f * (0.25f - 1.0f)), 0.5f),
               "comp: hard-knee ratio line lands where the static curve says");
    }
    {   // The knee sits ABOVE the threshold since 0.1.2 (ADR-0023): at
        // exactly T the curve computes ZERO (the old centred knee put
        // −1.125 dB here — the item-2 "GR below the threshold" defect), and
        // the quadratic applies inside (T, T+W]: at T + W/2 = −14 dB it is
        // GR = (invR−1)·(W/2)²/2W = −1.125 dB — the same figure, moved to
        // where the definition says it belongs.
        p.compKneeDb = 12.0f;
        const float atT = compOutputRmsDb (p, 0.14142f, sr);     // RMS −20.00 = T
        check (near (atT, -20.0f, 0.1f),
               "comp: zero reduction at exactly the threshold (the knee is above it)");
        const float atMid = compOutputRmsDb (p, 0.28184f, sr);   // RMS −14.00 = T + W/2
        check (near (atMid, -14.0f - 1.125f, 0.4f),
               "comp: soft knee applies the quadratic reduction at threshold + W/2");
        p.compKneeDb = 0.0f;
    }
    {   // Below the knee bottom: BIT-EXACT identity (the null path)
        anabasis::MasteringComp comp;
        comp.prepare (sr);
        anabasis::EngineParameters q;
        q.compThresholdDb = -20.0f; q.compKneeDb = 4.0f; q.compMix = 1.0f;
        comp.setPerBlock (q);
        bool exact = true;
        for (int n = 0; n < 48000; ++n)
        {
            const float v = 0.02f * std::sin (0.13f * (float) n);   // ~−34 dB
            float frame[2] = { v, -v };
            comp.processSample (frame, 2);
            if (! juce::exactlyEqual (frame[0], v) || ! juce::exactlyEqual (frame[1], -v))
            { exact = false; break; }
        }
        check (exact, "comp: below the knee the sample passes through bit-exact");
    }
}

// ---------------------------------------------------------------------------
static void testCompDetectorAndMix()
{
    const double sr = 48000.0;
    anabasis::EngineParameters p;
    p.compThresholdDb = -20.0f; p.compRatio = 4.0f; p.compKneeDb = 0.0f;
    p.compAttackMs = 5.0f; p.compAutoRelease = false; p.compReleaseMs = 50.0f;
    p.compMix = 1.0f;

    // Peak reads a sine ~3 dB hotter than RMS → visibly deeper reduction.
    p.compDetector = 0; const float rmsOut  = compOutputRmsDb (p, 0.31623f, sr);
    p.compDetector = 1; const float peakOut = compOutputRmsDb (p, 0.31623f, sr);
    check (peakOut < rmsOut - 1.0f, "comp: Peak detector reduces harder than RMS on a sine");

    // Parallel mix: 50 % sits between dry and wet; 0 % is bit-exact dry.
    p.compDetector = 0;
    p.compMix = 0.5f; const float halfOut = compOutputRmsDb (p, 0.31623f, sr);
    check (halfOut > rmsOut + 0.5f && halfOut < -13.01f - 0.5f,
           "comp: 50% mix lands between wet and dry");

    {
        anabasis::MasteringComp comp;
        comp.prepare (sr);
        p.compMix = 0.0f;
        comp.setPerBlock (p);
        bool exact = true;
        for (int n = 0; n < 24000; ++n)
        {
            const float v = 0.5f * std::sin (0.1309f * (float) n);   // loud: GR engaged
            float frame[2] = { v, v };
            comp.processSample (frame, 2);
            if (! juce::exactlyEqual (frame[0], v)) { exact = false; break; }
        }
        check (exact, "comp: 0% mix is bit-exact dry even under heavy reduction");
    }
}

// ---------------------------------------------------------------------------
// ADR-0019 (0.1.1): the comp's stereo link is ADJUSTABLE — the limiter's
// blend at the same point, linked = link·max(all) + (1−link)·own, before the
// integrator. Stimulus: a loud sine on ch 0 (deep over threshold), a quiet
// one on ch 1 (below the knee bottom). Full link must drag the quiet channel
// down with the loud one (one shared gain — the pre-0.1.1 glue); zero link
// must leave the quiet channel BIT-EXACT (its own detector never reaches the
// knee, and the gain-1 path multiplies by exactly 1.0f); half link sits
// between. A mutant that blends after the RMS integrator, or that keeps the
// single shared envelope, fails the zero-link half.
static void testCompStereoLink()
{
    const double sr = 48000.0;
    anabasis::EngineParameters p;
    p.compThresholdDb = -20.0f; p.compRatio = 4.0f; p.compKneeDb = 0.0f;
    p.compAttackMs = 5.0f; p.compAutoRelease = false; p.compReleaseMs = 50.0f;
    p.compMix = 1.0f; p.compDetector = 0;

    auto quietOutDb = [&] (float link) -> float
    {
        p.compStereoLink = link;
        anabasis::MasteringComp comp;
        comp.prepare (sr);
        comp.setPerBlock (p);
        const int total = (int) sr;
        double sumSq = 0.0; int counted = 0;
        for (int n = 0; n < total; ++n)
        {
            const float ph = 2.0f * juce::MathConstants<float>::pi
                           * 1000.0f * (float) n / (float) sr;
            float frame[2] = { 0.31623f * std::sin (ph),      // −13 dB RMS: ~7 dB over
                               0.02f    * std::sin (ph) };    // −37 dB RMS: far below
            comp.processSample (frame, 2);
            if (n >= total / 2) { sumSq += (double) frame[1] * frame[1]; ++counted; }
        }
        return (float) (20.0 * std::log10 (std::sqrt (sumSq / counted)));
    };

    const float linked   = quietOutDb (1.0f);
    const float unlinked = quietOutDb (0.0f);
    const float half     = quietOutDb (0.5f);
    check (linked < unlinked - 2.0f,
           "compLink: full link drags the quiet channel down with the loud one");
    check (half < unlinked - 0.5f && half > linked + 0.5f,
           "compLink: half link sits between full and none");

    {   // zero link: the below-knee channel is BIT-EXACT while the other compresses
        p.compStereoLink = 0.0f;
        anabasis::MasteringComp comp;
        comp.prepare (sr);
        comp.setPerBlock (p);
        bool exact = true;
        for (int n = 0; n < 24000; ++n)
        {
            const float ph = 0.1309f * (float) n;
            const float q  = 0.02f * std::sin (ph);
            float frame[2] = { 0.5f * std::sin (ph), q };
            comp.processSample (frame, 2);
            if (! juce::exactlyEqual (frame[1], q)) { exact = false; break; }
        }
        check (exact, "compLink: at zero link a below-knee channel passes bit-exact "
                      "while the other channel is deep in reduction");
    }
}

// ---------------------------------------------------------------------------
// The §2.3 auto release is TWO-STAGE: after a burst ends, the fast pole gives
// back most of its half quickly, the slow pole holds its half — so recovery
// in the first 100 ms strictly exceeds recovery in the following 100 ms, and
// attack stays fast (near-target within 20 ms). A single-pole mutant with
// either constant alone fails one half or the other.
static void testCompAutoReleaseIsTwoStage()
{
    const double sr = 48000.0;
    anabasis::MasteringComp comp;
    comp.prepare (sr);
    anabasis::EngineParameters p;
    p.compThresholdDb = -30.0f; p.compRatio = 4.0f; p.compKneeDb = 0.0f;
    p.compAttackMs = 5.0f; p.compAutoRelease = true; p.compDetector = 1;   // peak
    p.compMix = 1.0f;
    comp.setPerBlock (p);

    auto run = [&] (float amp, int samples)
    {
        for (int n = 0; n < samples; ++n)
        {
            const float v = amp * std::sin (2.0f * juce::MathConstants<float>::pi
                                            * 1000.0f * (float) n / (float) sr);
            float frame[2] = { v, v };
            comp.processSample (frame, 2);
        }
    };

    run (0.5f, (int) (0.5 * sr));                     // loud: −6 dBFS, 24 dB over
    const float grHeld = comp.currentGainReductionDb();
    check (grHeld < -10.0f, "comp: (premise) the burst drives deep reduction");

    run (0.5f, (int) (0.020 * sr));
    check (std::abs (comp.currentGainReductionDb() - grHeld) < 1.0f,
           "comp: reduction is stable while the burst continues");

    // The two bounds below are DISJOINT for any single release pole: the
    // deceleration ratio (rec2 < 0.6·rec1 ⇒ e^(−100 ms/τ) < 0.6) needs
    // τ < 196 ms, while the 800 ms tail-hold needs τ > 322 ms. Only a real
    // two-stage release satisfies both — an earlier revision's 200 ms/−0.5 dB
    // tail bar let a single ~150 ms pole pass, found by mutation.
    run (0.0001f, (int) (0.100 * sr));                // burst ends
    const float gr100 = comp.currentGainReductionDb();
    run (0.0001f, (int) (0.100 * sr));
    const float gr200 = comp.currentGainReductionDb();
    run (0.0001f, (int) (0.600 * sr));
    const float gr800 = comp.currentGainReductionDb();

    const float rec1 = gr100 - grHeld;                // both positive quantities
    const float rec2 = gr200 - gr100;
    check (rec1 > 2.0f,        "comp: the fast stage gives real recovery in the first 100 ms");
    check (rec2 < rec1 * 0.6f, "comp: recovery decelerates (kills every slow single pole)");
    check (gr800 < -1.5f,      "comp: the slow stage still holds after 800 ms (kills every fast single pole)");
}

// ---------------------------------------------------------------------------
// Brief §3: the sidechain HPF exists so LF content does not pump the glue.
// A 30 Hz tone 20 dB over threshold must compress with the HPF at its floor
// and go nearly untouched with the HPF at 300 Hz.
static void testCompSidechainHpf()
{
    const double sr = 48000.0;
    auto grAfter = [&] (float hpfHz)
    {
        anabasis::MasteringComp comp;
        comp.prepare (sr);
        anabasis::EngineParameters p;
        p.compThresholdDb = -30.0f; p.compRatio = 4.0f; p.compKneeDb = 0.0f;
        p.compAttackMs = 5.0f; p.compAutoRelease = false; p.compReleaseMs = 200.0f;
        p.compDetector = 1; p.compMix = 1.0f; p.scHpfFreqHz = hpfHz;
        comp.setPerBlock (p);
        for (int n = 0; n < 48000; ++n)
        {
            const float v = 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi
                                             * 30.0f * (float) n / (float) sr);
            float frame[2] = { v, v };
            comp.processSample (frame, 2);
        }
        return comp.currentGainReductionDb();
    };

    check (grAfter (20.0f)  < -8.0f, "scHpf: at the floor, a loud 30 Hz tone compresses hard");
    check (grAfter (300.0f) > -1.5f, "scHpf: at 300 Hz the same tone barely registers");
}

// ---------------------------------------------------------------------------
// §2.4 clipper: drive at exactly 0 dB is bit-identity (the driveTanh-precedent
// contract) no matter where shape/balance/tone sit, because the sub-block is
// SKIPPED — ADAA is inherently a half-sample smear even in its linear region,
// so "0 dB drive ≈ transparent" would be false; only the skip makes it exact.
static void testClipDriveZeroIsBitExact()
{
    anabasis::ClipSat clip;
    clip.prepare (48000.0);
    anabasis::EngineParameters p;
    p.clipDriveDb = 0.0f; p.clipShape = 1.0f; p.colourBalance = -0.7f;
    p.colourTone = 0.9f;  p.colourDepth = 0.0f; p.dynTiltDb = 0.0f; p.clipMix = 1.0f;
    clip.setPerBlock (p);

    bool exact = true;
    for (int n = 0; n < 48000; ++n)
    {
        const float v = 0.8f * std::sin (0.29f * (float) n);
        float frame[2] = { v, -v };
        clip.processSample (frame, 2);
        if (! juce::exactlyEqual (frame[0], v) || ! juce::exactlyEqual (frame[1], -v))
        { exact = false; break; }
    }
    check (exact, "clip: 0 dB drive is bit-identity with every other control wild");
}

// ---------------------------------------------------------------------------
// Level compensation and the knee morph. y = f(x·g)/g: the linear-region body
// passes at unity, the flat top sits at 1/g; at a driven peak of exactly 1.0
// the hard curve does not touch it while the w=1 knee already shapes it to
// f(1) = 0.75.
static void testClipCurveAndCompensation()
{
    const double sr = 48000.0;
    auto peakOut = [&] (float amp, float driveDb, float shape)
    {
        anabasis::ClipSat clip;
        clip.prepare (sr);
        anabasis::EngineParameters p;
        p.clipDriveDb = driveDb; p.clipShape = shape; p.clipMix = 1.0f;
        clip.setPerBlock (p);
        float peak = 0.0f;
        for (int n = 0; n < 24000; ++n)
        {
            const float v = amp * std::sin (2.0f * juce::MathConstants<float>::pi
                                            * 200.0f * (float) n / (float) sr);
            float frame[2] = { v, v };
            clip.processSample (frame, 2);
            if (n > 12000) peak = juce::jmax (peak, std::abs (frame[0]));
        }
        return peak;
    };

    const float g12 = std::pow (10.0f, 12.0f / 20.0f);
    const float flat = peakOut (0.9f, 12.0f, 0.0f);
    check (std::abs (flat - 1.0f / g12) < 0.01f,      // measured 0.25119 vs 0.25119
           "clip: hard-driven peak flattens at ceiling/drive (level compensation)");

    const float small = peakOut (0.05f, 12.0f, 0.0f);
    check (std::abs (small - 0.05f) < 2.0e-3f,        // measured 0.04999
           "clip: the linear-region body passes at unity gain under drive");

    const float hardAt1 = peakOut (1.0f, 0.001f, 0.0f);   // u peak ~1: hard leaves it
    const float softAt1 = peakOut (1.0f, 0.001f, 1.0f);   // w=1 knee: f(1)=0.75
    check (softAt1 < hardAt1 * 0.85f,                 // measured 0.7499 vs 0.9998
           "clip: the soft knee engages below the hard threshold (shape morph is real)");
}

// ---------------------------------------------------------------------------
// inv 6: ADAA measurably reduces aliasing versus the memoryless curve, with
// the naive reference computed from the SAME public transfer() the DSP uses.
// STIMULUS CALIBRATION MATTERS: ADAA-1's suppression is ~|sinc(pi*f_src/fs)|
// of the SOURCE harmonic, so a 5 kHz tone's folded 5th (source 25 kHz) only
// improves by the measured 4.8 dB — a 6 dB assertion there fails on correct
// code. A bright tone is both the honest use case and the strong measurement:
// at f0 = 11.72 kHz (bin 2000 of 8192 @ 48 kHz) the folded 3rd (source
// 35.2 kHz -> bin 2192) and 5th (source 58.6 kHz -> bin 1808) fold deep.
// Measured on this stimulus: 14.8 dB and 10.4 dB — asserted at 6/8 dB so a
// real regression fails while libm-level float variance cannot.
static void testClipAdaaReducesAliasing()
{
    const int N = 8192, warm = 256, k = 2000;
    const double sr = 48000.0;
    const float g = std::pow (10.0f, 12.0f / 20.0f);

    anabasis::ClipSat clip;
    clip.prepare (sr);
    anabasis::EngineParameters p;
    p.clipDriveDb = 12.0f; p.clipShape = 0.0f; p.clipMix = 1.0f;
    clip.setPerBlock (p);

    std::vector<float> adaa (N), naive (N);
    for (int n = 0; n < warm + N; ++n)
    {
        const float x = 0.9f * std::sin (2.0f * juce::MathConstants<float>::pi
                                         * (float) k * (float) n / (float) N);
        float frame[2] = { x, x };
        clip.processSample (frame, 2);
        if (n >= warm)
        {
            adaa[(size_t) (n - warm)]  = frame[0];
            naive[(size_t) (n - warm)] = anabasis::ClipSat::transfer (x * g, 0.0f) / g;
        }
    }

    juce::dsp::FFT fft (13);
    auto magDb = [&] (const std::vector<float>& sig, int bin)
    {
        std::vector<float> buf (2 * (size_t) N, 0.0f);
        std::copy (sig.begin(), sig.end(), buf.begin());
        fft.performRealOnlyForwardTransform (buf.data(), true);
        const float re = buf[(size_t) (2 * bin)], im = buf[(size_t) (2 * bin + 1)];
        return 20.0f * std::log10 (juce::jmax (1.0e-12f, std::sqrt (re * re + im * im)));
    };

    const int h3AliasBin = N - 3 * k;    // 35.16 kHz folds to bin 2192
    const int h5AliasBin = 5 * k - N;    // 58.59 kHz folds to bin 1808
    const float a3n = magDb (naive, h3AliasBin), a3a = magDb (adaa, h3AliasBin);
    const float a5n = magDb (naive, h5AliasBin), a5a = magDb (adaa, h5AliasBin);
    const float f0n = magDb (naive, k),          f0a = magDb (adaa, k);

    check (a3a < a3n - 6.0f,  "adaa: the folded 3rd harmonic drops by >6 dB");
    check (a5a < a5n - 8.0f,  "adaa: the folded 5th harmonic drops by >8 dB");
    check (std::abs (f0a - f0n) < 2.0f,
           "adaa: the fundamental survives (reduction is aliasing, not treble)");
}

// ---------------------------------------------------------------------------
// A realtime→offline flip is a RESET-class event, not an audible transition.
// With Force Max the flip changes effectiveFactor, so the engine wants a
// rewire — and the §2.8 duck would write ~45 ms of fade into the HEAD OF THE
// BOUNCE if the host does not re-prepare first (many do; the contract must not
// depend on it). The flip therefore adopts directly, exactly like the first
// block after prepare. Measured: the render is back at full amplitude 1500
// samples after the flip (the pipeline refill is ~547 samples and the host
// re-reads PDC across the flip, so that part is honest latency); with the duck
// it is still inside the held silent bottom there.
static void testOfflineFlipDoesNotDuckTheRender()
{
    const double sr = 48000.0;
    anabasis::AnabasisEngine engine;
    engine.prepare (sr, 512, 2);
    anabasis::EngineParameters p;
    p.truePeakMode    = false;
    p.oversample      = anabasis::OversampleFactor::x2;
    p.forceMaxOffline = true;          // offline forces 16x → wantIdx changes
    std::vector<float> out;
    juce::AudioBuffer<float> buf (2, 512);
    for (int b = 0; b < 20; ++b)
    {
        p.nonRealtime = b >= 10;       // the flip, deliberately with NO re-prepare
        for (int n = 0; n < 512; ++n)
        {
            const float v = 0.4f * std::sin (2.0f * juce::MathConstants<float>::pi
                                             * 300.0f * (float) (b * 512 + n) / (float) sr);
            buf.setSample (0, n, v); buf.setSample (1, n, v);
        }
        engine.process (buf, p);
        for (int n = 0; n < 512; ++n) out.push_back (buf.getSample (0, n));
    }

    auto peakOver = [&] (size_t from, size_t count)
    {
        float pk = 0.0f;
        for (size_t k = from; k < juce::jmin (from + count, out.size()); ++k)
            pk = juce::jmax (pk, std::abs (out[k]));
        return pk;
    };
    const float before = peakOver (9 * 512, 512);          // steady, pre-flip
    const float after  = peakOver (10 * 512 + 1500, 512);  // past the refill

    check (before > 0.3f, "offlineFlip: the pre-flip render is at full amplitude (baseline)");
    check (after > 0.9f * before,
           "offlineFlip: the render is not ducked across a realtime→offline flip");
}

// ---------------------------------------------------------------------------
// ...and the RETURN edge is the opposite case: offline→realtime lands in LIVE
// playback, where the direct adopt would clear the lookahead ring at full gain
// (~11 ms of silence, then an abrupt resumption) — the click invariant 8 names
// for an oversampling-factor switch. That edge must go through the §2.8 duck
// like any other rewire, so the transition is a fade, never a step.
static void testReturnFromOfflineIsDucked()
{
    const double sr = 48000.0;
    anabasis::AnabasisEngine engine;
    engine.prepare (sr, 512, 2);
    anabasis::EngineParameters p;
    p.truePeakMode    = false;
    p.oversample      = anabasis::OversampleFactor::x2;
    p.forceMaxOffline = true;
    std::vector<float> out;
    juce::AudioBuffer<float> buf (2, 512);
    for (int b = 0; b < 40; ++b)
    {
        p.nonRealtime = b >= 10 && b < 20;      // offline for ten blocks, then back
        for (int n = 0; n < 512; ++n)
        {
            const float v = 0.4f * std::sin (2.0f * juce::MathConstants<float>::pi
                                             * 300.0f * (float) (b * 512 + n) / (float) sr);
            buf.setSample (0, n, v); buf.setSample (1, n, v);
        }
        engine.process (buf, p);
        for (int n = 0; n < 512; ++n) out.push_back (buf.getSample (0, n));
    }

    // The return edge is at block 20. A 300 Hz sine at the post-limiter level
    // moves at most ~0.04 per sample; a resumption from exact silence at full
    // gain steps by up to the full amplitude in ONE sample. Bound sits between.
    float maxDelta = 0.0f;
    for (size_t n = 20 * 512; n < 34 * 512; ++n)
        maxDelta = juce::jmax (maxDelta, std::abs (out[n] - out[n - 1]));
    float tailPeak = 0.0f;
    for (size_t n = out.size() - 2400; n < out.size(); ++n)
        tailPeak = juce::jmax (tailPeak, std::abs (out[n]));

    check (maxDelta < 0.08f,
           "offlineReturn: coming back from a bounce fades, it does not step");
    check (tailPeak > 0.3f, "offlineReturn: and playback recovers to full level");
}

// ---------------------------------------------------------------------------
// The direct-adopt branch must clear the EQ's biquad history when the POSITION
// changes on it, exactly as the silent-bottom branch does — otherwise the new
// position starts from the other stream's past (MasteringEQ::resetState's own
// rule). Reachable since the offline-entry edge started using that branch:
// eqPosition differing on the very block nonRealtime first goes true.
//
// Isolation, so the assertion sees only the property: the input goes SILENT at
// the flip and Force Max changes the factor, so latchOsConfig empties the
// lookahead ring. Everything downstream of the region is then fed exact zeros
// and only the Post EQ can produce a nonzero sample — clean state gives
// exactly 0.0, stale state rings out the charged history.
static void testOfflineEntryClearsEqStateOnAPositionChange()
{
    const double sr = 48000.0;
    anabasis::AnabasisEngine engine;
    engine.prepare (sr, 512, 2);
    anabasis::EngineParameters p;
    p.truePeakMode      = false;
    p.oversample        = anabasis::OversampleFactor::x2;
    p.forceMaxOffline   = true;
    p.eqLowShelfGainDb  = 12.0f;      // charge the biquads hard
    p.eqLowShelfFreqHz  = 300.0f;
    p.eqPosition        = 0;          // Pre: the EQ sees the INPUT stream

    std::vector<float> out;
    juce::AudioBuffer<float> buf (2, 512);
    for (int b = 0; b < 16; ++b)
    {
        const bool afterFlip = b >= 10;
        if (afterFlip) { p.eqPosition = 1; p.nonRealtime = true; }   // both, same block
        for (int n = 0; n < 512; ++n)
        {
            const float v = afterFlip ? 0.0f
                                      : 0.4f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                         * 300.0f * (float) (b * 512 + n) / (float) sr);
            buf.setSample (0, n, v); buf.setSample (1, n, v);
        }
        engine.process (buf, p);
        for (int n = 0; n < 512; ++n) out.push_back (buf.getSample (0, n));
    }

    float prePeak = 0.0f, postPeak = 0.0f;
    for (size_t n = 8 * 512; n < 10 * 512; ++n)  prePeak  = juce::jmax (prePeak,  std::abs (out[n]));
    for (size_t n = 10 * 512; n < out.size(); ++n) postPeak = juce::jmax (postPeak, std::abs (out[n]));

    check (prePeak > 0.3f, "eqFlip: the EQ really was charged before the flip (baseline)");
    check (postPeak < 1.0e-6f,
           "eqFlip: a position change on the offline-entry edge starts from a cleared EQ state");
}

// ---------------------------------------------------------------------------
// inv 1 / ADR-0002 / DESIGN §2.5: the limiter push drives the LIMITER, not the
// clipper. It sits after Clip/Sat in the chain, so input gain and limiter push
// are NOT interchangeable — the clipper's operating point follows the first
// and is untouched by the second. Two renders reaching the clipper at levels
// 12 dB apart must therefore differ in harmonic content; if the push were
// applied upstream (as it was until the P4 review round) the two are bit-
// identical, because the compressor is inert at both levels and the only
// difference is which of the two gains carried the signal.
//
// The stimulus is chosen so nothing else moves: 0.05 peak with +12 dB of
// input gain is 0.2, still under the −3 dBFS comp knee bottom in BOTH runs,
// and the clipper's level compensation leaves both outputs near 0.2 — well
// below the ceiling (0.989 at the default; it was 0.891 when this was
// written), so the limiter and the clamp stay out of it and
// what the FFT sees is the clipper alone.
static void testLimiterPushDoesNotDriveTheClipper()
{
    const double sr = 48000.0;
    const int N = 8192, k = 500;      // 2.93 kHz, bin-aligned; h3 = bin 1500

    auto render = [&] (float inputDb, float pushDb)
    {
        anabasis::AnabasisEngine engine;
        engine.prepare (sr, 512, 2);
        anabasis::EngineParameters p;
        p.inputGainDb  = inputDb;
        p.limGainDb    = pushDb;
        p.clipDriveDb  = 18.0f;       // the clipper is doing real work
        p.clipMix      = 1.0f;
        p.truePeakMode = false;
        p.oversample   = anabasis::OversampleFactor::off;

        std::vector<float> out;
        juce::AudioBuffer<float> buf (2, 512);
        for (int b = 0; b < 30; ++b)  // 15360 samples: allowance + settle + N
        {
            for (int n = 0; n < 512; ++n)
            {
                const int t = b * 512 + n;
                const float v = 0.05f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                  * (float) k * (float) t / (float) N);
                buf.setSample (0, n, v); buf.setSample (1, n, v);
            }
            engine.process (buf, p);
            for (int n = 0; n < 512; ++n)
                out.push_back (buf.getSample (0, n));
        }
        return std::vector<float> (out.begin() + 4096, out.begin() + 4096 + N);
    };

    juce::dsp::FFT fft (13);
    auto magDb = [&] (const std::vector<float>& sig, int bin)
    {
        std::vector<float> buf (2 * (size_t) N, 0.0f);
        std::copy (sig.begin(), sig.end(), buf.begin());
        fft.performRealOnlyForwardTransform (buf.data(), true);
        const float re = buf[(size_t) (2 * bin)], im = buf[(size_t) (2 * bin + 1)];
        return 20.0f * std::log10 (juce::jmax (1.0e-12f, std::sqrt (re * re + im * im)));
    };

    const auto pushed = render (0.0f, 12.0f);   // clipper sees 0.05
    const auto driven = render (12.0f, 0.0f);   // clipper sees 0.20
    const float h3Pushed = magDb (pushed, 3 * k) - magDb (pushed, k);
    const float h3Driven = magDb (driven, 3 * k) - magDb (driven, k);

    // Measured: h3 sits at −127 dB relative in the pushed run (the clipper is
    // never reached — 0.05 × 7.94 = 0.40, inside the linear region) against
    // −17.7 dB in the driven run (0.20 × 7.94 = 1.59, hard into the knee).
    // 110 dB apart; asserted at 20. The level guard is 5 dB, not tighter: the
    // two runs sit at different points on the clipper's compensation curve
    // and land 2.7 dB apart, which is the mechanism working, not drift.
    check (std::abs (magDb (pushed, k) - magDb (driven, k)) < 5.0f,
           "pushPlacement: both runs land at a comparable output level (like compared with like)");
    check (h3Driven > h3Pushed + 20.0f,
           "pushPlacement: limiter push does not drive the clipper — input gain does");
}

// ---------------------------------------------------------------------------
// §2.4 colour: Clean is the null model at EVERY depth; depth 0 is exact with
// every model; balance swings the odd/even ratio; tone tilts the residue.
// KI-009's structural guard: the §2.4 clipper/colour stage CANNOT lose a
// channel. Both halves of that are asserted, over a wide parameter and
// stimulus sweep, because the field report's kill zone (0.1.3 addendum) runs
// through this stage and its gate is `clipMix` — the control that decides
// whether this stage's output reaches the wet ring at all.
//
//  (1) CHANNEL SYMMETRY. Feeding the SAME sequence to both channels must
//      produce BIT-IDENTICAL outputs, whatever the parameters. That is the
//      invariant a one-channel kill would have to violate: with it holding,
//      no state inside this stage can single out a channel, so a channel that
//      arrives alive leaves alive. (`activityEnv` and the tame gain are shared
//      across channels BY DESIGN — the image must not wander — and every other
//      state here is per channel and identically driven.)
//  (2) NO SELF-GENERATED NON-FINITE from bounded input. This is not a
//      duplicate of the finiteness tests: the ENGINE's boundary downstream of
//      this stage substitutes exactly 0.0f **for the offending channel alone**
//      (`AnabasisEngine::processChunk`, the wet-ring write), so a stage that
//      emits inf on one channel produces permanent one-channel DIGITAL
//      SILENCE — the field fingerprint precisely. The colour sub-block raises
//      to the FIFTH power (Transistor), and at `clipDrive == 0` the clipper is
//      exact-skipped, so nothing bounds its input inside the stage: this
//      assertion is what says the reachable input range never gets there.
//
// The mutant either half kills: any per-channel asymmetry introduced into this
// stage (a state array indexed by something other than `ch`, a per-channel
// branch, a shared accumulator made per-channel or vice versa) fails (1); a
// widened colour polynomial or a raised input range fails (2).
// ============================================================================
//  ClipSat must not be able to HIDE a non-finite from the engine's boundary.
//
//  Invariant 9 is a two-part contract: a boundary substitutes 0.0f for a
//  non-finite sample AND records `stageGeneratedNonFinite`, so the stage that
//  poisoned itself is repaired (`AnabasisEngine::processChunk` ->
//  `clip.sanitiseState()`). The whole mechanism is keyed on the fault being
//  VISIBLE at the boundary — on the stage's OUTPUT going non-finite.
//
//  Clip Mix at 0 breaks that key. The mix loop's exact endpoints leave the dry
//  sample untouched at M == 0 (the bit-exact identity path, which is correct and
//  must stay), so the stage emits a finite value NO MATTER what its internals
//  did. `tameLp[ch]` is nevertheless updated on BOTH tame branches — deliberately,
//  "keep state warm", so the 6 kHz split does not splice against stale audio when
//  the tame next engages. Warm state plus an invisible output is a latch: one
//  non-finite in `tameLp[ch]` survives every block, every preset load and every
//  A/B, because the flag that would repair it never rises.
//
//  It is then paid for later, on ORDINARY audio, the moment the user opens the
//  mix with the tame engaged — `wet[ch] += (gLin - 1) * (wet[ch] - tameLp[ch])`
//  is non-finite for that channel alone, the engine's ring substitutes exact
//  0.0f, and one channel goes digitally silent.
//
//  The fingerprint that shape produces is the reported one, which is why this
//  test exists rather than a comment: channel-local, gated on Clip Mix being
//  non-zero, absent at mix 0, upstream of the limiter's detector tap (so the
//  compressor still shows gain reduction on both channels while the limiter
//  shows it on one), and cured by bypass, which reads the dry ring.
// ============================================================================
// WHY THE COMPRESSOR DOES NOT PRE-EMPT THE SUSTAINED ONE-CHANNEL CASES BELOW,
// recorded because it is not obvious and those tests' premise rests on it. With
// the default RMS detector and a sustained 1e20+ input, `meanSquare[ch]`
// (`MasteringComp.h`) overflows to infinity on the first sample and to NaN on the
// second (inf − inf). From then on `grDbMin = juce::jmin (grDbMin, NaN)` keeps
// returning the running minimum, so the "no reduction anywhere" early return
// makes the compressor a pass-through and the huge sample reaches ClipSat intact
// — which is exactly what the ClipSat cases need. That is an ACCIDENT of jmin's
// NaN behaviour, not a designed property: a future change to how the compressor
// handles a non-finite detector would silently change what those tests exercise,
// rather than failing them. (It also means `meterCompGrDbCh` can publish NaN in
// that state; harmless to the audio, visible only to the GR lane's display.)
static void testClipSatCannotHideANonFiniteFromTheBoundary()
{
    const juce::ScopedNoDenormals noDenormals;

    auto drive = [] (anabasis::ClipSat& clip, const anabasis::EngineParameters& p,
                     float inL, float inR, int n)
    {
        anabasis::ClipSat& c = clip;
        c.setPerBlock (p);
        int bad = 0;
        for (int i = 0; i < n; ++i)
        {
            float f[2] = { inL, inR };
            c.processSample (f, 2);
            if (! std::isfinite (f[0]) || ! std::isfinite (f[1]))
                ++bad;
        }
        return bad;
    };

    anabasis::ClipSat clip;
    clip.prepare (48000.0);

    anabasis::EngineParameters p;
    p.clipDriveDb   = 0.0f;      // exact-skipped: the clipper's own bound is NOT in play
    p.clipShape     = 0.5f;
    p.colourDepth   = 0.0f;      // colour off, so this isolates the tame state
    p.colourModel   = 1;
    p.colourBalance = 0.0f;
    p.colourTone    = 0.0f;
    p.dynTiltDb     = 0.0f;      // tame IDLE — the branch that keeps state warm
    p.clipMix       = 0.0f;      // the setting the field report says CURES the fault

    // SWEEP the poisoning phase rather than assert one magnitude. The bound that
    // makes this safe is `kArithmeticLimit` on the colour argument, and a bound
    // is only demonstrated by driving up to and past it: the interesting inputs
    // are the ones near FLT_MAX (3.4e38), where a difference or a product inside
    // the tame is closest to overflowing, and ALTERNATING SIGN, which is what
    // keeps the 6 kHz split's state far from the sample it is subtracted from —
    // the arrangement that maximises |wet - tameLp|. Nothing here is hostile
    // input in the invariant-9 sense: the engine zeroes non-finite INPUT before
    // the EQ, so every value this stage can see is finite, and that is exactly
    // the premise the argument rests on.
    //
    // The colour stage is swept ON as well as off: it is the one sub-block that
    // can grow the through-signal (`c += dep * r`), so a tame overflow, if one
    // existed, would be reachable through it and not through the bare signal.
    int poisonedTotal = 0, hiddenTotal = 0, tameEngaged = 0, configs = 0;
    for (const float mag : { 1.0e6f, 1.0e20f, 1.0e30f, 1.0e38f, 3.4e38f })
        for (const int model : { 0, 1, 3 })
            for (const float dep : { 0.0f, 1.0f })
            {
                ++configs;
                anabasis::ClipSat c2;
                c2.prepare (48000.0);
                anabasis::EngineParameters q = p;
                q.colourModel = model;
                q.colourDepth = dep;

                // Phase 1 -- poison at Clip Mix 0, alternating sign, one channel.
                q.clipMix   = 0.0f;
                q.dynTiltDb = 0.0f;
                c2.setPerBlock (q);
                for (int i = 0; i < 4000; ++i)
                {
                    float f[2] = { (i & 1) ? mag : -mag, 0.25f };
                    c2.processSample (f, 2);
                    if (! std::isfinite (f[0]) || ! std::isfinite (f[1]))
                        ++hiddenTotal;          // the boundary WOULD have seen it
                }

                // Phase 2 -- the ordinary control moves the field report
                // describes: open the mix, raise Dynamic Tame, and DRIVE THE
                // CLIPPER.
                //
                // The drive is the load-bearing part and its absence made the
                // first version of this test vacuous. `activityRaw` is written
                // only inside the `clipOn` branch, so with `clipDriveDb == 0`
                // the activity envelope stays bit-zero, `tameGainDb` stays 0,
                // and the tame takes its IDLE branch — the branch that never
                // reads `tameLp[ch]` into the signal. A phase 2 that leaves the
                // drive at zero therefore claims to engage the tame and does
                // not, which is the same "the knob moved and nothing
                // downstream did" failure round 6 found across the whole
                // channel battery. 12 dB with a 0.9 input saturates hard
                // enough to push the envelope well past the -0.01 dB gate.
                q.clipMix     = 1.0f;
                q.dynTiltDb   = 2.0f;
                q.clipDriveDb = 12.0f;
                poisonedTotal += drive (c2, q, 0.9f, 0.9f, 4000);

                // …and assert the premise rather than trusting it, for exactly
                // the reason above: if a future change stops this configuration
                // engaging the tame, this test must go RED rather than quietly
                // stop testing anything.
                tameEngaged += (c2.activityEnvelope() > 0.0f) ? 1 : 0;
            }

    juce::String msg;
    msg << "clipSat: at Clip Mix 0 the stage's OUTPUT is the untouched dry sample, so a "
           "poisoned internal state would be INVISIBLE to the engine's boundary ("
        << hiddenTotal << " non-finite outputs during the poisoning phase — the count is the "
           "point: the boundary has nothing to catch)";
    check (hiddenTotal == 0, msg.toRawUTF8());

    msg.clear();
    msg << "clipSat: (premise) phase 2 really does engage the dynamic tame — the branch that "
           "reads tameLp into the signal, and the only one a latched state could reach ("
        << tameEngaged << " / " << configs << " configurations)";
    check (tameEngaged == configs, msg.toRawUTF8());

    msg.clear();
    msg << "clipSat: and nothing latches — after " << configs << " poisoning attempts up to "
           "FLT_MAX with the colour stage swept, opening the mix and driving the clipper with "
           "the tame ENGAGED leaves ordinary audio finite on both channels ("
        << poisonedTotal << " non-finite output samples)";
    check (poisonedTotal == 0, msg.toRawUTF8());
}

static void testClipSatCannotLoseAChannel()
{
    // Same runtime as the shipping plugin: `processBlock` opens with
    // `juce::ScopedNoDenormals`, so a stage property asserted without FTZ/DAZ
    // is a property of the harness rather than of the product.
    const juce::ScopedNoDenormals noDenormals;
    unsigned st = 424242u;
    auto rnd = [&st] (int n) { st = st * 1664525u + 1013904223u;
                               return (int) ((st >> 16) % (unsigned) n); };
    auto rf  = [&st] () { st = st * 1664525u + 1013904223u;
                          return (float) ((st >> 8) & 0xffff) / 65535.0f; };

    int asymmetric = 0, nonFinite = 0, cases = 0;
    for (int trial = 0; trial < 600; ++trial)
    {
        anabasis::EngineParameters p;
        p.clipDriveDb   = (float) rnd (5) * 6.0f;
        p.clipShape     = rf();
        p.colourDepth   = (float) rnd (3) * 0.5f;
        p.colourBalance = rf() * 2.0f - 1.0f;
        p.colourTone    = rf() * 2.0f - 1.0f;
        p.dynTiltDb     = (float) rnd (3);
        p.clipMix       = (float) rnd (5) * 0.25f;
        p.colourModel   = rnd (4);
        const double sr = rnd (2) ? 48000.0 : 192000.0;   // base and a 4x region rate

        anabasis::ClipSat clip;
        clip.prepare (sr);
        clip.setPerBlock (p);

        // The stimuli the ordinary sine battery never produces, each chosen
        // for a different way a stage can misbehave: Nyquist alternation (the
        // ADAA's own (1+z⁻¹)/2 null), gross over-scale, a large DC pedestal,
        // sparse impulses against near-silence, and denormal-scale material.
        const int kind = rnd (8);
        for (int n = 0; n < 4000; ++n)
        {
            const float ph = 2.0f * juce::MathConstants<float>::pi * (float) n / (float) sr;
            float v;
            switch (kind)
            {
                case 0:  v = 0.9f * std::sin (ph * 220.0f); break;
                case 1:  v = (n & 1) ? 0.99f : -0.99f; break;
                case 2:  v = 3.0f * std::sin (ph * 50.0f); break;
                case 3:  v = 0.5f + 0.4f * std::sin (ph * 90.0f); break;
                case 4:  v = (n % 997 == 0) ? 0.98f : 1.0e-20f; break;
                case 5:  v = 1.0e-24f * (float) (n % 3); break;
                // The two OVERFLOW magnitudes, which a ≤ 3.0 sweep cannot see
                // and which are exactly where this stage's two arithmetic
                // guards live: the colour polynomial's fifth power gives up
                // around 5.1e7, and the drive product `dry · g` around 2e37
                // once `g` leaves unity. Both are far outside anything the
                // chain can deliver — they are here because the stage's
                // guarantee is stated WITHOUT an exception, and a guarantee
                // no stimulus reaches is a guarantee no mutant can break.
                case 6:  v = 4.0e8f * std::sin (ph * 220.0f); break;
                default: v = 1.0e38f * std::sin (ph * 220.0f); break;
            }
            float frame[2] = { v, v };
            clip.processSample (frame, 2);
            ++cases;
            if (! juce::exactlyEqual (frame[0], frame[1]))
                ++asymmetric;
            if (! std::isfinite (frame[0]) || ! std::isfinite (frame[1]))
                ++nonFinite;
        }
    }
    juce::String msg;
    msg << "clipSat: the stage is channel-symmetric — identical input sequences leave "
           "bit-identical (" << cases << " samples swept, " << asymmetric << " divergent)";
    check (asymmetric == 0, msg.toRawUTF8());
    msg.clear();
    msg << "clipSat: …and never emits a non-finite value from bounded input, so the engine's "
           "per-channel zero substitution is never armed by this stage (" << nonFinite << ")";
    check (nonFinite == 0, msg.toRawUTF8());

    // ---- CROSS-CHANNEL INDEPENDENCE, the property KI-009 would have to break -
    // Symmetry (above) says the two channels are treated ALIKE. That is not
    // the same statement as: what happens on channel 1 cannot destroy channel
    // 0. A stage can be perfectly symmetric and still have a shared term that
    // one channel drives into the ground — and "one channel silent while the
    // other plays" is exactly that shape.
    //
    // So this asserts the structural claim directly, by DIFFERENCE: run the
    // SAME channel-0 input twice, once beside a silent channel 1 and once
    // beside a hostile one, and compare channel 0's output. The stage has
    // exactly one cross-channel term — `activityRaw`, the clip-depth maximum
    // that drives the shared dynamic-tame gain — and it is an ATTENUATION
    // bounded by `dynTilt` (≤ 2 dB). So channel 0's two renders may differ by
    // at most that, and by NOTHING else. Any future cross-channel coupling
    // that could zero a channel — a shared divisor, a shared envelope used as
    // a gain without a bound, a max/min that leaks one channel's magnitude
    // into the other's gain — fails this immediately, whether or not it is
    // symmetric.
    //
    // Run at dynTilt = 0 as well, where the bound collapses to BIT-EXACT
    // independence: with the tame disengaged the channels share nothing at
    // all, and that is the strongest form the architecture allows.
    //
    // WHAT MUTATION TESTING TAUGHT HERE, because it is a stronger structural
    // result than the assertion states and it belongs with the assertion.
    // Widening the shared gain alone does NOT break this — the tame is a
    // SHELF (`wet += (gLin−1)·(wet − tameLp)`, i.e. `gLin·wet + (1−gLin)·tameLp`),
    // so however far `gLin` falls the lowpass leg survives, and a first-order
    // lowpass still passes ~0.37 of its input at Nyquist. **The shared term
    // therefore cannot zero a channel even at an unbounded gain** — the
    // structure, not the 2 dB range, is what makes a cross-channel kill
    // impossible here. What DOES break the assertion is turning that shelf
    // into a broadband gain (`wet *= gLin`), which is the actual failure class:
    // a shared envelope used as a gain. That mutant reads −48.5 dB.
    // (A third lesson, kept because it cost a cycle: the probe tone must sit
    // ABOVE the shelf corner — a 220 Hz probe passes any mutant, because below
    // the corner the shelf does nothing whatever the gain is.)
    {
        auto renderChannelZero = [] (float tiltDb, bool hostileNeighbour,
                                     std::vector<float>& out)
        {
            anabasis::EngineParameters p;
            p.clipDriveDb = 12.0f; p.clipShape = 0.4f; p.colourDepth = 1.0f;
            p.colourModel = 3;     p.dynTiltDb = tiltDb; p.clipMix = 1.0f;
            anabasis::ClipSat clip;
            clip.prepare (48000.0);
            clip.setPerBlock (p);
            out.resize (24000);
            for (int n = 0; n < 24000; ++n)
            {
                const float ph = 2.0f * juce::MathConstants<float>::pi * (float) n / 48000.0f;
                // Channel 0's tone sits ABOVE the tame's ~6 kHz shelf corner
                // on purpose: below it the shelf barely acts and the bound
                // would pass however wide the shared gain became, which is a
                // test that cannot fail rather than a test that holds. (Found
                // by mutation: a 220 Hz probe survived a 40× widened tame.)
                float frame[2] = { 0.42f * std::sin (ph * 9000.0f),
                                   hostileNeighbour ? 8.0f * std::sin (ph * 3100.0f) : 0.0f };
                clip.processSample (frame, 2);
                out[(size_t) n] = frame[0];
            }
        };

        std::vector<float> alone, beside;
        renderChannelZero (0.0f, false, alone);
        renderChannelZero (0.0f, true,  beside);
        int differing = 0;
        for (size_t n = 0; n < alone.size(); ++n)
            if (! juce::exactlyEqual (alone[n], beside[n]))
                ++differing;
        juce::String crossMsg;
        crossMsg << "clipSat: with the dynamic tame idle the channels are BIT-EXACTLY independent — "
               "a hostile neighbour cannot move this channel by one ulp (" << differing << " differ)";
        check (differing == 0, crossMsg.toRawUTF8());

        renderChannelZero (2.0f, false, alone);
        renderChannelZero (2.0f, true,  beside);
        // A LEVEL comparison, not a per-sample ratio: the tame is a high-shelf
        // cut, so it shifts phase a little and a sample-by-sample dB ratio
        // explodes harmlessly at every zero crossing (measured ~28 dB there
        // while the level moves under 2). Level is also the claim that matters
        // — "cannot push this channel toward silence" is a statement about how
        // much energy survives, not about any one sample.
        double sumAlone = 0.0, sumBeside = 0.0;
        for (size_t n = alone.size() / 4; n < alone.size(); ++n)   // settled portion
        {
            sumAlone  += (double) alone[n]  * alone[n];
            sumBeside += (double) beside[n] * beside[n];
        }
        const double levelDelta = 20.0 * std::log10 (std::sqrt (sumBeside)
                                                     / juce::jmax (1.0e-30, std::sqrt (sumAlone)));
        crossMsg.clear();
        crossMsg << "clipSat: with the tame at its 2 dB maximum a hostile neighbour moves this channel's "
               "LEVEL by at most that shared attenuation, never toward silence ("
            << (float) levelDelta << " dB)";
        check (std::abs (levelDelta) < 2.5, crossMsg.toRawUTF8());
    }

    // The other direction, and the one the wrapper battery cannot see at unit
    // level: an ASYMMETRIC input must leave both channels alive. A channel
    // carrying programme cannot be zeroed by anything this stage does to the
    // other one — the shared tame gain is the only cross-channel term, and it
    // is an attenuation bounded by `dynTilt` (≤ 2 dB).
    {
        anabasis::EngineParameters p;
        p.clipDriveDb = 18.0f; p.clipShape = 0.3f; p.colourDepth = 1.0f;
        p.colourModel = 3; p.dynTiltDb = 2.0f; p.clipMix = 1.0f;
        anabasis::ClipSat clip;
        clip.prepare (48000.0);
        clip.setPerBlock (p);
        double sumSq[2] = { 0.0, 0.0 };
        for (int n = 0; n < 48000; ++n)
        {
            const float ph = 2.0f * juce::MathConstants<float>::pi * (float) n / 48000.0f;
            // L thirty dB below R, and at a different frequency: the quiet
            // channel is the one a cross-channel defect would swallow.
            float frame[2] = { 0.03f * std::sin (ph * 220.0f), 0.95f * std::sin (ph * 3000.0f) };
            clip.processSample (frame, 2);
            if (n >= 24000)
                for (int ch = 0; ch < 2; ++ch)
                    sumSq[ch] += (double) frame[ch] * frame[ch];
        }
        const double rmsL = std::sqrt (sumSq[0] / 24000.0);
        check (rmsL > 0.005,
               "clipSat: a quiet channel survives a loud one being clipped and coloured beside it");
    }
}

static void testColourModelsBalanceAndTone()
{
    const double sr = 48000.0;
    const int N = 8192, warm = 4096, k = 512;   // f0 = 3 kHz exact bin

    auto renderMag = [&] (int model, float depth, float bal, float ton,
                          int bin) -> float
    {
        anabasis::ClipSat clip;
        clip.prepare (sr);
        anabasis::EngineParameters p;
        p.colourModel = model; p.colourDepth = depth; p.colourBalance = bal;
        p.colourTone = ton; p.clipMix = 1.0f;
        clip.setPerBlock (p);
        std::vector<float> out (N);
        for (int n = 0; n < warm + N; ++n)
        {
            const float x = 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi
                                             * (float) k * (float) n / (float) N);
            float frame[2] = { x, x };
            clip.processSample (frame, 2);
            if (n >= warm) out[(size_t) (n - warm)] = frame[0];
        }
        juce::dsp::FFT fft (13);
        std::vector<float> buf (2 * (size_t) N, 0.0f);
        std::copy (out.begin(), out.end(), buf.begin());
        fft.performRealOnlyForwardTransform (buf.data(), true);
        const float re = buf[(size_t) (2 * bin)], im = buf[(size_t) (2 * bin + 1)];
        return 20.0f * std::log10 (juce::jmax (1.0e-12f, std::sqrt (re * re + im * im)));
    };

    {   // depth 0 → bit-exact with a coloured model and wild balance/tone
        anabasis::ClipSat clip;
        clip.prepare (sr);
        anabasis::EngineParameters p;
        p.colourModel = 1; p.colourDepth = 0.0f; p.colourBalance = 1.0f;
        p.colourTone = -1.0f; p.clipMix = 1.0f;
        clip.setPerBlock (p);
        bool exact = true;
        for (int n = 0; n < 24000; ++n)
        {
            const float v = 0.6f * std::sin (0.41f * (float) n);
            float frame[2] = { v, v };
            clip.processSample (frame, 2);
            if (! juce::exactlyEqual (frame[0], v)) { exact = false; break; }
        }
        check (exact, "colour: depth 0 is exact identity regardless of model");
    }
    {   // Clean at full depth → still exact identity (the null model)
        anabasis::ClipSat clip;
        clip.prepare (sr);
        anabasis::EngineParameters p;
        p.colourModel = 0; p.colourDepth = 1.0f; p.clipMix = 1.0f;
        clip.setPerBlock (p);
        bool exact = true;
        for (int n = 0; n < 24000; ++n)
        {
            const float v = 0.6f * std::sin (0.41f * (float) n);
            float frame[2] = { v, v };
            clip.processSample (frame, 2);
            if (! juce::exactlyEqual (frame[0], v)) { exact = false; break; }
        }
        check (exact, "colour: Clean applies more of nothing at every depth");
    }

    const int h2 = 2 * k, h3 = 3 * k;
    const float h2AllEven = renderMag (2, 1.0f, -1.0f, 0.0f, h2);
    const float h3AllEven = renderMag (2, 1.0f, -1.0f, 0.0f, h3);
    const float h2AllOdd  = renderMag (2, 1.0f,  1.0f, 0.0f, h2);
    const float h3AllOdd  = renderMag (2, 1.0f,  1.0f, 0.0f, h3);
    check (h2AllEven > h2AllOdd + 20.0f, "colour: balance -1 keeps the even harmonic only");
    check (h3AllOdd  > h3AllEven + 20.0f, "colour: balance +1 keeps the odd harmonic only");

    // Tone: the 3rd harmonic of a 3 kHz tone (9 kHz) sits in the HP half of
    // the 2 kHz split — bright keeps it, dark cuts it.
    const float h3Bright = renderMag (1, 1.0f, 1.0f,  1.0f, h3);
    const float h3Dark   = renderMag (1, 1.0f, 1.0f, -1.0f, h3);
    check (h3Bright > h3Dark + 6.0f,                  // measured 48.4 vs 37.0 dB
           "colour: tone tilts the residue bright/dark");
}

// ---------------------------------------------------------------------------
// §2.2's dynamic HF tame lives in this stage: it cuts up to dynTilt dB above
// ~6 kHz WHILE the clipper is working, and does exactly nothing when it is
// not — same output bit-for-bit as dynTilt 0, because harshness that does not
// exist must not be "tamed".
static void testDynamicTame()
{
    const double sr = 48000.0;
    float quietEnv = -1.0f;
    auto render = [&] (float amp, float tame) -> std::vector<float>
    {
        anabasis::ClipSat clip;
        clip.prepare (sr);
        anabasis::EngineParameters p;
        p.clipDriveDb = 12.0f; p.clipShape = 0.0f; p.dynTiltDb = tame; p.clipMix = 1.0f;
        clip.setPerBlock (p);
        std::vector<float> out;
        out.reserve (48000);
        for (int n = 0; n < 48000; ++n)
        {
            const float v = amp * std::sin (2.0f * juce::MathConstants<float>::pi
                                            * 8000.0f * (float) n / (float) sr);
            float frame[2] = { v, v };
            clip.processSample (frame, 2);
            out.push_back (frame[0]);
        }
        quietEnv = clip.activityEnvelope();
        return out;
    };

    auto tailRmsDb = [] (const std::vector<float>& v)
    {
        double s = 0.0; int c = 0;
        for (size_t n = v.size() / 2; n < v.size(); ++n) { s += (double) v[n] * v[n]; ++c; }
        return 20.0 * std::log10 (std::sqrt (s / c));
    };

    const auto loud0 = render (0.9f, 0.0f), loud2 = render (0.9f, 2.0f);
    check (tailRmsDb (loud2) < tailRmsDb (loud0) - 0.5,   // measured −13.53 vs −12.96
           "tame: a clipping 8 kHz tone is cut by the dynamic shelf");

    const auto quiet0 = render (0.05f, 0.0f), quiet2 = render (0.05f, 2.0f);
    bool identical = true;
    for (size_t n = 0; n < quiet0.size(); ++n)
        if (! juce::exactlyEqual (quiet0[n], quiet2[n])) { identical = false; break; }
    check (identical, "tame: with nothing clipping, dynTilt changes nothing at all");
    // MECHANICAL, not consequential. The §5.4 dynTilt trim reaches its +0.5 dB
    // clamp at factory defaults with the features un-converged, so the
    // invariant-7 bit-exact null does NOT hold by the trim being zero — it
    // holds because this envelope is exactly 0.0f while nothing clips, which
    // keeps `tameGainDb` at 0 and the whole branch skipped. That is a state
    // invariant in ClipSat.h that AdaptiveEngine.h depends on, so seeding or
    // flooring the detector would break the null from a file that never
    // mentions adaptation. Both headers warn about it; this asserts it. Note a
    // SMALL non-zero value would not fail the null above (the branch needs
    // `tameGainDb < -0.01f`), which is exactly why the check is on the zero
    // rather than on the null.
    check (juce::exactlyEqual (quietEnv, 0.0f),
           "tame: the activity envelope is bit-zero while nothing clips (invariant 7's third leg)");
}

// ---------------------------------------------------------------------------
static void testClipMixZeroIsDry()
{
    anabasis::ClipSat clip;
    clip.prepare (48000.0);
    anabasis::EngineParameters p;
    p.clipDriveDb = 18.0f; p.colourDepth = 1.0f; p.dynTiltDb = 2.0f; p.clipMix = 0.0f;
    clip.setPerBlock (p);
    bool exact = true;
    for (int n = 0; n < 24000; ++n)
    {
        const float v = 0.9f * std::sin (0.37f * (float) n);
        float frame[2] = { v, v };
        clip.processSample (frame, 2);
        if (! juce::exactlyEqual (frame[0], v)) { exact = false; break; }
    }
    check (exact, "clip: 0% mix is bit-exact dry under heavy drive and colour");
}

// ---------------------------------------------------------------------------
// inv 3 (P2 form): the true-peak estimator against known inter-sample-peak
// signals. fs/4 at phase pi/4 samples +-0.7071 while the true peak is 1.0
// (+3.01 dB ISP) and lands EXACTLY on a 4x phase — the canonical grid-aligned
// vector, asserted at <=0.1 dB. A peak between two 4x points under-reads more;
// that is a property of every max-reading 4x estimator (BS.1770's own
// tolerance envelope allows it) — measured and bounded rather than hidden.
static void testTruePeakAccuracy()
{
    auto measureDb = [] (float phase)
    {
        anabasis::TruePeakEstimator e;
        e.prepare();
        float best = 0.0f;
        for (int n = 0; n < 2000; ++n)
        {
            const float x = std::sin (juce::MathConstants<float>::pi * 0.5f * (float) n + phase);
            const float fr[2] = { x, x };
            float out[2];
            e.processFrame (fr, 2, out);
            if (n > 50) best = juce::jmax (best, out[0]);
        }
        return 20.0f * std::log10 (best);           // the true peak is 0 dB
    };

    const float grid    = measureDb (juce::MathConstants<float>::pi * 0.25f);
    // The continuous peak of sin(pi/2*t + phi) sits at t = 1 - 2*phi/pi, so
    // phi = 0.3125*pi puts it at t = 0.375 — exactly BETWEEN two 4x points
    // (an earlier phi = pi/8 landed the peak ON the 0.75 grid point and
    // measured -0.002 dB, i.e. it tested nothing). Measured here: -0.17 dB,
    // the max-reading 4x property.
    const float offGrid = measureDb (juce::MathConstants<float>::pi * 0.3125f);
    const float onSamp  = measureDb (juce::MathConstants<float>::pi * 0.5f);

    check (std::abs (grid) <= 0.1f,   "truePeak: grid-aligned +3 dB ISP vector reads within 0.1 dB");
    check (offGrid > -0.6f && offGrid <= 0.1f,
           "truePeak: off-grid worst case bounded (max-reading 4x property, recorded)");
    check (std::abs (onSamp) <= 0.1f, "truePeak: an on-sample peak reads exactly");
}

// ---------------------------------------------------------------------------
// §2.5: in true-peak mode the CEILING IS dBTP-AWARE — a signal whose sample
// peaks sit under the ceiling but whose inter-sample peaks exceed it engages
// the limiter; with the mode off it passes untouched.
static void testLimiterTruePeakMode()
{
    const double sr = 48000.0;
    auto settledGain = [&] (bool tpOn)
    {
        anabasis::AnabasisEngine engine;
        engine.prepare (sr, 512, 2);
        anabasis::EngineParameters p;
        p.truePeakMode      = tpOn;
        p.transientPreserve = 0.0f;
        p.lookaheadMs       = 2.0f;
        // EXPLICIT ceiling: the stimulus below is calibrated against -1 dBTP,
        // and this test is about TP-awareness, not about what the default is
        // (the default moved to -0.1 dBTP on 2026-08-05, where a 0.95 true
        // peak no longer exceeds it and the tpOn case would show nothing).
        p.ceilingDbTp       = -1.0f;
        // fs/4 phase pi/4: sample peak 0.601, TRUE peak 0.85 — ceiling -1 dBTP
        // (0.891)... needs the ISP peak OVER the ceiling: amp 0.95: samples
        // 0.672, true peak 0.95 > 0.891.
        juce::AudioBuffer<float> buf (2, 512);
        float outPeak = 0.0f;
        for (int b = 0; b < 20; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                const int t = b * 512 + n;
                const float v = 0.95f * std::sin (juce::MathConstants<float>::pi * 0.5f * (float) t
                                                  + juce::MathConstants<float>::pi * 0.25f);
                buf.setSample (0, n, v); buf.setSample (1, n, v);
            }
            engine.process (buf, p);
            if (b >= 16) outPeak = juce::jmax (outPeak, buf.getMagnitude (0, 512));
        }
        return outPeak;   // sample-domain output peak of the settled signal
    };

    const float offPeak = settledGain (false);
    const float onPeak  = settledGain (true);
    check (std::abs (offPeak - 0.6717f) < 5.0e-3f,
           "tpMode off: sample peaks under the ceiling pass untouched");
    check (onPeak < offPeak * 0.97f,
           "tpMode on: the same signal is reduced — the ceiling is dBTP-aware");
}

// ---------------------------------------------------------------------------
// §2.5 stereo link: at 1 both channels share the worst-case gain; at 0 a loud
// left channel does not duck a quiet right; between, partially.
static void testLimiterStereoLink()
{
    const double sr = 48000.0;
    auto gains = [&] (float link)
    {
        anabasis::LookaheadLimiter lim;
        lim.prepare (sr, 480);
        lim.setRelease (400.0f);
        lim.setStereoLink (link);
        float g[2] = { 1.0f, 1.0f };
        for (int t = 0; t < 2000; ++t)
        {
            const float fed[2] = { 1.0f, 0.1f };   // loud L, quiet R
            lim.processSample (fed, 2, 96, 0.5f, g);
        }
        return std::make_pair (g[0], g[1]);
    };

    auto [l1, r1] = gains (1.0f);
    auto [l0, r0] = gains (0.0f);
    auto [lh, rh] = gains (0.5f);

    check (juce::exactlyEqual (l1, r1),        "link 1: both channels share one gain");
    check (l0 < 0.6f && r0 > 0.95f,            "link 0: the quiet channel is not ducked");
    check (rh > r1 && rh < r0,                 "link 0.5: partial linking sits between");
}

// ---------------------------------------------------------------------------
// §2.5 auto release is dual-stage, pinned with the same disjoint-bounds
// technique as the compressor's: the deceleration ratio (rec2 < 0.6*rec1)
// forces a single pole under ~196 ms, the 800 ms tail-hold forces one over
// ~280 ms — only a genuine two-stage passes both.
static void testLimiterAutoReleaseIsTwoStage()
{
    const double sr = 48000.0;
    anabasis::LookaheadLimiter lim;
    lim.prepare (sr, 480);
    lim.setAutoRelease (true);

    float g[1] = { 1.0f };
    auto run = [&] (float level, double seconds)
    {
        for (int t = 0; t < (int) (seconds * sr); ++t)
        {
            const float fed[1] = { level };
            lim.processSample (fed, 1, 96, 0.5f, g);
        }
        return g[0];
    };

    const float held  = run (2.0f, 0.5);         // 12 dB over: gain 0.25
    check (held < 0.3f, "limAuto: (premise) the burst drives deep reduction");
    const float g100  = run (0.01f, 0.1);
    const float g200  = run (0.01f, 0.1);
    const float g800  = run (0.01f, 0.6);

    const float rec1 = g100 - held, rec2 = g200 - g100;
    check (rec1 > 0.15f,       "limAuto: the fast stage gives real recovery in 100 ms");
    check (rec2 < rec1 * 0.6f, "limAuto: recovery decelerates (kills slow single poles)");
    check (1.0f - g800 > 0.04f, "limAuto: still held at 800 ms (kills fast single poles)");
}

// ---------------------------------------------------------------------------
// §2.5 styles are envelope-constant presets: Loud releases fastest,
// Transparent slowest; Punchy lets more of a hit through at the play instant
// than Transparent at the same transientPreserve.
static void testLimiterStyles()
{
    const double sr = 48000.0;
    auto recovered = [&] (int style)
    {
        anabasis::LookaheadLimiter lim;
        lim.prepare (sr, 480);
        lim.setRelease (400.0f);
        lim.setStyle (style);
        float g[1] = { 1.0f };
        for (int t = 0; t < 24000; ++t)          // 0.5 s burst
        { const float fed[1] = { 2.0f }; lim.processSample (fed, 1, 96, 0.5f, g); }
        for (int t = 0; t < 4800; ++t)           // 100 ms quiet
        { const float fed[1] = { 0.01f }; lim.processSample (fed, 1, 96, 0.5f, g); }
        return g[0];
    };
    const float trans = recovered (0), loud = recovered (2);
    check (loud > trans + 0.05f, "styles: Loud recovers visibly faster than Transparent");

    auto pokeAtPlay = [&] (int style)
    {
        anabasis::LookaheadLimiter lim;
        lim.prepare (sr, 480);
        lim.setRelease (400.0f);
        lim.setStyle (style);
        lim.setTransientPreserve (0.6f);
        const int w = 24;                         // 0.5 ms: preserve visibly lags
        float g[1] = { 1.0f };
        float atPlay = 1.0f;
        for (int t = 0; t < 600; ++t)
        {
            const float fed[1] = { (t == 400) ? 2.0f : 0.01f };
            lim.processSample (fed, 1, w, 0.5f, g);
            if (t == 400 + w) atPlay = g[0];
        }
        return atPlay;
    };
    const float pokeT = pokeAtPlay (0), pokeP = pokeAtPlay (1);
    check (pokeP > pokeT * 1.02f, "styles: Punchy lets more of the hit through at the play instant");
}

// ---------------------------------------------------------------------------
// §2.5 transient preserve: at 0 the attack is EXACT (state == needed the
// moment the spike enters the window — the wedge tests rely on it); at 1 the
// envelope lags so the front of the hit pokes into the clamp.
static void testLimiterTransientPreserve()
{
    const double sr = 48000.0;
    auto atPlay = [&] (float preserve)
    {
        anabasis::LookaheadLimiter lim;
        lim.prepare (sr, 480);
        lim.setRelease (400.0f);
        lim.setTransientPreserve (preserve);
        const int w = 24;
        float g[1] = { 1.0f };
        float res = 1.0f;
        for (int t = 0; t < 600; ++t)
        {
            const float fed[1] = { (t == 400) ? 2.0f : 0.0f };
            lim.processSample (fed, 1, w, 0.5f, g);
            if (t == 400 + w) res = g[0];
        }
        return res;
    };
    check (juce::exactlyEqual (atPlay (0.0f), 0.25f),
           "preserve 0: instant attack — the gain IS needed when the hit plays");
    const float p1 = atPlay (1.0f);
    check (p1 > 0.3f && p1 < 1.0f,
           "preserve 1: the envelope deliberately lags into the clamp's territory");
}

// ---------------------------------------------------------------------------
// ADR-0023 (0.1.2): the limiter's detector is UNFILTERED — its threshold IS
// the ceiling, so detection must track the actual peak whatever the SC HPF
// is set to. Until 0.1.2 the shared HPF also fed this detector (brief §3),
// breaking the ceiling relationship in both directions: a 30 Hz over was
// invisible to the limiter (the CeilingClamp then flat-topped it — hard
// clipping instead of limiting), and the filter's edge overshoot drew
// reduction on legal material (the item-2 field report). The first half pins
// the repaired deaf direction at the engine level; the second pins the
// over-read direction at the COMP (the stage that keeps the filter): its
// filtered detector magnitude is clamped to the raw one, so filtering can
// only lower detection, never raise it.
static void testLimiterDetectorIsUnfiltered()
{
    const double sr = 48000.0;

    {   // A 30 Hz tone driven over the ceiling DUCKS with the SC HPF at
        // 300 Hz — the setting under which the old filtered detector was
        // deaf to it and the limiter passed it at unity.
        anabasis::AnabasisEngine engine;
        engine.prepare (sr, 512, 2);
        anabasis::EngineParameters p;
        p.scHpfFreqHz = 300.0f;
        juce::AudioBuffer<float> buf (2, 512);
        float minGain = 1.0f;
        for (int b = 0; b < 100; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                const float v = 1.9f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                 * 30.0f * (float) (b * 512 + n) / (float) sr);
                buf.setSample (0, n, v);
                buf.setSample (1, n, v);
            }
            engine.process (buf, p);
            if (b >= 50)
                minGain = juce::jmin (minGain, engine.lastBlockMinGain());
        }
        check (minGain < 0.75f,
               "limDet: a 30 Hz over ducks with the SC HPF at 300 Hz — the detector is unfiltered");
    }

    {   // The comp's filtered detector never reduces deeper than the raw one
        // on an LF square, whose edges the biquad overshoots by ~2×: the
        // magnitude clamp caps detection at the raw sample.
        auto minGrOnSquare = [&] (float hpfHz)
        {
            anabasis::MasteringComp comp;
            comp.prepare (sr);
            anabasis::EngineParameters q;
            q.compThresholdDb = -6.0f; q.compKneeDb = 0.0f; q.compRatio = 4.0f;
            q.compAttackMs = 0.1f; q.compAutoRelease = false; q.compReleaseMs = 1000.0f;
            q.compDetector = 1;                 // Peak: the overshoot reaches the curve directly
            q.compMix = 1.0f; q.scHpfFreqHz = hpfHz;
            comp.setPerBlock (q);
            float minGr = 0.0f;
            for (int n = 0; n < 48000; ++n)
            {
                const float v = std::sin (2.0f * juce::MathConstants<float>::pi
                                          * 30.0f * (float) n / (float) sr) >= 0.0f
                                    ? 0.45f : -0.45f;   // −6.9 dBFS peak, under the −6 threshold
                float fr[2] = { v, v };
                comp.processSample (fr, 2);
                minGr = juce::jmin (minGr, comp.currentGainReductionDb());
            }
            return minGr;
        };
        const float raw = minGrOnSquare (20.0f), filtered = minGrOnSquare (100.0f);
        check (juce::exactlyEqual (raw, 0.0f),
               "limDet/comp: the raw detector computes nothing under the threshold (the baseline)");
        check (filtered >= raw - 0.05f,
               "limDet/comp: the filtered detector never reduces deeper than the raw one");
    }

    {   // …and the guard must not RE-COUPLE the detector to the bass it
        // exists to ignore (0.1.2 review). The first form of the overshoot
        // bound was pointwise — `min(|filtered|, |raw|)` — and `|raw|` passes
        // through zero twice per bass cycle, so the clamp gated the detector
        // at the BASS rate whatever the passband content was doing.
        //
        // Stimulus: a loud 30 Hz fundamental under quiet 3 kHz programme,
        // with the SC HPF at 300 Hz — the configuration the control is sold
        // for. The 3 kHz content is steady, the filter removes the 30 Hz
        // (−40 dB, two octaves down at 12 dB/oct), so a detector that is
        // genuinely deaf to bass produces STEADY reduction. Ripple in the
        // settled gain reduction is precisely the modulation this fix
        // removes, and RMS mode is where it shows: the 10 ms integrator
        // smooths the 3 kHz carrier but not a 30 Hz envelope.
        auto grRipple = [&] (float hpfHz)
        {
            anabasis::MasteringComp comp;
            comp.prepare (sr);
            anabasis::EngineParameters q;
            q.compThresholdDb = -30.0f; q.compKneeDb = 0.0f; q.compRatio = 20.0f;
            q.compAttackMs = 1.0f; q.compAutoRelease = false; q.compReleaseMs = 5.0f;
            q.compDetector = 0;                 // RMS, the factory mode
            q.compMix = 1.0f; q.scHpfFreqHz = hpfHz;
            comp.setPerBlock (q);
            float lo = 0.0f, hi = -200.0f;
            for (int n = 0; n < 48000; ++n)
            {
                const float t    = (float) n / (float) sr;
                const float bass = 0.90f * std::sin (2.0f * juce::MathConstants<float>::pi * 30.0f * t);
                const float hf   = 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi * 3000.0f * t);
                float fr[2] = { bass + hf, bass + hf };
                comp.processSample (fr, 2);
                if (n >= 24000)                 // settled half only
                {
                    const float gr = comp.currentGainReductionDb();
                    lo = juce::jmin (lo, gr);
                    hi = juce::jmax (hi, gr);
                }
            }
            return hi - lo;                     // peak-to-peak GR, dB
        };
        // The HPF-OFF run is the reference for what bass-coupled reduction
        // looks like: there the detector is SUPPOSED to follow the 30 Hz
        // envelope, so its ripple is large. Engaging the filter must collapse
        // that ripple — the whole point of the control — and the pointwise
        // clamp did not, because it reinstated the same envelope through the
        // ceiling. Bounds an order of magnitude apart, not a tolerance.
        const float rippleOff = grRipple (20.0f), rippleOn = grRipple (300.0f);
        // MEASURED, 48 kHz: 1.295 dB unfiltered · 0.291 dB with the pointwise
        // clamp this fix replaced · 0.0026 dB with the envelope ceiling. The
        // pointwise form left 22 % of the unfiltered bass modulation standing;
        // the ceiling leaves 0.2 %. The bounds sit an order of magnitude clear
        // of BOTH the measurement and the superseded form, so the pointwise
        // clamp is a mutant this test kills rather than a variant it tolerates.
        check (rippleOff > 1.0f,
               "limDet/comp: (premise) with the HPF off the detector tracks the bass envelope");
        check (rippleOn < 0.05f,
               "limDet/comp: with the HPF on the bass no longer modulates the detector");
    }
}

// ---------------------------------------------------------------------------
// ADR-0004 / inv 2, the FULL matrix (docs/procedures/TESTING.md mandated
// stimulus): the impulse must land at exactly maxLookahead + osLatency for
// EVERY factor x phase, and the predictor must agree — reported == measured
// across the whole surface, including Force-Max-offline.
static void testOsLatencyMatrix()
{
    const double sr = 48000.0;
    for (int f = 0; f <= 4; ++f)                 // 0 = Off, 1..4 = 2x..16x
        for (int ph = 0; ph < 2; ++ph)
        {
            anabasis::AnabasisEngine engine;
            engine.prepare (sr, 512, 2);
            anabasis::EngineParameters p;
            p.oversample = (anabasis::OversampleFactor) f;
            p.osPhase    = (anabasis::OsPhaseMode) ph;
            p.truePeakMode = false;              // impulse-position measurement

            const int expected = anabasis::predictLatencySamples (p, sr);
            juce::AudioBuffer<float> buf (2, 512);
            int peakAt = -1; float peakVal = 0.0f;
            for (int b = 0; b < 6; ++b)
            {
                buf.clear();
                if (b == 0) { buf.setSample (0, 0, 0.5f); buf.setSample (1, 0, 0.5f); }
                engine.process (buf, p);
                for (int n = 0; n < 512; ++n)
                    if (std::abs (buf.getSample (0, n)) > peakVal)
                    { peakVal = std::abs (buf.getSample (0, n)); peakAt = b * 512 + n; }
            }
            // Linear phase: EXACT — a symmetric FIR's peak is its group
            // delay, so any deviation is a padding/table bug. Min phase:
            // ±1 sample — an IIR cascade's group delay is frequency-
            // dependent BY DESIGN, its impulse peak sits within a sample of
            // the nominal (integer-compensated) bulk delay the table
            // reports (measured: exact at 4x, one late at 2x/8x/16x).
            if (ph == 1 || f == 0)
                check (peakAt == expected,
                       "osMatrix: impulse lands at exactly the reported latency (linear)");
            else
                check (std::abs (peakAt - expected) <= 1,
                       "osMatrix: min-phase impulse peak within 1 sample of the nominal delay");
            // The engine's own prepare-time cross-check of the Latency.h table
            // against what JUCE actually built — recorded unconditionally, so
            // this catches a pin bump in the Release builds CI runs, where the
            // jassert beside it compiles away.
            check (engine.latencyTableMatchesJuce(),
                   "osMatrix: the Latency.h table equals the pinned JUCE's own reported latency");
        }

    {   // Force-Max offline: reported and measured both use the FORCED 16x
        anabasis::AnabasisEngine engine;
        engine.prepare (sr, 512, 2);
        anabasis::EngineParameters p;
        p.oversample      = anabasis::OversampleFactor::x2;
        p.osPhase         = anabasis::OsPhaseMode::linear;
        p.forceMaxOffline = true;
        p.nonRealtime     = true;
        p.truePeakMode    = false;
        const int expected = anabasis::predictLatencySamples (p, sr);
        check (expected == anabasis::maxLookaheadSamples (sr)
                           + anabasis::osLatencySamples (anabasis::OversampleFactor::x16,
                                                         anabasis::OsPhaseMode::linear, sr),
               "osMatrix: Force-Max offline predicts with the forced 16x factor");
        juce::AudioBuffer<float> buf (2, 512);
        int peakAt = -1; float peakVal = 0.0f;
        for (int b = 0; b < 6; ++b)
        {
            buf.clear();
            if (b == 0) { buf.setSample (0, 0, 0.5f); buf.setSample (1, 0, 0.5f); }
            engine.process (buf, p);
            for (int n = 0; n < 512; ++n)
                if (std::abs (buf.getSample (0, n)) > peakVal)
                { peakVal = std::abs (buf.getSample (0, n)); peakAt = b * 512 + n; }
        }
        check (peakAt == expected, "osMatrix: Force-Max offline measures at the forced factor too");
    }
}

// ---------------------------------------------------------------------------
// inv 7's second half survives oversampling: bypass reads the base-rate dry
// ring at allowance + osLatency, so it stays a BIT-EXACT null at every
// factor — the oversampler never touches the dry path.
static void testBypassNullUnderOs()
{
    const double sr = 48000.0;
    anabasis::AnabasisEngine engine;
    engine.prepare (sr, 512, 2);
    anabasis::EngineParameters p;
    p.bypass     = true;
    p.oversample = anabasis::OversampleFactor::x4;
    p.osPhase    = anabasis::OsPhaseMode::linear;
    p.limGainDb  = 18.0f;                        // wet path would be loud
    const int delay = anabasis::predictLatencySamples (p, sr);

    std::vector<float> inL, outL;
    juce::AudioBuffer<float> buf (2, 512);
    uint32_t rng = 0xBEEF1234u;
    for (int b = 0; b < 20; ++b)
    {
        for (int n = 0; n < 512; ++n)
        {
            rng = rng * 1664525u + 1013904223u;
            const float v = ((float) (rng >> 8) / 8388608.0f - 1.0f) * 0.5f;
            buf.setSample (0, n, v); buf.setSample (1, n, v);
            inL.push_back (v);
        }
        engine.process (buf, p);
        for (int n = 0; n < 512; ++n)
            outL.push_back (buf.getSample (0, n));
    }
    // Crossfade settles within 10 ms; compare from 2x delay onward.
    bool exact = true;
    for (size_t n = (size_t) (2 * delay); n < outL.size(); ++n)
        if (! juce::exactlyEqual (outL[n], inL[n - (size_t) delay])) { exact = false; break; }
    check (exact, "osBypass: bypass is a bit-exact null at 4x linear (dry path never oversampled)");
}

// ---------------------------------------------------------------------------
// Transparency sanity: defaults + oversampling engaged = the up/down cascade
// alone. Not bit-exact (filters never are) — the assertion is an error floor,
// measured and recorded.
static void testOsTransparency()
{
    const double sr = 48000.0;
    anabasis::AnabasisEngine engine;
    engine.prepare (sr, 512, 2);
    anabasis::EngineParameters p;
    p.oversample = anabasis::OversampleFactor::x4;
    p.osPhase    = anabasis::OsPhaseMode::linear;
    const int delay = anabasis::predictLatencySamples (p, sr);

    std::vector<float> inL, outL;
    juce::AudioBuffer<float> buf (2, 512);
    for (int b = 0; b < 40; ++b)
    {
        for (int n = 0; n < 512; ++n)
        {
            const float v = 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi
                                              * 1000.0f * (float) (b * 512 + n) / (float) sr);
            buf.setSample (0, n, v); buf.setSample (1, n, v);
            inL.push_back (v);
        }
        engine.process (buf, p);
        for (int n = 0; n < 512; ++n)
            outL.push_back (buf.getSample (0, n));
    }
    double errSq = 0.0, refSq = 0.0;
    for (size_t n = 10000; n < outL.size(); ++n)
    {
        const double e = (double) outL[n] - inL[n - (size_t) delay];
        errSq += e * e;
        refSq += (double) inL[n - (size_t) delay] * inL[n - (size_t) delay];
    }
    const double errDb = 10.0 * std::log10 (errSq / refSq);
    check (errDb < -60.0, "osTransparency: 4x linear round trip error under -60 dB on a 1 kHz tone");
}

// ---------------------------------------------------------------------------
// inv 4 with the region oversampled: down-filter ringing after the limiter
// could overshoot, and the clamp is downstream at base rate — so the promise
// holds at every factor.
static void testCeilingUnderOs()
{
    const double sr = 48000.0;
    for (int f : { 1, 2 })
    {
        anabasis::AnabasisEngine engine;
        engine.prepare (sr, 512, 2);
        anabasis::EngineParameters p;
        p.oversample  = (anabasis::OversampleFactor) f;
        p.limGainDb   = 12.0f;
        p.clipDriveDb = 6.0f;
        // Pinned for the same reason as `testOutputNeverExceedsCeiling`: the
        // property is "down-filter ringing after the limiter cannot get past
        // the base-rate clamp", which is a statement about the clamp at a
        // known ceiling, not about the shipped default.
        p.ceilingDbTp  = -1.0f;
        p.truePeakMode = false;
        const float ceilingLin = std::pow (10.0f, p.ceilingDbTp / 20.0f);
        juce::AudioBuffer<float> buf (2, 512);
        float maxOut = 0.0f;
        for (int b = 0; b < 60; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                const double t = (b * 512 + n) / sr;
                const float v = 0.7f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 97.0 * t)
                              + 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 4200.0 * t);
                buf.setSample (0, n, v); buf.setSample (1, n, v);
            }
            engine.process (buf, p);
            maxOut = juce::jmax (maxOut, buf.getMagnitude (0, 512));
        }
        check (maxOut <= ceilingLin * 1.0001f, "osCeiling: the clamp holds with the region oversampled");
        check (maxOut > 0.5f * ceilingLin,     "osCeiling: the limiter is engaged");
    }
}

// ---------------------------------------------------------------------------
// inv 5's measurement: the SAME driven-clipper stimulus as the ADAA test,
// with 4x oversampling vs Off — the folded harmonics drop further (numbers
// recorded per C2).
static void testOsReducesAliasing()
{
    const double sr = 48000.0;
    const int N = 8192, warm = 2048, k = 2000;
    auto render = [&] (int factor)
    {
        anabasis::AnabasisEngine engine;
        engine.prepare (sr, 512, 2);
        anabasis::EngineParameters p;
        p.oversample  = (anabasis::OversampleFactor) factor;
        p.osPhase     = anabasis::OsPhaseMode::linear;
        p.clipDriveDb = 12.0f;
        p.clipShape   = 0.0f;
        p.ceilingDbTp = 0.0f;                     // keep the clamp out of the picture
        p.truePeakMode = false;
        std::vector<float> out;
        juce::AudioBuffer<float> buf (2, 512);
        int produced = 0;
        for (int b = 0; produced < warm + N; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                const int t = b * 512 + n;
                buf.setSample (0, n, 0.35f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                       * (float) k * (float) t / (float) N));
                buf.setSample (1, n, buf.getSample (0, n));
            }
            engine.process (buf, p);
            for (int n = 0; n < 512 && produced < warm + N; ++n, ++produced)
                if (produced >= warm)
                    out.push_back (buf.getSample (0, n));
        }
        juce::dsp::FFT fft (13);
        std::vector<float> fbuf (2 * (size_t) N, 0.0f);
        std::copy (out.begin(), out.end(), fbuf.begin());
        fft.performRealOnlyForwardTransform (fbuf.data(), true);
        auto mag = [&] (int bin)
        {
            const float re = fbuf[(size_t) (2 * bin)], im = fbuf[(size_t) (2 * bin + 1)];
            return 20.0f * std::log10 (juce::jmax (1.0e-9f, std::sqrt (re * re + im * im)));
        };
        return std::make_pair (mag (N - 3 * k), mag (k));   // folded 3rd, fundamental
    };

    auto [aliasOff, fundOff] = render (0);
    auto [alias4x,  fund4x ] = render (2);
    check (alias4x < aliasOff - 20.0f,
           "osAliasing: 4x drops the folded 3rd by >20 dB beyond ADAA alone");   // measured 74 dB
    // The fundamental RISES ~1.3 dB at 4x: ADAA-1's sinc droop at 11.72 kHz
    // (~0.9 dB at base rate) nearly vanishes at 192 kHz — a real, correct
    // effect of oversampling the nonlinearity, not an error.
    check (std::abs (fund4x - fundOff) < 2.5f, "osAliasing: the fundamental is preserved");
}

// ---------------------------------------------------------------------------
// §4.5 dither: Off is a true no-op (the null test already proves it); 16-bit
// lands every sample on the 2^-15 grid with TPDF noise present; shaping
// pushes the quantisation error's energy toward the top of the band.
static void testDitherModes()
{
    const double sr = 48000.0;
    auto render = [&] (int mode, bool shaping)
    {
        anabasis::AnabasisEngine engine;
        engine.prepare (sr, 512, 2);
        anabasis::EngineParameters p;
        p.ditherMode    = mode;
        p.ditherShaping = shaping;
        std::vector<float> out;
        juce::AudioBuffer<float> buf (2, 512);
        for (int b = 0; b < 20; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                const float v = 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                  * 441.0f * (float) (b * 512 + n) / (float) sr);
                buf.setSample (0, n, v); buf.setSample (1, n, v);
            }
            engine.process (buf, p);
            for (int n = 0; n < 512; ++n)
                out.push_back (buf.getSample (0, n));
        }
        return out;
    };

    {   // 16-bit: on-grid, and genuinely dithered (not just truncated)
        const auto out = render (1, false);
        const float q = 3.0517578125e-5f;
        bool onGrid = true; bool anyOff = false;
        for (size_t n = 1000; n < out.size(); ++n)
        {
            const float k = out[n] / q;
            if (std::abs (k - std::nearbyint (k)) > 1.0e-3f) onGrid = false;
            // dithered quantisation differs from PLAIN rounding somewhere:
            if (! anyOff)
            {
                // compare against the undithered input's rounded value
                // (dither randomises the LSB, so some samples must differ)
                const float in = 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                   * 441.0f * (float) (n - 480) / (float) sr);
                if (! juce::exactlyEqual (out[n], q * std::nearbyint (in / q)))
                    anyOff = true;
            }
        }
        check (onGrid, "dither16: every output sample sits on the 2^-15 grid");
        check (anyOff, "dither16: the LSB is randomised, not plain rounding");
    }
    {   // shaping tilts the error spectrum upward
        auto errSpectrumSplit = [&] (bool shaping)
        {
            const auto out = render (2, shaping);
            const int N = 8192;
            juce::dsp::FFT fft (13);
            std::vector<float> fbuf (2 * (size_t) N, 0.0f);
            // error = out - ideal (delay-aligned input), 24-bit error is tiny:
            for (int n = 0; n < N; ++n)
            {
                const int idx = 1000 + n;
                const float in = 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                   * 441.0f * (float) (idx - 480) / (float) sr);
                fbuf[(size_t) n] = out[(size_t) idx] - in;
            }
            fft.performRealOnlyForwardTransform (fbuf.data(), true);
            double lo = 0.0, hi = 0.0;
            for (int bin = 16; bin < N / 8; ++bin)
            { const float re = fbuf[(size_t)(2*bin)], im = fbuf[(size_t)(2*bin+1)]; lo += re*re + im*im; }
            for (int bin = 3 * N / 8; bin < N / 2 - 16; ++bin)
            { const float re = fbuf[(size_t)(2*bin)], im = fbuf[(size_t)(2*bin+1)]; hi += re*re + im*im; }
            return 10.0 * std::log10 (hi / lo);
        };
        const double flat = errSpectrumSplit (false), shaped = errSpectrumSplit (true);
        check (shaped > flat + 6.0, "ditherShaping: error energy moves to the top of the band");
    }
}

// ---------------------------------------------------------------------------
// §2.8 / DSP_POLICY invariant 8: a discrete rewire is wrapped by the duck —
// the output dips to silence on the ~6 ms raised cosine, the rewire executes
// at the bottom, and the ~28 ms recovery leg brings it back. The measured
// property is SMOOTHNESS: no per-sample step beyond what the signal's own
// slope plus the duck's slope allow. An unducked eqPosition flip with a
// +12 dB shelf steps the output by an order of magnitude more.
static void testDuckWrapsDiscreteRewires()
{
    const double sr = 48000.0;
    anabasis::AnabasisEngine engine;
    engine.prepare (sr, 512, 2);
    anabasis::EngineParameters p;
    p.eqLowShelfGainDb = 12.0f;
    p.eqLowShelfFreqHz = 400.0f;
    p.eqPosition       = 0;
    p.truePeakMode     = false;

    std::vector<float> out;
    juce::AudioBuffer<float> buf (2, 512);
    const int flipAtBlock = 20;
    for (int b = 0; b < 40; ++b)
    {
        if (b == flipAtBlock)
            p.eqPosition = 1;                       // Pre → Post, a genuine rewire
        for (int n = 0; n < 512; ++n)
        {
            const float v = 0.4f * std::sin (2.0f * juce::MathConstants<float>::pi
                                             * 200.0f * (float) (b * 512 + n) / (float) sr);
            buf.setSample (0, n, v); buf.setSample (1, n, v);
        }
        engine.process (buf, p);
        for (int n = 0; n < 512; ++n)
            out.push_back (buf.getSample (0, n));
    }

    const size_t flipAt = (size_t) flipAtBlock * 512;
    // (a) smooth: 200 Hz sine at the ducked level moves ≤ ~0.02/sample; the
    // duck adds ≤ ~0.01; an unducked +12 dB rewire steps several times that.
    float maxDelta = 0.0f;
    for (size_t n = flipAt; n < flipAt + 4000; ++n)
        maxDelta = juce::jmax (maxDelta, std::abs (out[n] - out[n - 1]));
    check (maxDelta < 0.045f, "duck: the rewire never steps — the envelope is band-limited");

    // (b) the dip exists (the rewire really waited for silence)...
    float minEnv = 1.0f;
    for (size_t n = flipAt; n < flipAt + 2000; n += 60)
    {
        float peak = 0.0f;
        for (size_t k = n; k < n + 240 && k < out.size(); ++k)
            peak = juce::jmax (peak, std::abs (out[k]));
        minEnv = juce::jmin (minEnv, peak);
    }
    check (minEnv < 0.02f, "duck: the output reaches the silent bottom");

    // (c) ...and it recovers.
    float tailPeak = 0.0f;
    for (size_t n = out.size() - 2400; n < out.size(); ++n)
        tailPeak = juce::jmax (tailPeak, std::abs (out[n]));
    check (tailPeak > 0.3f, "duck: the output recovers after the rewire");
}

// ---------------------------------------------------------------------------
// The OS factor latch rides the same duck: flipping 0 → 4x mid-stream dips,
// latches at the bottom (the region state reset happens at zero gain), and
// recovers at the new factor — finite, at level, and still under the ceiling.
static void testDuckWrapsOsLatch()
{
    const double sr = 48000.0;
    anabasis::AnabasisEngine engine;
    engine.prepare (sr, 512, 2);
    anabasis::EngineParameters p;
    p.limGainDb    = 6.0f;
    p.truePeakMode = false;

    std::vector<float> out;
    juce::AudioBuffer<float> buf (2, 512);
    for (int b = 0; b < 40; ++b)
    {
        if (b == 20)
        {
            p.oversample = anabasis::OversampleFactor::x4;
            p.osPhase    = anabasis::OsPhaseMode::linear;
        }
        for (int n = 0; n < 512; ++n)
        {
            const float v = 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi
                                             * 500.0f * (float) (b * 512 + n) / (float) sr);
            buf.setSample (0, n, v); buf.setSample (1, n, v);
        }
        engine.process (buf, p);
        for (int n = 0; n < 512; ++n)
            out.push_back (buf.getSample (0, n));
    }

    float minEnv = 1.0f, tailPeak = 0.0f, maxDelta = 0.0f;
    for (size_t n = 20 * 512; n < 20 * 512 + 2000; n += 60)
    {
        float peak = 0.0f;
        for (size_t k = n; k < n + 240; ++k)
            peak = juce::jmax (peak, std::abs (out[k]));
        minEnv = juce::jmin (minEnv, peak);
    }
    // The delta window must span the WHOLE transition, in-leg included. It
    // used to stop at 22*512 = 11264 while the recovery's first real sample
    // landed at ~11293 — thirty samples past the last one measured, which is
    // how a −9 dB splice sat here unnoticed.
    for (size_t n = 20 * 512; n < 27 * 512; ++n)
        maxDelta = juce::jmax (maxDelta, std::abs (out[n] - out[n - 1]));
    for (size_t n = out.size() - 2400; n < out.size(); ++n)
        tailPeak = juce::jmax (tailPeak, std::abs (out[n]));
    bool allFinite = true;
    for (float v : out) if (! std::isfinite (v)) { allFinite = false; break; }

    // THE refill property, measured where it lives. A latch empties the
    // lookahead ring and resets the oversampler, so the processed path is
    // exactly silent for delaySamples + osLatBase (480 + 61) samples
    // afterwards. Find the first sample that is not exactly zero after the
    // latch and look at the cycle that follows it: if the in-leg started at
    // the latch, that audio arrives at duckGain ≈ 0.35 (541 samples into the
    // 1344-sample ramp) and the cycle peaks near 0.34; if the bottom is held
    // until the pipeline refills, the ramp is at its own beginning there and
    // the cycle peaks near 0.01. Disjoint by a factor of ~30.
    size_t firstAudible = out.size();
    for (size_t n = 21 * 512; n < out.size(); ++n)
        if (std::abs (out[n]) > 1.0e-7f) { firstAudible = n; break; }
    float onsetPeak = 0.0f;
    for (size_t n = firstAudible; n < juce::jmin (firstAudible + 128, out.size()); ++n)
        onsetPeak = juce::jmax (onsetPeak, std::abs (out[n]));

    check (minEnv < 0.02f,   "duckOs: the latch waits for the silent bottom");
    check (maxDelta < 0.07f, "duckOs: the factor switch never steps the output");
    check (firstAudible < out.size(),
           "duckOs: the processed path does come back (the onset check is not vacuous)");
    check (onsetPeak < 0.05f,
           "duckOs: the recovery starts from the REFILLED pipeline, not partway up the ramp");
    check (tailPeak > 0.35f, "duckOs: the stream recovers at the new factor");
    check (allFinite,        "duckOs: no garbage crosses the latch");
}

// ---------------------------------------------------------------------------
// §2.8: a duck request that lands while the OUT leg is still running must not
// evaporate — the bulk swap it guards reaches the snapshot a block later and
// has to find the engine at zero gain. Blocks of 128 samples so the ~6 ms
// out-leg (288 samples) spans several of them and the request can be consumed
// in the `out` state at all; at 512 the bottom is always reached in the same
// block that starts the fade.
static void testDuckRequestDuringOutIsHeld()
{
    const double sr = 48000.0;
    auto firstAudibleAfter = [&] (bool secondRequest) -> size_t
    {
        anabasis::AnabasisEngine engine;
        engine.prepare (sr, 128, 2);
        anabasis::EngineParameters p;
        p.truePeakMode = false;
        std::vector<float> out;
        juce::AudioBuffer<float> buf (2, 128);
        for (int b = 0; b < 60; ++b)
        {
            if (b == 10)                       // fade begins at block 10's top
                engine.requestForcedDuck();
            if (secondRequest && b == 11)      // consumed at block 11's top: still OUT
                engine.requestForcedDuck();
            for (int n = 0; n < 128; ++n)
            {
                const float v = 0.4f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                 * 300.0f * (float) (b * 128 + n) / (float) sr);
                buf.setSample (0, n, v); buf.setSample (1, n, v);
            }
            engine.process (buf, p);
            for (int n = 0; n < 128; ++n)
                out.push_back (buf.getSample (0, n));
        }
        size_t silentAt = out.size();
        for (size_t n = 10 * 128; n < out.size(); ++n)      // the bottom
            if (juce::exactlyEqual (out[n], 0.0f)) { silentAt = n; break; }
        for (size_t n = silentAt; n < out.size(); ++n)      // ...and the recovery
            if (! juce::exactlyEqual (out[n], 0.0f)) return n;
        return out.size();
    };

    const size_t plain = firstAudibleAfter (false), held = firstAudibleAfter (true);
    check (plain < 60 * 128, "duckOut: the control run recovers (the comparison is not vacuous)");
    check (held >= plain + 100,
           "duckOut: a request during the out-leg buys a held bottom block, not nothing");
}

// ---------------------------------------------------------------------------
// §2.7/§2.8 together: delta monitoring is a PROCESSED-path function, so the
// duck must cover it. With a transparent chain the delta output is exact
// silence; during a transition it must STAY silence. Subtracting a ducked
// processed term from an unducked dry one did the opposite — the delta leg
// rose to the full dry signal exactly while the transition was meant to be
// inaudible, the loudest possible artefact from the layer that exists to
// prevent them.
static void testDeltaIsCoveredByTheDuck()
{
    const double sr = 48000.0;
    auto render = [&] (float pushDb, bool duckAt10) -> std::vector<float>
    {
        anabasis::AnabasisEngine engine;
        engine.prepare (sr, 512, 2);
        anabasis::EngineParameters p;
        p.deltaMonitor = true;
        p.limGainDb    = pushDb;
        p.truePeakMode = false;
        std::vector<float> out;
        juce::AudioBuffer<float> buf (2, 512);
        for (int b = 0; b < 30; ++b)
        {
            if (duckAt10 && b == 10)
                engine.requestForcedDuck();
            for (int n = 0; n < 512; ++n)
            {
                const float v = 0.4f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                 * 300.0f * (float) (b * 512 + n) / (float) sr);
                buf.setSample (0, n, v); buf.setSample (1, n, v);
            }
            engine.process (buf, p);
            for (int n = 0; n < 512; ++n)
                out.push_back (buf.getSample (0, n));
        }
        return out;
    };

    const auto transparent = render (0.0f, true);
    float peakThroughDuck = 0.0f;
    for (size_t n = 10 * 512; n < 16 * 512; ++n)
        peakThroughDuck = juce::jmax (peakThroughDuck, std::abs (transparent[n]));
    check (peakThroughDuck < 1.0e-4f,
           "delta+duck: the difference signal stays silent through a transition");

    // Guard against a vacuous pass: with the chain actually working, delta is
    // the removed material and is plainly nonzero.
    const auto pushed = render (9.0f, false);
    float peakPushed = 0.0f;
    for (size_t n = 20 * 512; n < 24 * 512; ++n)
        peakPushed = juce::jmax (peakPushed, std::abs (pushed[n]));
    check (peakPushed > 0.05f, "delta+duck: delta is not silent when the chain removes material");
}

// ---------------------------------------------------------------------------
// §5.4 restore transport: two session loads between audio blocks must leave
// the engine holding the LAST one. The earlier two-flag form consumed "forget"
// before "restore" unconditionally, so an un-learned session loaded after a
// learned one inherited the learned references — and the next save wrote them
// back out.
static void testAdaptiveRestoreLastStagedWins()
{
    const double sr = 48000.0;
    auto runOrder = [&] (bool learnedLast)
    {
        anabasis::AnabasisEngine engine;
        engine.prepare (sr, 512, 2);
        anabasis::EngineParameters p;
        juce::AudioBuffer<float> buf (2, 512);
        buf.clear();
        engine.process (buf, p);                        // prime
        if (learnedLast) { engine.restoreNeverLearned(); engine.restoreLearnedTargets (9.0f, -2.0f); }
        else             { engine.restoreLearnedTargets (9.0f, -2.0f); engine.restoreNeverLearned(); }
        engine.process (buf, p);                        // both staged, one block top
        return std::make_pair (engine.adaptiveForWrapper().hasLearned(),
                               engine.adaptiveForWrapper().publishedRefOnset());
    };

    const auto neverLast  = runOrder (false);
    const auto learnedNow = runOrder (true);
    check (! neverLast.first
             && juce::approximatelyEqual (neverLast.second,
                                          anabasis::AdaptiveEngine::kDefaultRefOnset),
           "adaptiveRestore: an un-learned session loaded last wins over a learned one");
    check (learnedNow.first && std::abs (learnedNow.second - 9.0f) < 1.0e-4f,
           "adaptiveRestore: a learned session loaded last wins over an un-learned one");
}

// ---------------------------------------------------------------------------
// Detector state that is not advanced while its path is off must not be
// re-entered when the path comes back: the true-peak estimator's 12-tap
// history freezes whenever tpMode is false (the OS factor flips it), which
// is observable as gain reduction on silence — impossible from a clean
// detector. (The detector HPF's half of this test left with the filter
// itself, 0.1.2 / ADR-0023: the limiter's detector is unfiltered now, so
// there is no biquad delay line to re-enter.)
static void testStaleDetectorStateIsNotReentered()
{
    const double sr = 48000.0;
    // Differential, so the envelope's own asymptotic approach to unity (it
    // converges, it does not arrive) cannot be mistaken for the effect: the
    // SAME sequence is run with the charging passage present and replaced by
    // silence. Only stale state can separate them.
    auto minGainOnSilence = [&] (bool chargeIt)
    {
        anabasis::LookaheadLimiter lim;
        lim.prepare (sr, 480);
        lim.setAutoRelease (false);
        lim.setRelease (1.0f);              // so the envelope releases fast
        lim.setStereoLink (1.0f);
        lim.setTransientPreserve (0.0f);
        lim.setTruePeakMode (true);

        float g[2] = { 1.0f, 1.0f };
        const float ceiling = 0.5f;
        // 1) charge the stale state with a loud passage while the path is ON.
        //    4200 samples of 60 Hz at 48 kHz is 5.25 periods — the passage
        //    ends ON THE CREST, so the twelve taps that freeze are all near
        //    ±4.0. The first draft used 4000 (5.00 periods) and froze the
        //    history at a zero crossing: both mutants survived, because there
        //    was nothing in the stale state worth resurrecting. The stimulus
        //    has to put the property where the assertion looks.
        for (int n = 0; n < 4200; ++n)
        {
            const float v = chargeIt ? 4.0f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                        * 60.0f * (float) n / (float) sr)
                                     : 0.0f;
            const float fr[2] = { v, v };
            lim.processSample (fr, 2, 480, ceiling, g);
        }
        // 2) command the path OFF (the TP mode flips instantly — a bool — so
        //    its history freezes at the crest), then silence: the envelope
        //    releases to unity while the frozen state (if any survived the
        //    off edge) keeps its charge.
        lim.setTruePeakMode (false);
        for (int n = 0; n < 8000; ++n)
        {
            const float fr[2] = { 0.0f, 0.0f };
            lim.processSample (fr, 2, 480, ceiling, g);
        }
        // 3) turn it back ON and keep feeding silence: a clean detector sees
        //    exactly zero, a stale one interpolates the old passage.
        lim.setTruePeakMode (true);
        float minGain = 1.0f;
        for (int n = 0; n < 600; ++n)
        {
            const float fr[2] = { 0.0f, 0.0f };
            lim.processSample (fr, 2, 480, ceiling, g);
            minGain = juce::jmin (minGain, g[0], g[1]);
        }
        return minGain;
    };

    const float tpClean = minGainOnSilence (false), tpStale = minGainOnSilence (true);
    // Bound reasoning, measured rather than assumed: a never-charged run
    // returns EXACTLY 1.0, a charged one stalls at 0.99999857 — the one-pole
    // release's float floor (once (1−env)·a drops below half an ULP near
    // unity the addition rounds away, so the limiter never returns to bit-
    // exact unity after any reduction; −0.00001 dB, recorded, not chased).
    // The defect being tested is nothing like that size: a stale 4.0 tap
    // against a 0.5 ceiling pins the gain near 0.125. 0.999 sits between the
    // two by four orders of magnitude.
    check (tpClean > 0.9999f,
           "staleDetector: silence into a never-charged detector is unity gain (the baseline)");
    check (tpStale > 0.999f,
           "staleDetector: re-enabling true-peak mode does not resurrect the frozen tap history");
}

// ---------------------------------------------------------------------------
// Invariant 8 at the limiter's own control boundary: stereo link and
// transient preserve are LEVEL-affecting (link blends the detector level,
// preserve selects the attack alpha), so a per-block step in either must
// glide, not jump. The release/style/autoRelease setters stay unsmoothed by
// design — there the envelope IS the smoother. (The detector HPF's third of
// this test left with the filter, 0.1.2 / ADR-0023.)
static void testLimiterControlSmoothing()
{
    const double sr = 48000.0;

    {   // LINK: ch0 loud (limiting), ch1 quiet. link 0 → ch1 rides its own
        // level (gain 1); link 1 → ch1 takes ch0's reduction (~0.55). The
        // step happens through the DOWNWARD (attack, instant at preserve 0)
        // direction, so without smoothing the whole 0.44 change lands in ONE
        // sample; smoothed, the per-sample delta is the 20 ms ramp slope.
        anabasis::LookaheadLimiter lim;
        lim.prepare (sr, 480);
        lim.setAutoRelease (false);
        lim.setRelease (200.0f);
        lim.setTransientPreserve (0.0f);
        lim.setTruePeakMode (false);
        lim.setStereoLink (0.0f);

        float g[2] = { 1.0f, 1.0f };
        const float fr[2] = { 0.9f, 0.1f };            // steady magnitudes
        for (int n = 0; n < 3000; ++n)
            lim.processSample (fr, 2, 480, 0.5f, g);
        const float before = g[1];

        lim.setStereoLink (1.0f);                      // the step under test
        float maxDelta = 0.0f, prev = g[1];
        for (int n = 0; n < 3000; ++n)
        {
            lim.processSample (fr, 2, 480, 0.5f, g);
            maxDelta = juce::jmax (maxDelta, std::abs (g[1] - prev));
            prev = g[1];
        }
        check (before > 0.99f,                "limSmooth/link: unlinked quiet channel rides at unity");
        check (std::abs (g[1] - 0.5f / 0.9f) < 0.01f,
               "limSmooth/link: the change ARRIVES (fully linked gain — not a frozen control)");
        check (maxDelta < 0.01f,
               "limSmooth/link: a full-scale link step glides (unsmoothed = 0.44 in one sample)");
    }

    {   // PRESERVE: primed at 0 (instant attack), stepped to 1 immediately
        // before a transient. Smoothed, the transient still meets an ~instant
        // attack (the glide has only advanced one step); unsmoothed, aAtk
        // jumps to the 1.5 ms pole at once and the gain walks down slowly.
        // min gain over the first 8 loud samples: ~0.5 smoothed vs ~0.9
        // unsmoothed — disjoint by design, not by tolerance.
        anabasis::LookaheadLimiter lim;
        lim.prepare (sr, 480);
        lim.setAutoRelease (false);
        lim.setRelease (200.0f);
        lim.setStereoLink (1.0f);
        lim.setTruePeakMode (false);
        lim.setTransientPreserve (0.0f);

        float g[2] = { 1.0f, 1.0f };
        const float quiet[2] = { 0.0f, 0.0f };
        for (int n = 0; n < 1000; ++n)
            lim.processSample (quiet, 2, 480, 0.5f, g);   // primes; envelope at unity

        lim.setTransientPreserve (1.0f);               // the step under test
        const float loud[2] = { 1.0f, 1.0f };
        float minGain = 1.0f;
        for (int n = 0; n < 8; ++n)
        {
            lim.processSample (loud, 2, 480, 0.5f, g);
            minGain = juce::jmin (minGain, g[0]);
        }
        check (minGain < 0.65f,
               "limSmooth/preserve: a step to full preserve cannot blunt the NEXT transient's attack");
    }

}

// ---------------------------------------------------------------------------
// The wrapper's forced-duck request (requestForcedDuck before an A/B swap /
// preset apply / session load) produces the same envelope with NO discrete
// engine rewire — the duck is the mask the smoothed bulk glide happens under.
static void testDuckOnWrapperRequest()
{
    const double sr = 48000.0;
    anabasis::AnabasisEngine engine;
    engine.prepare (sr, 512, 2);
    anabasis::EngineParameters p;
    p.truePeakMode = false;

    std::vector<float> out;
    juce::AudioBuffer<float> buf (2, 512);
    for (int b = 0; b < 30; ++b)
    {
        if (b == 10)
            engine.requestForcedDuck();
        for (int n = 0; n < 512; ++n)
        {
            const float v = 0.4f * std::sin (2.0f * juce::MathConstants<float>::pi
                                             * 300.0f * (float) (b * 512 + n) / (float) sr);
            buf.setSample (0, n, v); buf.setSample (1, n, v);
        }
        engine.process (buf, p);
        for (int n = 0; n < 512; ++n)
            out.push_back (buf.getSample (0, n));
    }

    float minEnv = 1.0f, tailPeak = 0.0f;
    for (size_t n = 10 * 512; n < 10 * 512 + 2000; n += 60)
    {
        float peak = 0.0f;
        for (size_t k = n; k < n + 240; ++k)
            peak = juce::jmax (peak, std::abs (out[k]));
        minEnv = juce::jmin (minEnv, peak);
    }
    for (size_t n = out.size() - 2400; n < out.size(); ++n)
        tailPeak = juce::jmax (tailPeak, std::abs (out[n]));
    check (minEnv < 0.02f,  "duckReq: a wrapper request alone dips to the bottom");
    check (tailPeak > 0.3f, "duckReq: and recovers on the 28 ms leg");
}

// ---------------------------------------------------------------------------
// §2.8: a duck request that lands DURING the bottom block is not dropped — the
// bottom is held one more block so the swap that request guards is adopted at
// zero gain. Run A ducks once; run B issues a second request while the engine
// sits at the bottom. Run B's next block must be exact silence (bottom held)
// where run A's is already recovering, and run B must still recover after.
static void testDuckRequestDuringBottomExtendsBottom()
{
    const double sr = 48000.0;
    auto render = [&] (bool secondRequest) -> std::vector<float>
    {
        anabasis::AnabasisEngine engine;
        engine.prepare (sr, 512, 2);
        anabasis::EngineParameters p;
        p.truePeakMode = false;
        std::vector<float> out;
        juce::AudioBuffer<float> buf (2, 512);
        for (int b = 0; b < 30; ++b)
        {
            if (b == 10)
                engine.requestForcedDuck();          // reaches bottom inside block 10 (~6 ms out)
            if (secondRequest && b == 11)
                engine.requestForcedDuck();          // consumed at block 11's top: state == bottom
            for (int n = 0; n < 512; ++n)
            {
                const float v = 0.4f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                 * 300.0f * (float) (b * 512 + n) / (float) sr);
                buf.setSample (0, n, v); buf.setSample (1, n, v);
            }
            engine.process (buf, p);
            for (int n = 0; n < 512; ++n)
                out.push_back (buf.getSample (0, n));
        }
        return out;
    };

    const auto a = render (false), b = render (true);
    float aBlk11 = 0.0f, bBlk11 = 0.0f, bTail = 0.0f;
    for (size_t n = 11 * 512; n < 12 * 512; ++n)
    {
        aBlk11 = juce::jmax (aBlk11, std::abs (a[n]));
        bBlk11 = juce::jmax (bBlk11, std::abs (b[n]));
    }
    for (size_t n = b.size() - 2400; n < b.size(); ++n)
        bTail = juce::jmax (bTail, std::abs (b[n]));
    check (aBlk11 > 0.01f, "duckBottom: without a second request the recovery leg is already audible");
    check (juce::exactlyEqual (bBlk11, 0.0f),
           "duckBottom: a request landing during the bottom holds the NEXT block at exact silence");
    check (bTail > 0.3f, "duckBottom: and the held duck still recovers afterwards");
}

// ---------------------------------------------------------------------------
// inv 11 (P3): LUFS against the standard's own calibration points, synthesised
// exactly as BS.1770-4 defines them. The compliance sentence in the standard:
// "if a 0 dB FS 997 Hz sine wave is applied to the left, centre, or right
// channel input, the indicated loudness will equal −3.01 LKFS" — that single
// vector pins the K-filter gain, the −0.691 offset and the channel weighting
// at once. Contract <= 0.1 LU (DESIGN §2.9).
static float lufsOfSine (float freqHz, float ampL, float ampR, double seconds,
                         double sr, int which /*0=M 1=S 2=I*/)
{
    anabasis::LoudnessMeter m;
    m.prepare (sr);
    const int total = (int) (seconds * sr);
    for (int n = 0; n < total; ++n)
    {
        const float s = std::sin (2.0f * juce::MathConstants<float>::pi
                                  * freqHz * (float) n / (float) sr);
        const float fr[2] = { ampL * s, ampR * s };
        m.processFrame (fr, 2);
    }
    return which == 0 ? m.momentaryLufs() : which == 1 ? m.shortTermLufs()
                                                       : m.integratedLufs();
}

static void testLufsCalibration()
{
    const double sr = 48000.0;
    auto near = [] (float a, float b, float tol) { return std::abs (a - b) <= tol; };

    check (near (lufsOfSine (997.0f, 1.0f, 0.0f, 5.0, sr, 2), -3.01f, 0.1f),
           "lufs: 0 dBFS 997 Hz in ONE channel reads -3.01 LKFS (the standard's compliance point)");
    check (near (lufsOfSine (997.0f, 1.0f, 1.0f, 5.0, sr, 2), 0.0f, 0.1f),
           "lufs: the same tone in BOTH channels reads +3.01 higher");
    check (near (lufsOfSine (997.0f, 0.1f, 0.1f, 5.0, sr, 2), -20.0f, 0.1f),
           "lufs: -20 dBFS stereo tone reads -20 LUFS (linearity)");
    // K-weighting shape: 100 Hz sits ~ -0.3 dB below 1 kHz on the RLB slope's
    // tail, 10 kHz ~ +3.6 dB above it on the head shelf — assert the SIGNS
    // and rough magnitudes so a swapped stage or missing shelf fails.
    const float at100 = lufsOfSine (100.0f, 1.0f, 1.0f, 5.0, sr, 2);
    const float at10k = lufsOfSine (10000.0f, 1.0f, 1.0f, 5.0, sr, 2);
    check (at100 < -0.5f && at100 > -6.0f, "lufs: 100 Hz reads below 1 kHz (RLB high-pass tail)");
    check (at10k > 3.0f  && at10k < 5.0f,  "lufs: 10 kHz reads ~+4 dB above (head shelf)");

    // 44.1 kHz: the pre-warped design holds off the 48 kHz reference rate.
    check (near (lufsOfSine (997.0f, 1.0f, 0.0f, 5.0, 44100.0, 2), -3.01f, 0.1f),
           "lufs: the compliance point holds at 44.1 kHz (pre-warped design)");
}

// ---------------------------------------------------------------------------
// The two-stage gate, each half isolated:
// - absolute: trailing silence must not drag the integrated figure down;
// - relative: a long quiet tail ABOVE -70 but >10 LU below the programme is
//   gated out — ungated it would read ~-26, gated it stays at the programme.
static void testLufsGating()
{
    const double sr = 48000.0;
    auto near = [] (float a, float b, float tol) { return std::abs (a - b) <= tol; };

    {   // absolute gate: 5 s at -20 then 10 s of silence
        anabasis::LoudnessMeter m;
        m.prepare (sr);
        for (int n = 0; n < (int) (15.0 * sr); ++n)
        {
            const float a = n < (int) (5.0 * sr) ? 0.1f : 0.0f;
            const float s = a * std::sin (2.0f * juce::MathConstants<float>::pi
                                          * 997.0f * (float) n / (float) sr);
            const float fr[2] = { s, s };
            m.processFrame (fr, 2);
        }
        check (near (m.integratedLufs(), -20.0f, 0.15f),
               "gating: trailing silence is absolutely gated — integrated holds the programme");
    }
    {   // relative gate: 10 s at -20 then 30 s at -45 (above absolute, >10 LU below)
        anabasis::LoudnessMeter m;
        m.prepare (sr);
        for (int n = 0; n < (int) (40.0 * sr); ++n)
        {
            const float a = n < (int) (10.0 * sr) ? 0.1f : 0.0056234f;   // -45 dB
            const float s = a * std::sin (2.0f * juce::MathConstants<float>::pi
                                          * 997.0f * (float) n / (float) sr);
            const float fr[2] = { s, s };
            m.processFrame (fr, 2);
        }
        // Ungated mean would be ~ -26; the relative gate holds ~ -20.
        check (near (m.integratedLufs(), -20.0f, 0.3f),
               "gating: a -45 LUFS tail is relatively gated out of the integrated figure");
    }
    {   // The ABSOLUTE gate's distinct job: keeping silence out of the
        // relative threshold's BASE. 10 s at -20 + 20 s at -38 + 120 s of
        // silence. Correct: silence is absolutely gated, pass-1 mean ~ -24.8,
        // threshold -34.8, the -38 band is gated -> integrated -20. With the
        // absolute gate removed, ~1200 silence blocks drag the pass-1 mean to
        // ~ -31.7, the threshold to ~ -41.7, the -38 band survives and the
        // integrated figure reads ~ -24.7. (Found by mutation: with the first
        // two stimuli alone, the relative gate masked an absolute-gate
        // removal completely — silence sits below ANY plausible relative
        // threshold, so only its effect on the threshold's base is
        // observable.)
        anabasis::LoudnessMeter m;
        m.prepare (sr);
        for (int n = 0; n < (int) (150.0 * sr); ++n)
        {
            const double t = n / sr;
            const float a = t < 10.0 ? 0.1f : (t < 30.0 ? 0.0126f : 0.0f);
            const float s = a * std::sin (2.0f * juce::MathConstants<float>::pi
                                          * 997.0f * (float) n / (float) sr);
            const float fr[2] = { s, s };
            m.processFrame (fr, 2);
        }
        check (near (m.integratedLufs(), -20.0f, 0.3f),
               "gating: silence never enters the relative threshold's base (absolute gate)");
    }
}

// ---------------------------------------------------------------------------
// ADR-0020's two additions to this class, each against the same stimuli the
// gate tests above use, so a reader can compare the three answers directly.
static void testLoudnessRangeAndTheUngatedReading()
{
    const double sr = 48000.0;
    auto near = [] (float a, float b, float tol) { return std::abs (a - b) <= tol; };
    auto feed = [sr] (anabasis::LoudnessMeter& m, double seconds, float amp, double t0)
    {
        for (int n = 0; n < (int) (seconds * sr); ++n)
        {
            const double t = t0 * sr + n;
            const float s = amp * std::sin (2.0f * juce::MathConstants<float>::pi
                                            * 997.0f * (float) t / (float) sr);
            const float fr[2] = { s, s };
            m.processFrame (fr, 2);
        }
    };

    {   // A steady tone has NO range. The one reading that would pass by
        // accident if `lraLu()` returned its sentinel, so the count is checked
        // by the case below rather than by trusting a zero here.
        anabasis::LoudnessMeter m;
        m.prepare (sr);
        feed (m, 20.0, 0.1f, 0.0);
        check (near (m.lraLu(), 0.0f, 0.3f), "lra: a steady tone reads ~0 LU");
    }
    {   // 20 s at −20 LUFS then 20 s at −30: the 95th percentile sits in the
        // loud passage and the 10th in the quiet one, so LRA ≈ 10 LU. Both
        // passages clear the −20 LU relative gate (the energy mean is ≈ −22.6,
        // so the threshold is ≈ −42.6), which is what makes this a percentile
        // measurement rather than a gate measurement.
        anabasis::LoudnessMeter m;
        m.prepare (sr);
        feed (m, 20.0, 0.1f, 0.0);        // −20 LUFS
        feed (m, 20.0, 0.0316228f, 20.0); // −30 LUFS
        check (near (m.lraLu(), 10.0f, 1.0f), "lra: a 10 LU level step reads ~10 LU of range");
    }
    {   // The LRA relative gate is −20 LU, NOT the integrated reading's −10:
        // a passage 15 LU down survives here and would be gated out there. So
        // this stimulus separates the two constants — with −10 substituted the
        // quiet passage vanishes and LRA collapses toward 0.
        anabasis::LoudnessMeter m;
        m.prepare (sr);
        feed (m, 30.0, 0.1f, 0.0);         // −20 LUFS, the bulk of the programme
        feed (m, 10.0, 0.0177828f, 30.0);  // −35 LUFS, 15 LU down
        check (m.lraLu() > 10.0f, "lra: the -20 LU gate keeps a 15 LU-down passage in range");
    }
    {   // The ungated (BS.1770-1) reading against the gated one, on the
        // absolute gate's own stimulus: 5 s of tone then 10 s of silence.
        // Gated holds the programme; ungated is dragged down by two thirds of
        // the measurement being silent. `energyToLufs` floors at 1e-12, so the
        // silent blocks contribute a finite ~−120, not −inf.
        anabasis::LoudnessMeter m;
        m.prepare (sr);
        feed (m, 5.0, 0.1f, 0.0);
        for (int n = 0; n < (int) (10.0 * sr); ++n)
        {
            const float fr[2] = { 0.0f, 0.0f };
            m.processFrame (fr, 2);
        }
        check (near (m.integratedLufs(), -20.0f, 0.15f),
               "ungated: (premise) the GATED reading holds the programme");
        check (m.integratedUngatedLufs() < -24.0f,
               "ungated: BS.1770-1 has no absolute gate, so the silence drags it down");
    }
    {   // The reset clears both, and the LRA watermark is the SHORT-TERM
        // window's rather than the integrated one's: a reset issued during
        // loud playback must not leave a pre-reset short-term value setting
        // the 95th percentile. Loud, reset, then quiet-but-steady — a shared
        // watermark of +4 would readmit ~2.9 s of the loud passage and read a
        // large range; the correct +30 reads ~0.
        anabasis::LoudnessMeter m;
        m.prepare (sr);
        feed (m, 20.0, 0.3f, 0.0);
        m.resetIntegrated();
        feed (m, 20.0, 0.01f, 20.0);
        check (near (m.lraLu(), 0.0f, 0.5f),
               "lra: the reset watermark spans the short-term window, so no pre-reset value survives");
        check (near (m.integratedLufs(), -40.0f, 0.5f),
               "lra: (premise) the integrated reading restarted on the quiet passage");
    }
}

// ---------------------------------------------------------------------------
// THE TWO CACHED READINGS MUST TRACK THEIR ACCUMULATORS. `integratedLufs()`
// walks the 751-bin histogram twice and `lraLu()` walks it three times, and the
// wrapper reads both once per processBlock — ~3750 iterations per block, which
// is ~22 M/s at 192 kHz with 32-sample buffers against DESIGN §9's ≤ 0.5 %
// metering allocation. Both are pure functions of the session-cumulative
// accumulators, so they are held between gating blocks.
//
// The cache cannot change a value; it can only make one STALE, and there are
// exactly two ways to get that wrong — forgetting to invalidate when a gating
// block commits (the reading freezes at the first level ever measured) and
// forgetting on a reset (the reading survives the clear it was asked for).
// This drives both. The VALUES themselves are pinned by every other loudness
// test in this file, unchanged: a cache that returned anything different would
// fail there first.
static void testTheCachedLoudnessReadingsAreNeverStale()
{
    const double sr = 48000.0;
    auto near = [] (float a, float b, float tol) { return std::abs (a - b) <= tol; };
    auto feed = [sr] (anabasis::LoudnessMeter& m, double seconds, float amp, double t0)
    {
        for (int n = 0; n < (int) (seconds * sr); ++n)
        {
            const double t = t0 * sr + n;
            const float s = amp * std::sin (2.0f * juce::MathConstants<float>::pi
                                            * 997.0f * (float) t / (float) sr);
            const float fr[2] = { s, s };
            m.processFrame (fr, 2);
        }
    };

    anabasis::LoudnessMeter m;
    m.prepare (sr);

    // Read BEFORE any audio: the sentinels, and reading them must not poison
    // the cache for the measurement that follows.
    check (juce::exactlyEqual (m.integratedLufs(), anabasis::LoudnessMeter::kSilentLufs)
           && juce::exactlyEqual (m.lraLu(), anabasis::LoudnessMeter::kNoLra),
           "lufsCache: an unfed meter reads its sentinels");

    feed (m, 20.0, 0.1f, 0.0);                       // −20 LUFS
    check (near (m.integratedLufs(), -20.0f, 0.2f),
           "lufsCache: …and the first real reading lands after the sentinel read");
    const float loudLra = m.lraLu();
    check (loudLra >= 0.0f, "lufsCache: (premise) LRA has a reading too");

    // A repeated read with no audio between is the cache's whole point, and it
    // must be the SAME value — not merely close.
    check (juce::exactlyEqual (m.integratedLufs(), m.integratedLufs())
           && juce::exactlyEqual (m.lraLu(), m.lraLu()),
           "lufsCache: repeated reads inside one sub-block are identical");

    // 20 s at −10 LUFS: gating blocks keep committing, so both figures MUST
    // move. A cache that never invalidates on a commit freezes them at −20.
    // The step is UPWARD deliberately — a −40 passage would be excluded by the
    // integrated reading's own −10 LU relative gate and leave the figure at
    // −20 legitimately, which would make this assertion pass against a frozen
    // cache as well.
    feed (m, 20.0, 0.316228f, 20.0);
    check (m.integratedLufs() > -18.0f,
           "lufsCache: a committed gating block moves the integrated reading");
    check (! juce::exactlyEqual (m.lraLu(), loudLra),
           "lufsCache: …and the LRA reading, which the same commit feeds");

    // A reset clears the accumulators with no audio at all, so both must drop
    // to their sentinels on the very next read. This is the failure a stale
    // cache makes user-visible: the meter-reset button appearing to do nothing.
    m.resetIntegrated();
    check (juce::exactlyEqual (m.integratedLufs(), anabasis::LoudnessMeter::kSilentLufs),
           "lufsCache: a reset invalidates immediately, without waiting for a block");
    check (juce::exactlyEqual (m.lraLu(), anabasis::LoudnessMeter::kNoLra),
           "lufsCache: …and clears the LRA reading with it");

    // …and the meter still measures after the reset, so the invalidation did
    // not simply pin the sentinels.
    feed (m, 20.0, 0.1f, 40.0);
    check (near (m.integratedLufs(), -20.0f, 0.3f),
           "lufsCache: the meter measures again after the reset");
}

// ---------------------------------------------------------------------------
// The 50 ms Hann RMS (ADR-0020). Every case is a closed-form level: a sine's
// RMS is its amplitude/√2, DC's is its own value, and both are window-shape
// independent BECAUSE the window sum normalises — which is the property the
// second case exists to pin (an un-normalised window reads ~3 dB low).
static void testRmsMeterReadsTrueLevels()
{
    const double sr = 48000.0;
    auto near = [] (float a, float b, float tol) { return std::abs (a - b) <= tol; };
    auto settledDb = [sr] (float amp, bool dc)
    {
        anabasis::RmsMeter m;
        m.prepare (sr);
        for (int n = 0; n < (int) (0.5 * sr); ++n)     // 10 windows: fully settled
        {
            const float s = dc ? amp
                               : amp * std::sin (2.0f * juce::MathConstants<float>::pi
                                                 * 1000.0f * (float) n / (float) sr);
            const float fr[2] = { s, s };
            m.processFrame (fr, 2);
        }
        return m.rmsDb();
    };

    check (near (settledDb (1.0f, false), -3.01f, 0.05f),
           "rms: a full-scale sine reads -3.01 dBFS (the MATHEMATICAL reference)");
    check (near (settledDb (1.0f, true), 0.0f, 0.05f),
           "rms: full-scale DC reads 0 dBFS (the window normalisation is exact)");
    check (near (settledDb (0.1f, false), -23.01f, 0.05f),
           "rms: linearity — a -20 dBFS sine reads -23.01");

    {   // Before the first full 50 ms window there is NO reading, deliberately:
        // a partially filled window reads low by the fraction still empty, and
        // a wrong number on a transport start is worse than an absent one.
        anabasis::RmsMeter m;
        m.prepare (sr);
        for (int n = 0; n < (int) (0.02 * sr); ++n)    // 20 ms — under the window
        {
            const float fr[2] = { 0.5f, 0.5f };
            m.processFrame (fr, 2);
        }
        check (juce::exactlyEqual (m.rmsDb(), anabasis::RmsMeter::kSilentDb),
               "rms: a partly filled window reads the silent sentinel, not a low number");
    }
    {   // Stereo is the MEAN square across channels, so a correlated signal
        // reads the same as the same signal on one channel — and a signal on
        // ONE channel of a stereo frame reads 3 dB lower, which is the half of
        // the convention a correlated-only test cannot see.
        anabasis::RmsMeter m;
        m.prepare (sr);
        for (int n = 0; n < (int) (0.5 * sr); ++n)
        {
            const float s = std::sin (2.0f * juce::MathConstants<float>::pi
                                      * 1000.0f * (float) n / (float) sr);
            const float fr[2] = { s, 0.0f };
            m.processFrame (fr, 2);
        }
        check (near (m.rmsDb(), -6.02f, 0.05f),
               "rms: one channel of a stereo frame reads 3 dB below the correlated case");
    }
    {   // The sentinel and the readings must NOT overlap. Once a full window
        // has been seen the meter has measured something, and "below what this
        // meter resolves" is an answer — so it reports `kFloorDb`, which sits
        // strictly above "nothing measured yet". Both stimuli here published
        // the sentinel while a single constant served both jobs: exact silence
        // has no logarithm, and −163 dBFS fell below where the computed range
        // was cut off. Either would have been read as an absent measurement.
        static_assert (anabasis::RmsMeter::kFloorDb > anabasis::RmsMeter::kSilentDb,
                       "a reading must never be mistaken for the sentinel");

        anabasis::RmsMeter m;
        m.prepare (sr);
        for (int n = 0; n < (int) (0.5 * sr); ++n)
        {
            const float fr[2] = { 0.0f, 0.0f };
            m.processFrame (fr, 2);
        }
        check (juce::exactlyEqual (m.rmsDb(), anabasis::RmsMeter::kFloorDb),
               "rms: digital silence reads the floor — a measurement, not the sentinel");

        // A real signal 23 dB below the floor: clamped to the floor, never to
        // the sentinel. This is the case the clamp exists for; without it the
        // reading is −163, which is on the wrong side of "nothing measured".
        check (juce::exactlyEqual (settledDb (1.0e-8f, false), anabasis::RmsMeter::kFloorDb),
               "rms: a signal under the meter's resolution reads the floor, not the sentinel");
    }
}

// ---------------------------------------------------------------------------
// Window semantics: M is the newest 400 ms, S the last 3 s — after a level
// step, M has fully adopted by 500 ms while S still remembers the old level.
static void testLufsWindows()
{
    const double sr = 48000.0;
    anabasis::LoudnessMeter m;
    m.prepare (sr);
    auto run = [&] (float amp, double seconds)
    {
        for (int n = 0; n < (int) (seconds * sr); ++n)
        {
            const float s = amp * std::sin (2.0f * juce::MathConstants<float>::pi
                                            * 997.0f * (float) n / (float) sr);
            const float fr[2] = { s, s };
            m.processFrame (fr, 2);
        }
    };
    run (0.01f, 4.0);                       // -40 LUFS for 4 s
    run (0.1f, 0.5);                        // step to -20, half a second
    const float mNow = m.momentaryLufs(), sNow = m.shortTermLufs();
    check (std::abs (mNow - (-20.0f)) < 0.3f, "windows: momentary adopts a step within 500 ms");
    check (sNow < -23.0f && sNow > -40.0f,    "windows: short-term still remembers the old level");
}

// ---------------------------------------------------------------------------
// inv 10, the named monitoring-honesty test: loudness compensation must not
// alter the RENDER. With nonRealtime set, the output with loudnessComp on is
// BIT-IDENTICAL to the output with it off; in realtime the same signal is
// measurably attenuated toward the dry loudness.
static void testLoudnessCompensationDoesNotAlterRender()
{
    const double sr = 48000.0;
    auto render = [&] (bool compOn, bool offline) -> std::vector<float>
    {
        anabasis::AnabasisEngine engine;
        engine.prepare (sr, 512, 2);
        anabasis::EngineParameters p;
        p.limGainDb    = 12.0f;                 // wet is much louder than dry
        p.loudnessComp = compOn;
        p.nonRealtime  = offline;
        p.truePeakMode = false;
        std::vector<float> out;
        juce::AudioBuffer<float> buf (2, 512);
        for (int b = 0; b < 200; ++b)           // ~2.1 s: measure + smoother settle
        {
            for (int n = 0; n < 512; ++n)
            {
                const float v = 0.15f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                  * 500.0f * (float) (b * 512 + n) / (float) sr);
                buf.setSample (0, n, v); buf.setSample (1, n, v);
            }
            engine.process (buf, p);
            for (int n = 0; n < 512; ++n)
                out.push_back (buf.getSample (0, n));
        }
        return out;
    };

    {   // OFFLINE: bit-identical with comp on vs off — the render is untouched.
        const auto off = render (false, true), on = render (true, true);
        bool identical = true;
        for (size_t n = 0; n < off.size(); ++n)
            if (! juce::exactlyEqual (off[n], on[n])) { identical = false; break; }
        check (identical, "inv10: loudnessComp does not alter the offline render, bit for bit");
    }
    {   // REALTIME: comp attenuates the monitor toward the dry loudness.
        const auto off = render (false, false), on = render (true, false);
        auto tailRmsDb = [] (const std::vector<float>& v)
        {
            double s = 0.0; int c = 0;
            for (size_t n = v.size() - 24000; n < v.size(); ++n) { s += (double) v[n] * v[n]; ++c; }
            return 20.0 * std::log10 (std::sqrt (s / c));
        };
        const double offDb = tailRmsDb (off), onDb = tailRmsDb (on);
        check (onDb < offDb - 6.0,
               "inv10: in realtime the monitor is pulled well below the uncompensated level");
        // ...and toward the DRY level (-16.5 dB RMS input): within a few dB.
        check (std::abs (onDb - (-16.5)) < 3.5,
               "inv10: the compensated monitor sits near the dry loudness");
        // The PREDICT floor acts before the measure can (short-term needs
        // seconds of data; the 200 ms smoother is the only delay): the first
        // 300 ms are already pulled down hard.
        auto earlyRmsDb = [] (const std::vector<float>& v)
        {
            double s = 0.0; int c = 0;
            for (size_t n = 4800; n < 14400; ++n) { s += (double) v[n] * v[n]; ++c; }
            return 20.0 * std::log10 (std::sqrt (s / c));
        };
        check (earlyRmsDb (on) < earlyRmsDb (off) - 4.0,
               "inv10: the predict floor pre-ducks the monitor before the measure exists");
    }
    {   // MID-STREAM realtime→offline flip: the monitor state must SNAP inert
        // (gain 1, delta 0), not slew — from the first offline block the
        // render is bit-identical between comp on and comp off. The monitor
        // gain is post-mix and the meters are fed pre-monitor frames, so the
        // two runs' engine states agree; only the snap can differ.
        auto renderFlip = [&] (bool compOn) -> std::vector<float>
        {
            anabasis::AnabasisEngine engine;
            engine.prepare (sr, 512, 2);
            anabasis::EngineParameters p;
            p.limGainDb    = 12.0f;
            p.loudnessComp = compOn;
            p.truePeakMode = false;
            std::vector<float> out;
            juce::AudioBuffer<float> buf (2, 512);
            for (int b = 0; b < 200; ++b)
            {
                p.nonRealtime = b >= 100;           // the flip, mid-stream
                for (int n = 0; n < 512; ++n)
                {
                    const float v = 0.15f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                      * 500.0f * (float) (b * 512 + n) / (float) sr);
                    buf.setSample (0, n, v); buf.setSample (1, n, v);
                }
                engine.process (buf, p);
                for (int n = 0; n < 512; ++n)
                    out.push_back (buf.getSample (0, n));
            }
            return out;
        };
        const auto off = renderFlip (false), on = renderFlip (true);
        bool preDiffers = false, postIdentical = true;
        for (size_t n = 90 * 512; n < 100 * 512; ++n)
            if (! juce::exactlyEqual (off[n], on[n])) { preDiffers = true; break; }
        for (size_t n = 100 * 512; n < off.size(); ++n)
            if (! juce::exactlyEqual (off[n], on[n])) { postIdentical = false; break; }
        check (preDiffers, "inv10 flip: before the flip the comp IS acting (the runs differ)");
        check (postIdentical,
               "inv10 flip: from the first offline block the render is bit-identical — no residual slew");
    }
}

// ---------------------------------------------------------------------------
// §2.7 delta: with a transparent chain the difference signal is exact silence
// (the default path is bit-exact, so dry-minus-wet cancels perfectly); with
// processing engaged it is the removed material — nonzero, and inert offline.
static void testDeltaMonitor()
{
    const double sr = 48000.0;
    auto render = [&] (float pushDb, bool deltaOn, bool offline) -> std::vector<float>
    {
        anabasis::AnabasisEngine engine;
        engine.prepare (sr, 512, 2);
        anabasis::EngineParameters p;
        p.limGainDb    = pushDb;
        p.deltaMonitor = deltaOn;
        p.nonRealtime  = offline;
        p.truePeakMode = false;
        std::vector<float> out;
        juce::AudioBuffer<float> buf (2, 512);
        for (int b = 0; b < 60; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                const float v = 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                 * 500.0f * (float) (b * 512 + n) / (float) sr);
                buf.setSample (0, n, v); buf.setSample (1, n, v);
            }
            engine.process (buf, p);
            for (int n = 0; n < 512; ++n)
                out.push_back (buf.getSample (0, n));
        }
        return out;
    };
    auto tailPeak = [] (const std::vector<float>& v)
    {
        float pk = 0.0f;
        for (size_t n = v.size() - 24000; n < v.size(); ++n)
            pk = juce::jmax (pk, std::abs (v[n]));
        return pk;
    };

    check (tailPeak (render (0.0f, true, false)) < 1.0e-6f,
           "delta: a transparent chain's difference signal is silence");
    check (tailPeak (render (12.0f, true, false)) > 0.05f,
           "delta: a pushed chain's difference signal is the removed material");
    {
        const auto normal = render (12.0f, false, true), withDelta = render (12.0f, true, true);
        bool identical = true;
        for (size_t n = 0; n < normal.size(); ++n)
            if (! juce::exactlyEqual (normal[n], withDelta[n])) { identical = false; break; }
        check (identical, "delta: inert in the offline render (monitor path only)");
    }
}

// ---------------------------------------------------------------------------
// MODE inv 3: adaptation converges on steady programme and then HOLDS — the
// residual block-to-block output modulation attributable to adaptation stays
// under a stated bound. Programme: a transient-dense pattern (clicks over a
// tone) so the trims genuinely move first.
static void testAdaptationConvergesAndHolds()
{
    const double sr = 48000.0;
    anabasis::AnabasisEngine engine;
    engine.prepare (sr, 512, 2);
    anabasis::EngineParameters p;
    p.limGainDb    = 6.0f;                     // limiting engaged: release trim audible
    p.truePeakMode = false;
    juce::AudioBuffer<float> buf (2, 512);

    auto runBlock = [&] (int b) -> double
    {
        for (int n = 0; n < 512; ++n)
        {
            const int t = b * 512 + n;
            float v = 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi
                                       * 220.0f * (float) t / (float) sr);
            if ((t % 12000) < 96) v += 0.6f;   // 4 clicks/s: steady transient density
            buf.setSample (0, n, v); buf.setSample (1, n, v);
        }
        engine.process (buf, p);
        double s = 0.0;
        for (int n = 0; n < 512; ++n) { const double x = buf.getSample (0, n); s += x * x; }
        return std::sqrt (s / 512.0);
    };

    for (int b = 0; b < 800; ++b) runBlock (b);          // ~8.5 s: converge
    const float relA = engine.adaptive().publishedTrimRelease();
    double rmsMin = 1.0e9, rmsMax = 0.0;
    for (int b = 800; b < 1200; ++b)                     // ~4.3 s: hold window
    {
        const double r = runBlock (b);
        // Ignore the click blocks: compare only the steady-tone blocks, so the
        // programme's own pattern does not masquerade as modulation.
        if (r < 0.25) { rmsMin = juce::jmin (rmsMin, r); rmsMax = juce::jmax (rmsMax, r); }
    }
    const float relB = engine.adaptive().publishedTrimRelease();

    check (std::abs (relB - relA) < 0.02f,
           "adapt: trims hold once converged on steady programme (hysteresis)");
    check (20.0 * std::log10 (rmsMax / juce::jmax (rmsMin, 1.0e-9)) < 0.5,
           "adapt: residual output modulation on steady material is under 0.5 dB");
}

// ---------------------------------------------------------------------------
// MODE inv 3's Freeze clause: freeze latches the trim vector exactly — the
// four published values do not move by a single ulp across a PROGRAMME
// CHANGE that would otherwise re-slew them; unfreezing lets them move again.
// ---------------------------------------------------------------------------
// §5.4 Learn vs the reset lifecycle: an IN-FLIGHT pass does not survive a
// reset (a sample-rate change or host stop is a discontinuity in the material
// the pass is measuring, and the features are zeroed by the same call), while
// the session reference a previous commit established DOES.
//
// The stimulus is a steady sine: no transients, so a pass that commits lands
// the onset reference near 0 — far from the 4.0 factory default, which is what
// makes "did the cancelled pass commit?" a disjoint question rather than a
// tolerance one.
// ---------------------------------------------------------------------------
// §5.4 Learn commands are ONE staged record, so a stop and a start issued
// inside the same audio block both survive. Two flags consumed in a fixed
// order could not: `startLearn` ran first and zeroed the accumulator the stop
// was about to commit, then `commitLearn` no-opped on learnBlocks == 0 — the
// finished pass was lost AND the new one never began.
//
// Steady sine again (no transients ⇒ a committed onset reference lands near 0
// against the 4.0 factory default), so "did the first pass commit?" is a
// disjoint question rather than a tolerance one.
static void testStopThenStartInOneBlockKeepsBoth()
{
    const double sr = 48000.0;
    anabasis::AnabasisEngine engine;
    engine.prepare (sr, 512, 2);
    anabasis::EngineParameters p;
    p.truePeakMode = false;
    juce::AudioBuffer<float> buf (2, 512);

    auto feed = [&] (int blocks, int t0)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                const float v = 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                 * 220.0f * (float) (t0 + b * 512 + n) / (float) sr);
                buf.setSample (0, n, v); buf.setSample (1, n, v);
            }
            engine.process (buf, p);
        }
    };

    check (! engine.adaptiveForWrapper().hasLearned(), "learnCmd: nothing learned yet (baseline)");
    engine.requestLearnStart();
    feed (60, 0);                       // ~0.6 s accumulated

    engine.requestLearnStop();          // both issued between two blocks…
    engine.requestLearnStart();         // …stop first, then start
    feed (1, 60 * 512);                 // one block consumes the composed command

    // hasLearned() IS the "accumulator survived" property, not a proxy for it:
    // commitLearn only latches when learnBlocks > 0, so it can be true only if
    // the pass was still intact when the commit ran. (An earlier draft also
    // asserted the reference had MOVED off its 4.0 default — brittle, because
    // a steady sine's own startup transient leaves the onset feature near that
    // value by coincidence. The weaker-looking check is the exact one.)
    check (engine.adaptiveForWrapper().hasLearned(),
           "learnCmd: the stopped pass commits even when a restart lands in the same block");
    check (engine.adaptiveForWrapper().isLearning(),
           "learnCmd: ...and the restart is running, not swallowed by the commit");
}

static void testResetCancelsAnInFlightLearnPass()
{
    const double sr = 48000.0;
    anabasis::AnabasisEngine engine;
    engine.prepare (sr, 512, 2);
    anabasis::EngineParameters p;
    p.truePeakMode = false;
    juce::AudioBuffer<float> buf (2, 512);

    auto feed = [&] (int blocks, int t0)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                const float v = 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                 * 220.0f * (float) (t0 + b * 512 + n) / (float) sr);
                buf.setSample (0, n, v); buf.setSample (1, n, v);
            }
            engine.process (buf, p);
        }
    };

    const float ref0 = engine.adaptiveForWrapper().publishedRefOnset();
    engine.requestLearnStart();
    feed (40, 0);                      // ~0.4 s accumulated into the pass
    engine.reset();                    // the discontinuity, mid-pass
    feed (40, 40 * 512);
    engine.requestLearnStop();
    feed (2, 80 * 512);                // the commit is consumed at a block top

    check (! engine.adaptiveForWrapper().hasLearned(),
           "learnReset: a pass interrupted by reset() does not commit");
    check (juce::approximatelyEqual (engine.adaptiveForWrapper().publishedRefOnset(), ref0),
           "learnReset: ...so the reference the session already had is untouched");
}

static void testFreezeLatchesTrims()
{
    const double sr = 48000.0;
    anabasis::AnabasisEngine engine;
    engine.prepare (sr, 512, 2);
    anabasis::EngineParameters p;
    p.truePeakMode = false;
    juce::AudioBuffer<float> buf (2, 512);

    auto feed = [&] (int blocks, float clickAmp)
    {
        static int t0 = 0;
        for (int b = 0; b < blocks; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                const int t = t0 + n;
                float v = 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi
                                           * 220.0f * (float) t / (float) sr);
                if ((t % 6000) < 96) v += clickAmp;
                buf.setSample (0, n, v); buf.setSample (1, n, v);
            }
            t0 += 512;
            engine.process (buf, p);
        }
    };

    feed (400, 0.6f);                                    // adapt on transient-dense material
    p.freeze = true;
    feed (4, 0.6f);                                      // latch settles at a block boundary
    const float r0 = engine.adaptive().publishedTrimRelease();
    const float l0 = engine.adaptive().publishedTrimLink();
    const float h0 = engine.adaptive().publishedTrimHpf();
    const float d0 = engine.adaptive().publishedTrimTilt();

    feed (400, 0.0f);                                    // programme changes completely
    check (juce::exactlyEqual (engine.adaptive().publishedTrimRelease(), r0)
            && juce::exactlyEqual (engine.adaptive().publishedTrimLink(), l0)
            && juce::exactlyEqual (engine.adaptive().publishedTrimHpf(), h0)
            && juce::exactlyEqual (engine.adaptive().publishedTrimTilt(), d0),
           "freeze: the latched trim vector does not move by an ulp under new programme");

    p.freeze = false;
    feed (400, 0.0f);
    check (! juce::exactlyEqual (engine.adaptive().publishedTrimRelease(), r0),
           "freeze: unfreezing lets adaptation move again");
}

// ---------------------------------------------------------------------------
// MODE inv 4: trims stay inside their declared bounds under pathological
// programme (maximally bright, transient-dense, loud) — and the published
// vector is what proves it, since the effective values are clamped inside
// the engine anyway.
static void testTrimBounds()
{
    const double sr = 48000.0;
    anabasis::AnabasisEngine engine;
    engine.prepare (sr, 512, 2);
    anabasis::EngineParameters p;
    p.truePeakMode = false;
    juce::AudioBuffer<float> buf (2, 512);
    uint32_t rng = 0x1234u;
    for (int b = 0; b < 1200; ++b)
    {
        for (int n = 0; n < 512; ++n)
        {
            rng = rng * 1664525u + 1013904223u;          // bright noise + clicks
            float v = ((float) (rng >> 8) / 8388608.0f - 1.0f) * 0.4f;
            if (((b * 512 + n) % 3000) < 60) v += 0.9f;
            buf.setSample (0, n, v); buf.setSample (1, n, v);
        }
        engine.process (buf, p);
    }
    const auto& a = engine.adaptive();
    check (std::abs (a.publishedTrimRelease()) <= 1.0f, "bounds: release trim within ±1 octave");
    check (std::abs (a.publishedTrimLink()) <= 0.2f,    "bounds: link trim within ±0.2");
    check (a.publishedTrimHpf() >= 0.0f && a.publishedTrimHpf() <= 30.0f,
           "bounds: scHpf trim within 0…+30 Hz");
    check (a.publishedTrimTilt() >= 0.0f && a.publishedTrimTilt() <= 0.5f,
           "bounds: dynTilt trim within 0…+0.5 dB");
}

// ---------------------------------------------------------------------------
// ADR-0013 (OQ-016): the release trim reaches the AUTO release path — the two
// pole time-constants scale by 2^octaves, so after the same over-ceiling
// burst a scale-2 limiter recovers measurably SLOWER than a scale-1 limiter.
// Measured on the envelope the limiter itself emits (gainsOut), no chain.
static void testAutoReleaseFollowsTheTrimScale()
{
    auto tailGainAfter = [] (float scale) -> float
    {
        anabasis::LookaheadLimiter lim;
        lim.prepare (48000.0, 96 + 3);
        lim.setAutoRelease (true);
        lim.setStereoLink (1.0f);
        lim.setTransientPreserve (0.0f);
        lim.setTruePeakMode (false);
        lim.setAutoReleaseScale (scale);

        float gains[2] = { 1.0f, 1.0f };
        // 10 ms hard over-ceiling, then 600 ms of quiet — one time constant of
        // the SLOW auto pole (600 ms), where the scale-1 and scale-2 recovery
        // curves are furthest apart (63 % vs 39 % of the slow pole's travel).
        for (int n = 0; n < 480 + 28800; ++n)
        {
            const float v = n < 480 ? 2.0f : 0.1f;
            float frame[2] = { v, v };
            lim.processSample (frame, 2, 96, 0.891f, gains);
        }
        return gains[0];                            // how far recovery has come
    };

    const float fast = tailGainAfter (1.0f);
    const float slow = tailGainAfter (2.0f);
    check (fast > 0.7f && fast < 1.0f,
           "autoScale: (premise) scale 1 is mid-recovery after one slow time constant");
    check (slow < fast - 0.02f,
           "autoScale: scale 2 is clearly behind — the trim reaches the auto poles");

    // prepare() is a clean-state contract: the trim scale is per-block engine
    // state, not a prepared setting, so a limiter carrying a scale into a
    // prepare must come out neutral. Only AnabasisEngine::process rewrites it
    // per block, so a standalone user (the bench's limiter section) would
    // otherwise inherit whatever the previous owner last set.
    {
        anabasis::LookaheadLimiter lim;
        lim.prepare (48000.0, 96 + 3);
        lim.setAutoRelease (true);
        lim.setStereoLink (1.0f);
        lim.setTransientPreserve (0.0f);
        lim.setTruePeakMode (false);
        lim.setAutoReleaseScale (2.0f);
        lim.prepare (48000.0, 96 + 3);            // …and the scale goes with it
        float gains[2] = { 1.0f, 1.0f };
        for (int n = 0; n < 480 + 28800; ++n)
        {
            const float v = n < 480 ? 2.0f : 0.1f;
            float frame[2] = { v, v };
            lim.processSample (frame, 2, 96, 0.891f, gains);
        }
        check (std::abs (gains[0] - fast) < 1.0e-6f,
               "autoScale: prepare() resets the scale — a re-prepared limiter recovers like scale 1");
    }
}

// ---------------------------------------------------------------------------
// ADR-0014's structural obligation: a STAGED frozen-trim record always gets a
// silent bottom to land at. The wrapper requests the duck and stages the
// record as two separate stores with a whole parameter restore between them,
// so a block landing in that gap consumes the request, runs the entire ~34 ms
// duck, and returns to idle BEFORE the record exists — after which nothing
// brings the duck back and the vector waits for an unrelated one (an A/B
// switch, say), landing in whatever slot is live by then. That is the same
// misapplication the duck was added to prevent, one level down. The record
// therefore carries its own request.
//
// The interleaving is reproduced exactly rather than raced: the request is
// spent first, deliberately, and only then is the record staged.
static void testAStagedFrozenVectorAlwaysGetsABottom()
{
    const double sr = 48000.0;
    anabasis::AnabasisEngine engine;
    engine.prepare (sr, 512, 2);
    anabasis::EngineParameters p;
    p.limGainDb    = 6.0f;
    p.truePeakMode = false;
    juce::AudioBuffer<float> buf (2, 512);

    auto feed = [&] (int blocks, int t0)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int n = 0; n < 512; ++n)
            {
                const int t = t0 + b * 512 + n;
                float v = 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi
                                           * 220.0f * (float) t / (float) sr);
                if ((t % 4800) < 96) v += 0.6f;
                buf.setSample (0, n, v); buf.setSample (1, n, v);
            }
            engine.process (buf, p);
        }
    };

    feed (400, 0);                       // adapt, then latch something non-zero
    p.freeze = true;
    feed (4, 400 * 512);
    const float latched = engine.adaptive().publishedTrimRelease();

    // The wrapper's request is consumed and the whole duck completes…
    engine.requestForcedDuck();
    feed (40, 404 * 512);                // out → bottom → in → idle, all spent

    // …and only now does the record arrive. Distinct from the latched vector
    // so "did it land?" is a disjoint question.
    const float wanted = latched > 0.0f ? -0.75f : 0.75f;
    engine.restoreFrozenTrims (wanted, 0.15f, 12.0f, 0.4f);
    feed (40, 444 * 512);                // enough for a duck the record must ask for itself

    check (std::abs (engine.adaptive().publishedTrimRelease() - wanted) < 1.0e-6f,
           "frozenStage: a record staged after the caller's duck was spent still gets a bottom");
    check (std::abs (engine.adaptive().publishedTrimHpf() - 12.0f) < 1.0e-6f,
           "frozenStage: …and the whole vector lands, not just one member");
}

// ---------------------------------------------------------------------------
// The meter-reset watermark's OFF-BY-ONE half, which the wrapper-level test
// cannot see: gating blocks are assembled from the last four 100 ms
// sub-blocks, and at the instant of the reset one sub-block is PARTIALLY
// FILLED with pre-reset material. Admitting the first block at subCount + 4
// includes that straddler — a quarter of a gating block's energy taken from
// the old programme — and through the −10 LU relative gate one loud block
// then excludes every quieter block measured afterwards, which is the exact
// failure the watermark was added to prevent.
//
// Driven at the meter directly: no lookahead line, so "old programme" and
// "in-flight audio" cannot be confused, and the reset lands mid-sub-block by
// construction (2.5 sub-blocks of loud material).
static void testMeterResetIgnoresTheStraddlingSubBlock()
{
    anabasis::LoudnessMeter meter;
    meter.prepare (48000.0);

    auto feed = [&] (float amp, int frames, int t0)
    {
        for (int i = 0; i < frames; ++i)
        {
            const float v = amp * std::sin (2.0f * juce::MathConstants<float>::pi
                                            * 997.0f * (float) (t0 + i) / 48000.0f);
            float frame[2] = { v, v };
            meter.processFrame (frame, 2);
        }
    };

    // 4.5 sub-blocks: four complete ones make the first gating block exist (a
    // block needs 400 ms), and the half sub-block is the straddler under test.
    feed (0.5f, 21600, 0);
    check (meter.integratedLufs() > -20.0f,
           "meterWatermark: (premise) the loud programme is measured before the reset");

    meter.resetIntegrated();
    feed (0.005f, 144000, 21600);               // 3 s of ~-46 LUFS

    // With the straddler admitted the first fresh gating block reads ≈ -15
    // LUFS and the relative gate then drops every -46 block after it, pinning
    // the integrated figure to the old programme. Disjoint by ~30 dB.
    check (meter.integratedLufs() < -40.0f,
           "meterWatermark: the partially-filled sub-block at the reset stays out of the histogram");
}

// ---------------------------------------------------------------------------
// §2.9 spectrum capture rings (THREAD_MODEL planned edge → implemented at
// P5): tap 1 is post-input-gain, tap 2 the render, one release-published
// block per processed chunk. Pinned headlessly: the counts advance exactly
// once per chunk, and tap 1's content IS the input when inputGain is 0 dB —
// content equality is what dies if the tap moves (e.g. behind the EQ).
static void testSpectrumRingsCarryTheTaps()
{
    anabasis::AnabasisEngine engine;
    const int block = 512;
    engine.prepare (48000.0, block, 2);
    anabasis::EngineParameters p;
    juce::AudioBuffer<float> buf (2, block);

    for (int b = 0; b < 4; ++b)
    {
        for (int n = 0; n < block; ++n)
        {
            const float v = 0.25f * std::sin (0.05f * (float) (b * block + n));
            buf.setSample (0, n, v);
            buf.setSample (1, n, v);
        }
        engine.process (buf, p);
    }

    const auto& in = engine.spectrumInRing();
    check (in.writeCount() == 4 * (uint64_t) block,
           "spectrum: tap 1 published exactly one block per chunk");
    check (engine.spectrumOutRing().writeCount() == 4 * (uint64_t) block,
           "spectrum: tap 2 published exactly one block per chunk");

    // Content: the LAST input block, bit-for-bit (post-input-gain at 0 dB is
    // the identity, and stage A writes the tap before any filter).
    std::vector<float> l (block), r (block);
    check (in.readLatest (l.data(), r.data(), block) == block,
           "spectrum: tap 1 hands back a full block");
    bool exact = true;
    for (int n = 0; n < block; ++n)
    {
        const float v = 0.25f * std::sin (0.05f * (float) (3 * block + n));
        if (! juce::exactlyEqual (l[(size_t) n], v)) { exact = false; break; }
    }
    check (exact, "spectrum: tap 1 is the post-input-gain signal, untouched");

    // -- specSync (KI-015, 2026-09-02). THE PAYLOAD IS ATOMIC. The ring's
    //    release/acquire index settles what a reader SEES and is a BACKWARD
    //    edge only, so once the producer laps a reader mid-copy the two touch
    //    the same floats with nothing ordering them — and until this round
    //    both accesses were plain, which is a data race and undefined the
    //    moment it happens, not merely improbable. A race cannot be staged
    //    deterministically, so what is pinned is the TYPE that makes it
    //    defined, exactly as `grSync` pins the sibling ring's: these
    //    assertions do not COMPILE against the unrepaired header, because it
    //    has no `Sample` at all.
    {
        using Ring = anabasis::ScopeBuffer;
        check (! std::is_copy_constructible_v<Ring::Sample> && ! std::is_copy_assignable_v<Ring::Sample>,
               "specSync: the ring's PAYLOAD is not a plain float — a read racing the producer is defined, not merely rare");
        // NOT asserted, and the reason is a measurement: `is_trivially_copyable`
        // reports TRUE for this type on both libstdc++ and libc++ even though
        // every copy and move operation is deleted, so it cannot separate the
        // wrapper from a plain float. GCC's `-Wclass-memaccess` fires on a
        // different criterion — "no trivial copy-assignment" — which is why a
        // re-introduced `memcpy` is still a red job on the GCC lanes.
        check (! std::is_assignable_v<Ring::Sample&, float>,
               "specSync: …and it cannot be written with `=`, which on a bare atomic would be a SEQ_CST store per sample on the audio thread");
        check (std::atomic<float>::is_always_lock_free,
               "specSync: …and it is lock-free, so the audio thread's store stays a store and never takes a lock");
        check (sizeof (Ring::Sample) == sizeof (float) && alignof (Ring::Sample) == alignof (float),
               "specSync: …at the layout of the plain float it replaced — the ring's heap footprint is unchanged");

        // The repair moved no audio: same slots, same values, same order, and
        // the publication is still one release store carrying the whole push.
        const auto ringStorage = std::make_unique<Ring>();      // 128 KB: heap, not stack
        auto& ring = *ringStorage;
        std::vector<float> src ((size_t) 300), dl ((size_t) 300), dr ((size_t) 300);
        for (int i = 0; i < 300; ++i)
            src[(size_t) i] = 0.001f * (float) i - 0.1f;
        ring.pushBlock (src.data(), src.data(), 300);
        bool roundTrip = ring.writeCount() == 300
                         && ring.readLatest (dl.data(), dr.data(), 300) == 300;
        for (int i = 0; i < 300 && roundTrip; ++i)
            roundTrip = juce::exactlyEqual (dl[(size_t) i], src[(size_t) i])
                        && juce::exactlyEqual (dr[(size_t) i], src[(size_t) i]);
        check (roundTrip,
               "specSync: (premise) a settled batch reads exactly the published values back through the atomic payload");

        // THE `n > capacity` BRANCH, which nothing in this tree had ever
        // executed — and this round turns it into a scalar loop, so it is
        // pinned now rather than left to a host that renders in one buffer.
        // A push longer than the ring keeps the NEWEST `capacity` frames and
        // still advances the index by the WHOLE block.
        const auto bigStorage = std::make_unique<Ring>();
        auto& big = *bigStorage;
        const int over = Ring::capacity + 777;
        std::vector<float> ramp ((size_t) over), ol ((size_t) Ring::capacity), orr ((size_t) Ring::capacity);
        for (int i = 0; i < over; ++i)
            ramp[(size_t) i] = (float) i;
        big.pushBlock (ramp.data(), ramp.data(), over);
        const int got = big.readLatest (ol.data(), orr.data(), Ring::capacity);
        bool newest = big.writeCount() == (uint64_t) over && got == Ring::capacity;
        for (int i = 0; i < Ring::capacity && newest; ++i)
            newest = juce::exactlyEqual (ol[(size_t) i], ramp[(size_t) (over - Ring::capacity + i)]);
        check (newest,
               "specSync: a push longer than the ring keeps the newest capacity frames and still advances the index by the whole block");

        // THE WINDOW'S END IS THE CALLER'S TO CHOOSE (0.2.12). The analyser
        // draws TWO traces from TWO rings the producer publishes with one
        // release-store each, so it reads both at the newest index BOTH have
        // published rather than at each ring's own — `readEndingAt` is what
        // lets it, and the CLAMP is the part with a correctness argument: an
        // index from the other ring can be larger than this one's, and an index
        // this ring has since rewound is larger still, so the read must land on
        // what THIS ring has published in both cases and never past it.
        const auto endStorage = std::make_unique<Ring>();
        auto& e = *endStorage;
        std::vector<float> ramp2 ((size_t) 100), el ((size_t) 8), er ((size_t) 8), el2 ((size_t) 8), er2 ((size_t) 8);
        for (int i = 0; i < 100; ++i)
            ramp2[(size_t) i] = (float) i;
        e.pushBlock (ramp2.data(), ramp2.data(), 100);
        bool endsWhereAsked = e.readEndingAt (el.data(), er.data(), 8, 60) == 8;
        for (int i = 0; i < 8 && endsWhereAsked; ++i)
            endsWhereAsked = juce::exactlyEqual (el[(size_t) i], (float) (52 + i));
        check (endsWhereAsked,
               "specSync: a read ending at 60 returns frames 52…59 — the window the caller asked for, not the ring's newest");

        const int gotClamped = e.readEndingAt (el.data(), er.data(), 8, 1000);   // past the head
        const int gotLatest  = e.readLatest   (el2.data(), er2.data(), 8);
        bool clampsToHead = gotClamped == gotLatest && gotClamped == 8;
        for (int i = 0; i < 8 && clampsToHead; ++i)
            clampsToHead = juce::exactlyEqual (el[(size_t) i], el2[(size_t) i])
                           && juce::exactlyEqual (el[(size_t) i], (float) (92 + i));
        check (clampsToHead,
               "specSync: an end PAST this ring's head reads the head's own window — the other ring's index can be ahead, and a rewound head leaves a stale one behind");

        check (e.readEndingAt (el.data(), er.data(), 8, 0) == 0,
               "specSync: an end of 0 reads nothing, which is what a rewound partner ring drags the committed head to");
        check (anabasis::ScopeBuffer::kNewest > (uint64_t) 1 << 62,
               "specSync: …and `readLatest` is that same read with no bound, so the two cannot drift apart");

        // A WINDOW ENDING INSIDE THE RING CAN STILL BEGIN OUTSIDE IT. The end is
        // clamped to the head; the START has to be clamped to what the producer
        // has not taken back, because a chunk longer than `capacity − count`
        // laps the window the caller asked for — one frame at 12289, all 4096 at
        // 16384. Pinned by VALUE, not by count alone: the ring is filled with a
        // ramp whose sample IS its absolute index, so a frame the producer
        // overwrote reads back as the marker instead of as itself.
        std::vector<float> idx ((size_t) Ring::capacity), notHistory ((size_t) 20000),
                           gotL ((size_t) 4096), gotR ((size_t) 4096);
        for (int i = 0; i < Ring::capacity; ++i) idx[(size_t) i] = (float) i;
        for (int i = 0; i < 20000; ++i)          notHistory[(size_t) i] = -1.0f;
        bool boundary = true;
        for (const int n : { 12287, 12288, 12289, 13000, 16383, 16384, 20000 })
        {
            const auto fresh = std::make_unique<Ring>();
            auto& rr = *fresh;
            rr.pushBlock (idx.data(), idx.data(), Ring::capacity);          // frames 0…16383
            const uint64_t E = rr.writeCount();                            // 16384
            rr.pushBlock (notHistory.data(), notHistory.data(), n);         // the producer runs on
            const int served = rr.readEndingAt (gotL.data(), gotR.data(), 4096, E);
            const int want = juce::jlimit (0, 4096, Ring::capacity - n);
            bool values = served == want;
            for (int i = 0; i < served && values; ++i)
                values = juce::exactlyEqual (gotL[(size_t) i], (float) ((int) E - served + i));
            if (! values) boundary = false;
        }
        check (boundary,
               "specSync: a read whose window the producer has lapped returns only the frames it still holds — 4096 at a 12288-frame chunk, 4095 at 12289, 3384 at 13000, 1 at 16383, none at 16384 — and every one of them is its own history, never an overwritten slot");

        const auto floorStorage = std::make_unique<Ring>();
        auto& fl = *floorStorage;
        check (fl.oldestReadable() == 0, "specSync: an empty ring can serve from frame 0");
        fl.pushBlock (idx.data(), idx.data(), Ring::capacity);
        check (fl.oldestReadable() == 0,
               "specSync: a ring holding exactly its capacity has taken nothing back yet");
        fl.pushBlock (notHistory.data(), notHistory.data(), 5000);
        check (fl.oldestReadable() == 5000,
               "specSync: …and after 5000 more frames the oldest it can serve is frame 5000 — the floor a reader pairing two rings takes the higher of");

        // A PUSH THAT HAS NOT PUBLISHED YET IS STILL WRITING, AND THAT BOUND IS
        // THE READER'S. `pushBlock` fills the payload BEFORE it releases the
        // index, so `write` says nothing about the slots the producer is
        // trampling right now — a reader that checks the index before and after
        // its copy sees a ring that never moved. The ring deliberately does NOT
        // reserve for it: the size of the largest push is `samplesPerBlock`,
        // which the plugin already publishes once, and carrying it a second time
        // inside the ring would extend the producer/consumer protocol for a fact
        // the protocol already has. What `oldestReadable` promises is exactly
        // what the PUBLISHED index proves, and the checks below pin that it
        // promises no more than that — `SpectrumView::reservedFloor` adds the
        // in-flight block on the reader, where the frame that needs it lives
        // (state suite, "specReserve").
        const auto resStorage = std::make_unique<Ring>();
        auto& res = *resStorage;
        check (res.oldestReadable() == 0, "specSync: an empty ring serves from frame 0");
        res.pushBlock (idx.data(), idx.data(), Ring::capacity);
        check (res.oldestReadable() == 0,
               "specSync: the ring's floor is the published one — a full ring has published nothing it has taken back, and it does not pretend to know what an unpublished push is doing");
        res.pushBlock (notHistory.data(), notHistory.data(), 512);
        check (res.oldestReadable() == 512,
               "specSync: …and it travels with the head, one frame per frame pushed");

        std::vector<float> rl ((size_t) 4096), rr ((size_t) 4096);
        const int reserved = res.readEndingAt (rl.data(), rr.data(), 4096, Ring::capacity);
        bool reservedRight = reserved == 4096;
        for (int i = 0; i < reserved && reservedRight; ++i)
            reservedRight = juce::exactlyEqual (rl[(size_t) i], (float) (Ring::capacity - 4096 + i));
        check (reservedRight,
               "specSync: a window that ends before the floor is served whole and from its own history, whatever the producer did after it");
    }
}

// ===========================================================================
//  OQ-017 FIX 1 — THE GR HISTORY'S CADENCE IS THE PREPARED BLOCK, NOT THE
//  DELIVERED ONE (0.2.12).
//
//  `GrHistoryView` maps entry k to time k·block/rate, reading the pair the
//  ring publishes — the PREPARED one. Until 0.2.12 the wrapper pushed one
//  entry per processBlock CALL, so a host delivering D samples per call ran
//  that time base out by B/D: JUCE's `prepareToPlay` contract says a host may
//  deliver "completely variable block sizes", and the AU and VST3 wrappers
//  both prepare with the maximum and render with whatever arrives. The engine
//  now closes an entry when it has collected `maxBlock` samples of PROCESSED
//  audio, carrying the remainder across calls.
//
//  What this pins, in the order the passes below assert it:
//   1. CADENCE — entries = samples / B for every D, at every rate.
//   2. CONSERVATION — the entry SEQUENCE is identical whatever the delivery
//      schedule. That single statement is "no sample is lost and none is
//      counted twice" and "the statistics describe the entry's own span", and
//      it is asserted bit-exactly rather than within a tolerance.
//   3. EXACTNESS — an entry's peak is the max of exactly the B rendered
//      samples it spans, computed here from the input and the group delay
//      with no reference to the implementation, which is what proves a large
//      delivered block is SPLIT rather than assigned wholesale.
//   4. REMAINDER — a partial entry is carried across calls, published on the
//      sample that completes it and never before.
//   5. D == B is BIT-IDENTICAL to the pair the wrapper pushed before 0.2.12.
//   6. A re-prepare that CLEARS the ring drops the partial, so no sample from
//      the old configuration reaches a new one's entry; one that keeps the
//      ring's entries keeps the partial with them (round 15 — this item read
//      "a re-prepare drops the partial" until the PR review found that it
//      dropped it across a pause/resume too, which the ring does not).
//      `testTheHistorySurvivesASameConfigurationRePrepare` is the matrix.
// ===========================================================================
namespace grfix
{
    struct Run
    {
        std::vector<anabasis::GrHistoryBuffer::Entry> entries;
        std::vector<float> callGrDb, callPeak;   // the PRE-0.2.12 per-call pair
        int delay = 0;
    };

    // Prepare at (rate, B), deliver `in` in the sizes `schedule` cycles
    // through, and collect every entry the engine pushes. The per-call pair
    // the wrapper used to push is recorded beside them, so the D == B identity
    // is asserted against the old EXPRESSION rather than a remembered number.
    static Run run (double rate, int B, const std::vector<float>& in,
                    const std::vector<int>& schedule, bool freeze = false)
    {
        Run r;
        anabasis::AnabasisEngine engine;
        anabasis::GrHistoryBuffer ring;
        ring.prepare (rate, B);
        engine.prepare (rate, B, 2);
        engine.setGrHistorySink (&ring);
        r.delay = engine.groupDelaySamples();

        anabasis::EngineParameters p;                       // POD defaults
        p.freeze = freeze;
        int maxD = 1;
        for (int d : schedule) maxD = juce::jmax (maxD, d);
        juce::AudioBuffer<float> buf (2, maxD);

        int pos = 0;
        size_t si = 0;
        int64_t taken = 0;
        while (pos < (int) in.size())
        {
            const int d = juce::jmin (schedule[si++ % schedule.size()], (int) in.size() - pos);
            for (int n = 0; n < d; ++n)
            {
                buf.setSample (0, n, in[(size_t) (pos + n)]);
                buf.setSample (1, n, in[(size_t) (pos + n)]);
            }
            juce::AudioBuffer<float> sub (buf.getArrayOfWritePointers(), 2, d);
            engine.process (sub, p);
            r.callGrDb.push_back (juce::Decibels::gainToDecibels (engine.lastBlockMinGain(), -60.0f));
            r.callPeak.push_back (engine.lastRenderPeak());
            // Drain inside the loop: a long run pushes more than the ring's
            // 4096 slots, and one call never pushes more than eight.
            for (int64_t k = taken; k < ring.available(); ++k)
                r.entries.push_back (ring.peek (k));
            taken = ring.available();
            pos += d;
        }
        return r;
    }
}

static void testGrHistoryEntriesFollowThePreparedBlock()
{
    using grfix::run;

    // ---- 1. Cadence: entries = samples / B, whatever D is. ----
    //
    // Three rates, two prepared sizes (1156 is the AU default, which is not a
    // power of two and is exactly the case the old code got wrong in Logic),
    // and D/B from an eighth of a block to eight blocks a call.
    {
        const double rates[]  = { 44100.0, 48000.0, 96000.0 };
        const int    blocks[] = { 512, 1156 };
        const double ratios[] = { 0.25, 0.5, 1.0, 2.0, 8.0 };
        bool cadenceHeld = true, durationHeld = true;
        int  configs = 0;
        for (double rate : rates)
            for (int B : blocks)
                for (double ratio : ratios)
                {
                    const int wanted = 24;
                    const int total  = B * wanted;
                    const int D      = (int) std::lround ((double) B * ratio);
                    std::vector<float> in ((size_t) total);
                    for (int t = 0; t < total; ++t)
                        in[(size_t) t] = 0.5f * std::sin (0.017f * (float) t);
                    const auto r = run (rate, B, in, { D });
                    ++configs;
                    if ((int) r.entries.size() != wanted) cadenceHeld = false;
                    // …stated as the RATE the display reads it as, which is
                    // the quantity OQ-017 measured running out by B/D.
                    const double seconds = (double) total / rate;
                    const double cadence = (double) r.entries.size() / seconds;
                    if (std::abs (cadence - rate / (double) B) > 1.0e-9) cadenceHeld = false;
                    // …and the history's DURATION, which is what the twenty
                    // second window is a promise about.
                    const double held = (double) r.entries.size() * (double) B / rate;
                    if (std::abs (held - seconds) > 1.0e-9) durationHeld = false;
                }
        check (configs == 30, "grCadence: (premise) thirty rate x block x D/B configurations were run");
        check (cadenceHeld,
               "grCadence: the entry rate is rate/B at every rate, every prepared size and every "
               "D/B from 0.25 to 8 — the delivered size does not enter it");
        check (durationHeld,
               "grCadence: …so the history spans the duration of the audio that produced it");
    }

    // ---- 2. Variable D whose mean is B, and repeated boundary crossings. ----
    {
        const double rate = 48000.0;
        const int    B    = 512;
        // Seven deliveries summing to 7·512: every one of them crosses,
        // undershoots or overshoots a boundary, and none is a multiple of it.
        const std::vector<int> schedule { 1, 3, 17, 63, 512, 1024, 1964 };
        int sum = 0;
        for (int d : schedule) sum += d;
        check (sum == 7 * B, "grCadence: (premise) the variable schedule's mean delivery IS the prepared size");
        const int total = sum * 30;
        std::vector<float> in ((size_t) total);
        for (int t = 0; t < total; ++t)
            in[(size_t) t] = 0.5f * std::sin (0.017f * (float) t);
        const auto r = run (rate, B, in, schedule);
        check ((int) r.entries.size() == total / B,
               "grCadence: a host with a variable block size whose mean is the prepared one still "
               "publishes exactly one entry per prepared block");
        check (r.callGrDb.size() == (size_t) (30 * schedule.size()),
               "grCadence: (premise) …across far more CALLS than entries in the small deliveries "
               "and far fewer in the large ones");
    }

    // ---- 3. The entry sequence does not depend on the delivery schedule. ----
    //
    // Same audio, six schedules — including one that puts a boundary inside
    // every delivered block. Bit-exact equality is the strong form of "the
    // statistics correspond to the samples the entry represents": if a sample
    // were lost, double-counted, or folded into the wrong entry by any
    // schedule, the sequences would part.
    {
        const double rate = 48000.0;
        const int    B    = 512;
        const int    total = B * 48;
        const std::vector<std::vector<int>> schedules {
            { B }, { 128 }, { 64 }, { 1024 }, { 4096 }, { 1, 3, 17, 63, 512, 1024, 1964 } };

        // Two stimuli: one the chain passes through bit-exactly (defaults on
        // sub-ceiling material), and one that drives real gain reduction, so
        // BOTH statistics an entry carries are covered rather than only the
        // peak. The amplitude steps every 97 samples — coprime with every
        // block and every delivery below, so no pattern edge can hide behind
        // a boundary.
        //
        // THE HOT PASS RUNS FROZEN, and the reason is a measured property of
        // the chain rather than a convenience. §5.4's `finishBlock` runs once
        // per `process()` CALL and the trims it produces are adopted for that
        // whole call, so the DELIVERED size — not the chunking — sets the
        // adaptation cadence, and a host running 64 adapts eight times as
        // often as one running 512. That was true before this fix and is
        // untouched by it; frozen, the trim vector is constant and what
        // remains under test is the entry accumulation alone. The unfrozen
        // divergence is measured and bounded by the pass that follows.
        for (int hot = 0; hot < 2; ++hot)
        {
            std::vector<float> in ((size_t) total);
            for (int t = 0; t < total; ++t)
            {
                const float amp = (hot != 0 ? 1.30f : 0.90f) * (0.13f + 0.037f * (float) ((t / 97) % 23));
                in[(size_t) t] = amp * std::sin (0.31f * (float) t);
            }
            const auto ref = run (rate, B, in, schedules[0], hot != 0);
            check ((int) ref.entries.size() == total / B,
                   "grSplit: (premise) the reference run holds one entry per prepared block");

            bool grSeen = false;
            for (const auto& e : ref.entries) if (e.grDb < -0.5f) grSeen = true;
            check (hot == 0 ? ! grSeen : grSeen,
                   hot == 0 ? "grSplit: (premise) the transparent stimulus draws no gain reduction"
                            : "grSplit: (premise) the hot stimulus DOES, so the grDb statistic is under test too");

            bool same = true;
            float worst = 0.0f;
            for (size_t s = 1; s < schedules.size(); ++s)
            {
                const auto other = run (rate, B, in, schedules[s], hot != 0);
                if (other.entries.size() != ref.entries.size()) { same = false; continue; }
                for (size_t k = 0; k < ref.entries.size(); ++k)
                {
                    if (! juce::exactlyEqual (other.entries[k].grDb, ref.entries[k].grDb)
                        || ! juce::exactlyEqual (other.entries[k].peak, ref.entries[k].peak))
                        same = false;
                    worst = juce::jmax (worst, std::abs (other.entries[k].grDb - ref.entries[k].grDb));
                }
            }
            check (same,
                   hot == 0 ? "grSplit: the entry sequence is bit-identical under every delivery "
                              "schedule — 64, 128, 512, 1024, 4096 and a variable one"
                            : "grSplit: …with gain reduction running, so neither statistic depends "
                              "on how the host chopped the audio up");

            if (hot == 0)
            {
                // The peak an entry MUST carry, in closed form: at defaults on
                // sub-ceiling material the render is the input delayed by
                // `groupDelaySamples()` exactly (`testNullWithDefaults`), so
                // entry k's peak is the max of |in| over the B samples that
                // land in [k·B, k·B+B). Nothing here reads the implementation.
                bool exact = true;
                for (size_t k = 0; k < ref.entries.size(); ++k)
                {
                    float want = 0.0f;
                    for (int n = (int) k * B; n < (int) k * B + B; ++n)
                    {
                        const int src = n - ref.delay;
                        if (src >= 0 && src < total)
                            want = juce::jmax (want, std::abs (in[(size_t) src]));
                    }
                    if (! juce::exactlyEqual (ref.entries[k].peak, want)) exact = false;
                }
                check (exact,
                       "grSplit: every entry's peak is the max of exactly the B rendered samples it "
                       "spans, derived from the input and the group delay");
                int steps = 0;
                for (size_t k = 1; k < ref.entries.size(); ++k)
                    if (! juce::exactlyEqual (ref.entries[k].peak, ref.entries[k - 1].peak)) ++steps;
                check (steps > (int) ref.entries.size() / 2,
                       "grSplit: (negative control) neighbouring entries carry DIFFERENT peaks, so a "
                       "delivered block assigned wholesale to one entry would fail the check above");
            }
        }
    }

    // ---- 3b. …and UNFROZEN the sequences part only by §5.4's per-call
    //          adaptation cadence, which is bounded and pre-existing. ----
    //
    // Recorded rather than left implicit: with the trims live, the delivered
    // size changes how often `finishBlock` runs and therefore the release
    // scale, stereo link and detector trim the whole call is processed with.
    // The entries follow the audio, so they follow that too. The point of the
    // measurement is the SIZE of it — hundredths of a decibel, against the
    // B/D time-base error this fix removes, which was unbounded.
    {
        const double rate = 48000.0;
        const int    B    = 512, total = B * 48;
        std::vector<float> in ((size_t) total);
        for (int t = 0; t < total; ++t)
            in[(size_t) t] = 1.30f * (0.13f + 0.037f * (float) ((t / 97) % 23))
                                   * std::sin (0.31f * (float) t);
        const auto ref = run (rate, B, in, { B });
        float worstGr = 0.0f, worstPeak = 0.0f;
        for (const std::vector<int>& sched : std::vector<std::vector<int>> { { 64 }, { 4096 },
                                                                            { 1, 3, 17, 63, 512, 1024, 1964 } })
        {
            const auto other = run (rate, B, in, sched);
            if (other.entries.size() != ref.entries.size()) { worstGr = 1.0e9f; break; }
            for (size_t k = 0; k < ref.entries.size(); ++k)
            {
                worstGr   = juce::jmax (worstGr,   std::abs (other.entries[k].grDb - ref.entries[k].grDb));
                worstPeak = juce::jmax (worstPeak, std::abs (other.entries[k].peak - ref.entries[k].peak));
            }
        }
        check (worstGr < 0.05f && worstPeak < 0.005f,
               juce::String ("grSplit: with the §5.4 trims LIVE the delivery schedule moves an entry by "
                             "at most hundredths of a decibel — the adaptation's per-call cadence, not "
                             "the accumulation (worst " + juce::String (worstGr, 5) + " dB, "
                             + juce::String (worstPeak, 6) + " linear)").toRawUTF8());
    }

    // ---- 4. A transient inside one large delivered block lands in the one
    //         entry that spans it, not in the block's worth of entries. ----
    {
        const double rate = 48000.0;
        const int    B    = 512, total = B * 32, hit = 2600, len = 64;
        std::vector<float> in ((size_t) total, 0.0f);
        for (int n = 0; n < len; ++n) in[(size_t) (hit + n)] = 0.9f;
        const auto r = run (rate, B, in, { 4096 });
        const int first = (hit + r.delay) / B;
        const int last  = (hit + len - 1 + r.delay) / B;
        check (first == last,
               "grSplit: (premise) the burst is short enough and placed so that it lies wholly "
               "inside one prepared block once the group delay has moved it");
        int loud = 0, where = -1;
        for (size_t k = 0; k < r.entries.size(); ++k)
            if (r.entries[k].peak > 0.1f) { ++loud; where = (int) k; }
        check ((int) r.entries.size() == total / B,
               "grSplit: (premise) eight 4096-sample deliveries publish thirty-two 512-sample entries");
        check (loud == 1 && where == first,
               "grSplit: a 64-sample transient delivered inside a 4096-sample block occupies exactly "
               "ONE entry, at the index the prepared grid and the group delay put it at");
    }

    // ---- 5. The remainder is carried: published on the sample that completes
    //         an entry, never before, and never dropped at a call boundary. ----
    {
        const double rate = 48000.0;
        const int    B    = 512;
        anabasis::AnabasisEngine engine;
        anabasis::GrHistoryBuffer ring;
        ring.prepare (rate, B);
        engine.prepare (rate, B, 2);
        engine.setGrHistorySink (&ring);
        anabasis::EngineParameters p;
        juce::AudioBuffer<float> buf (2, 4096);
        buf.clear();
        auto deliver = [&] (int n)
        {
            juce::AudioBuffer<float> sub (buf.getArrayOfWritePointers(), 2, n);
            engine.process (sub, p);
        };
        deliver (100); check (ring.available() == 0, "grRemainder: 100 of 512 samples publishes nothing");
        deliver (300); check (ring.available() == 0, "grRemainder: …400 of 512 still publishes nothing");
        deliver (112); check (ring.available() == 1, "grRemainder: the entry closes ON its 512th sample");
        deliver (1);   check (ring.available() == 1, "grRemainder: …and the 513th opens the next entry rather than closing it");
        deliver (511); check (ring.available() == 2, "grRemainder: a partial entry is carried across a call boundary, not dropped");
        deliver (5 * B + 7);
        check (ring.available() == 7,
               "grRemainder: one delivered block larger than several prepared blocks closes every one of them");
        deliver (B - 7);
        check (ring.available() == 8, "grRemainder: …and its remainder is completed by the next call's audio");
        deliver (B - 1);
        check (ring.available() == 8, "grRemainder: a final partial entry stays unpublished");
    }

    // ---- 6. D == B is bit-identical to the pre-0.2.12 wrapper pair. ----
    {
        const double rate = 48000.0;
        const int    B    = 512, total = B * 40;
        std::vector<float> in ((size_t) total);
        for (int t = 0; t < total; ++t)
            in[(size_t) t] = 1.3f * (0.13f + 0.037f * (float) ((t / 97) % 23)) * std::sin (0.31f * (float) t);
        const auto r = run (rate, B, in, { B });
        bool identical = r.entries.size() == r.callGrDb.size() && r.entries.size() == (size_t) (total / B);
        for (size_t k = 0; identical && k < r.entries.size(); ++k)
            identical = juce::exactlyEqual (r.entries[k].grDb, r.callGrDb[k])
                     && juce::exactlyEqual (r.entries[k].peak, r.callPeak[k]);
        check (identical,
               "grIdentity: at D == B the engine's entry is bit-identical to the pair the wrapper "
               "pushed before 0.2.12 — gainToDecibels (lastBlockMinGain(), -60) and lastRenderPeak()");
    }

    // ---- 6b. An entry's GR describes ITS OWN samples, not every sample since
    //          the transport started. ----
    //
    // The per-chunk minima are reset at the top of every chunk, and this is
    // what says so: leave that reset out and `grMinChunk` becomes a running
    // global minimum, which is still schedule-invariant (the entry boundaries
    // are at the same absolute samples in every schedule) and still agrees
    // with `lastBlockMinGain()` (the per-call fold reads the same running
    // value) — so passes 3 and 6 both stay green while the GR trace latches at
    // the deepest reduction of the session and never recovers. Only a stimulus
    // whose reduction GOES AWAY can see it.
    {
        const double rate = 48000.0;
        const int    B    = 512, loud = 16, quiet = 512;
        anabasis::AnabasisEngine engine;
        anabasis::GrHistoryBuffer ring;
        ring.prepare (rate, B);
        engine.prepare (rate, B, 2);
        engine.setGrHistorySink (&ring);
        anabasis::EngineParameters p;
        juce::AudioBuffer<float> buf (2, B);
        int t = 0;
        for (int b = 0; b < loud + quiet; ++b)
        {
            for (int n = 0; n < B; ++n, ++t)
            {
                const float v = b < loud ? 1.5f * std::sin (0.05f * (float) t) : 0.0f;
                buf.setSample (0, n, v);
                buf.setSample (1, n, v);
            }
            engine.process (buf, p);
        }
        check (ring.available() == loud + quiet, "grRecover: (premise) one entry a block");
        float deepest = 0.0f;
        for (int k = 0; k < loud; ++k) deepest = juce::jmin (deepest, ring.peek (k).grDb);
        check (deepest < -0.5f, "grRecover: (premise) the loud passage draws real reduction");
        float worstTail = -1000.0f;
        for (int k = loud + quiet / 2; k < loud + quiet; ++k)
            worstTail = juce::jmax (worstTail, ring.peek (k).grDb);
        float shallowestTail = 0.0f;
        for (int k = loud + quiet / 2; k < loud + quiet; ++k)
            shallowestTail = juce::jmin (shallowestTail, ring.peek (k).grDb);
        check (shallowestTail > -0.02f && shallowestTail > deepest + 1.0f,
               juce::String ("grRecover: once the loud passage has passed, every entry reports the "
                             "reduction of ITS OWN samples and the trace returns to zero (deepest "
                             + juce::String (deepest, 2) + " dB in the passage, "
                             + juce::String (shallowestTail, 4) + " dB after it)").toRawUTF8());
        check (worstTail <= 0.0f,
               "grRecover: (premise) the entries report reduction, never gain");
        check (engine.lastBlockMinGain() > 0.9977f,
               "grRecover: …and the per-call GR meter recovers with it");
    }

    // ---- 7. A re-prepare during a partial accumulation: the partial follows
    //         the RING. Kept when the pair is unchanged, dropped when it
    //         changes. (Round 15 inverted the first half of this pass: it used
    //         to assert the drop in both cases, which was the review's
    //         blocking finding written down as an expectation.
    //         `testTheHistorySurvivesASameConfigurationRePrepare` is the full
    //         matrix; this stays here because the two halves belong beside the
    //         accumulation passes above them.) ----
    {
        const double rate = 48000.0;
        const int    B    = 512;
        anabasis::AnabasisEngine engine;
        anabasis::GrHistoryBuffer ring;
        ring.prepare (rate, B);
        engine.prepare (rate, B, 2);
        engine.setGrHistorySink (&ring);
        anabasis::EngineParameters p;
        juce::AudioBuffer<float> buf (2, B);

        // TWO FULL ENTRIES OF LOUD AUDIO FIRST, and the reason is the group
        // delay: the render is the input delayed by `groupDelaySamples()`, so a
        // partial entry fed before the pipeline has filled would carry silence
        // whether it survived a re-prepare or not, and the assertion below
        // would pass for the wrong reason. Two entries put loud audio in the
        // RENDER, and the 300 samples after them put it in the accumulator.
        //
        // THE BUFFER IS REFILLED BEFORE EVERY CALL because `process` works IN
        // PLACE: reusing it would feed the engine its own delayed output, and
        // three calls of that is digital silence — which is exactly how this
        // test passed for the wrong reason before the refill was added.
        const auto deliverLoud = [&] (int n)
        {
            for (int i = 0; i < n; ++i) { buf.setSample (0, i, 0.9f); buf.setSample (1, i, 0.9f); }
            juce::AudioBuffer<float> sub (buf.getArrayOfWritePointers(), 2, n);
            engine.process (sub, p);
        };
        deliverLoud (B);
        deliverLoud (B);
        check (ring.available() == 2 && ring.peek (1).peak > 0.5f,
               "grPrepared: (premise) the pipeline is full and the entries carry the loud render");
        deliverLoud (300);
        check (ring.available() == 2 && engine.lastRenderPeak() > 0.5f,
               "grPrepared: (premise) 300 samples of LOUD RENDER are sitting in a partial entry");
        check (ring.prepare (rate, B) == false,
               "grPrepared: (premise) a re-prepare at an unchanged pair keeps the ring's timeline");
        engine.prepare (rate, B, 2);
        engine.syncHistoryTimeline();   // …and the sync is a no-op, as it must be
        buf.clear();
        { juce::AudioBuffer<float> sub (buf.getArrayOfWritePointers(), 2, B); engine.process (sub, p); }
        // The COUNT does not discriminate here and the message says so: 300
        // carried plus 212 closes entry 2 and leaves 300, while 0 carried plus
        // 512 closes it and leaves 0 — both read 3. It earns its place against
        // a different mutant (one that publishes the partial early), and the
        // two checks after it are what separate carried from dropped.
        check (ring.available() == 3,
               "grPrepared: (premise) one more entry has closed, whichever samples closed it");
        check (ring.peek (2).peak > 0.5f,
               "grPrepared: it CARRIES the samples that were in flight — the ring kept this timeline, "
               "so the audio already folded into the unpublished entry is not thrown away with the "
               "run state");
        // …and the REMAINDER is carried with them, which the count above cannot
        // see: 300 of the 512 silent samples are still in the accumulator, so
        // 300 more close another entry. Dropped, they would not.
        buf.clear();
        { juce::AudioBuffer<float> sub (buf.getArrayOfWritePointers(), 2, 300); engine.process (sub, p); }
        check (ring.available() == 4,
               "grPrepared: …and so is the count they came with — 300 more samples close the next "
               "entry, which they could not if the accumulator had been zeroed and refilled");

        // A CHANGED pair clears the ring as well, and the new timeline's first
        // entry is one block of the NEW prepared size.
        deliverLoud (200);
        check (ring.prepare (rate, 256) == true,
               "grPrepared: (premise) a changed pair clears the ring");
        engine.prepare (rate, 256, 2);
        engine.syncHistoryTimeline();
        check (ring.available() == 0, "grPrepared: …so the old timeline's entries are gone");
        buf.clear();
        { juce::AudioBuffer<float> sub (buf.getArrayOfWritePointers(), 2, 256); engine.process (sub, p); }
        check (ring.available() == 1 && juce::exactlyEqual (ring.peek (0).peak, 0.0f),
               "grPrepared: the new configuration's first entry is one NEW prepared block of its own "
               "audio — the partial in flight was dropped with the timeline that ended, so nothing "
               "counted against a 512-sample entry part-fills a 256-sample one");
    }
}

// ===========================================================================
//  THE PARTIAL HISTORY ENTRY SURVIVES A SAME-CONFIGURATION RE-PREPARE
//  (0.2.12 round 15 — the PR review's blocking finding).
//
//  `GrHistoryBuffer::prepare` keeps the ring's entries when the (rate, block)
//  pair is unchanged, because hosts re-prepare on every transport start and
//  the display must continue rather than restart (0.1.2 item 6). Round 14's
//  producer-side accumulator did not follow that rule: `AnabasisEngine::
//  prepare` zeroed it unconditionally, so every pause/resume dropped up to
//  `maxBlock - 1` samples of already-rendered audio into a timeline that went
//  on running. Measured at 48 kHz / 512 before the repair: forty pause/resume
//  cycles over 8.783 s of audio published 800 entries where the audio was
//  worth 823 — 0.245 s of history gone — and a marker burst inside the partial
//  reached no entry at all while the ring kept every entry around it.
//
//  The rule is now the ring's own: the partial is carried when the pair is
//  unchanged and dropped when it changes, decided by the same comparison on
//  the same two raw values, so "the ring kept its entries" and "the engine
//  kept its partial" are one condition.
//
//  Every pass below is written against SAMPLE CONSERVATION — total samples
//  delivered is an exact multiple of B, so `entries * B == samples` is an
//  equality with no floor to hide behind — and against a MARKER whose render
//  falls inside the partial, so which entry received those samples is a fact
//  rather than an inference.
// ===========================================================================
namespace grpause
{
    struct Run
    {
        int64_t entries = 0;
        int64_t samples = 0;      // samples DELIVERED (== samples rendered)
        int     markerEntries = 0;
        int     markerIndex = -1;
        float   markerPeak = 0.0f;
        int     delay = 0;
        bool    ringCleared = false;   // what the RING decided at the break
    };

    enum class Break { none, samePair, rateChange, blockChange, explicitReset };

    // Prime with `pre` full blocks, deliver `partial` samples carrying a marker
    // in their RENDER, take the break, then deliver enough to bring the total
    // to an exact multiple of the (possibly new) block size.
    static Run run (double rate, int B, int pre, int partial, int post, Break brk,
                    int cycles = 1)
    {
        Run r;
        anabasis::AnabasisEngine engine;
        anabasis::GrHistoryBuffer ring;
        ring.prepare (rate, B);
        engine.prepare (rate, B, 2);
        engine.setGrHistorySink (&ring);
        r.delay = engine.groupDelaySamples();
        anabasis::EngineParameters p;

        const int   markerLen = 64;
        const float baseAmp   = 0.20f;
        const float markerAmp = 0.85f;
        // The render is the input delayed by the group delay, so a burst that
        // must be HEARD ten samples into the partial is fed that much earlier.
        const int   markerIn  = pre * B - r.delay + 10;

        int t = 0;                                     // global INPUT index
        juce::AudioBuffer<float> buf (2, juce::jmax (B, 1));
        // Refilled before every call: `process` works IN PLACE, so a re-used
        // buffer feeds the engine its own delayed output.
        const auto deliver = [&] (int n)
        {
            for (int i = 0; i < n; ++i, ++t)
            {
                const bool marked = t >= markerIn && t < markerIn + markerLen;
                const float v = marked ? markerAmp : baseAmp;
                buf.setSample (0, i, v);
                buf.setSample (1, i, v);
            }
            juce::AudioBuffer<float> sub (buf.getArrayOfWritePointers(), 2, n);
            engine.process (sub, p);
            r.samples += n;
        };

        for (int c = 0; c < cycles; ++c)
        {
            for (int b = 0; b < pre; ++b) deliver (B);
            if (partial > 0) deliver (partial);
            switch (brk)
            {
                case Break::none:          break;
                case Break::samePair:      engine.prepare (rate, B, 2);
                                           r.ringCleared = ring.prepare (rate, B);       break;
                case Break::rateChange:    engine.prepare (rate * 2.0, B, 2);
                                           r.ringCleared = ring.prepare (rate * 2.0, B); break;
                case Break::blockChange:   engine.prepare (rate, B / 2, 2);
                                           r.ringCleared = ring.prepare (rate, B / 2);   break;
                case Break::explicitReset: engine.reset();                      break;
            }
            // What the wrapper does after anything that can clear the ring
            // (`AnabasisAudioProcessor::prepareToPlay`): give the engine the
            // moment to ask the ring which timeline it is on. A no-op when the
            // epoch has not moved, which is every case but the two changes.
            engine.syncHistoryTimeline();
            const int nowB = brk == Break::blockChange ? B / 2 : B;
            for (int b = 0; b < post; ++b) deliver (nowB);
            // …and, for a SINGLE-cycle run, top the total up to a whole number
            // of the current entry size so the conservation check is an
            // equality with no floor in it. A REPEATED run must not do that:
            // a cycle that is an exact multiple of the entry size leaves the
            // accumulator empty at every break after the first, so at most one
            // prepared block could ever be lost however many times it repeats,
            // and the repetition would prove nothing. Without the top-up the
            // grid re-phases at every break, which is what makes the loss
            // cumulative and the count below worth taking.
            if (partial > 0 && cycles == 1) deliver (nowB - (partial % nowB));
        }

        r.entries = ring.available();
        for (int64_t k = 0; k < r.entries; ++k)
        {
            const auto e = ring.peek (k);
            if (e.peak > 0.5f) { ++r.markerEntries; r.markerIndex = (int) k; r.markerPeak = e.peak; }
        }
        return r;
    }
}

static void testTheHistorySurvivesASameConfigurationRePrepare()
{
    using grpause::run;
    using Break = grpause::Break;
    const double rate = 48000.0;
    const int    B    = 512;

    // ---- 1. The control: no break at all. Everything below is measured
    //         against this, so "unchanged" is a comparison and not a guess. ----
    const auto control = run (rate, B, 4, 300, 6, Break::none);
    check (control.samples % B == 0 && control.entries == control.samples / B,
           "grPause: (premise) with no break the entries account for every sample delivered");
    check (control.markerEntries == 1,
           "grPause: (premise) the marker's render lands in exactly one entry");

    // ---- 2. Same-configuration re-prepare: zero, small, and nearly-full
    //         partials, and one taken immediately before a boundary. ----
    for (int partial : { 0, 1, 100, 300, B - 1 })
    {
        const auto r = run (rate, B, 4, partial, 6, Break::samePair);
        check (r.samples % B == 0 && r.entries == r.samples / B,
               juce::String ("grPause: a same-configuration re-prepare with " + juce::String (partial)
                             + " samples in the partial entry loses none of them — the entries still "
                               "account for every sample delivered").toRawUTF8());
        if (partial > 10)   // …below that the marker has not been rendered yet
            check (r.markerEntries == 1 && r.markerIndex == control.markerIndex,
                   juce::String ("grPause: …and the marker reaches the SAME entry it reaches with no "
                                 "break at all (partial " + juce::String (partial) + ")").toRawUTF8());
    }

    // ---- 2b. THE BOUNDARY CASE, asserted exactly rather than in aggregate.
    //          With `B - 1` samples in flight the very next sample must close
    //          the entry; zero the accumulator instead and it takes `B`. One
    //          sample is the whole difference, and nothing else in this test
    //          measures it that sharply. ----
    {
        anabasis::AnabasisEngine engine;
        anabasis::GrHistoryBuffer ring;
        ring.prepare (rate, B);
        engine.prepare (rate, B, 2);
        engine.setGrHistorySink (&ring);
        anabasis::EngineParameters p;
        juce::AudioBuffer<float> buf (2, B);
        buf.clear();
        const auto feed = [&] (int n)
        {
            juce::AudioBuffer<float> sub (buf.getArrayOfWritePointers(), 2, n);
            engine.process (sub, p);
        };
        feed (B);                       // one whole entry, so the ring is non-empty
        feed (B - 1);                   // …and the next is one sample short
        check (ring.available() == 1, "grPause: (premise) the entry in flight is one sample short");
        check (ring.prepare (rate, B) == false, "grPause: (premise) the ring keeps this timeline");
        engine.prepare (rate, B, 2);
        engine.syncHistoryTimeline();
        feed (1);
        check (ring.available() == 2,
               "grPause: a re-prepare taken one sample before an entry boundary still lets the NEXT "
               "sample close that entry — the count in flight is carried, not just the statistics");
    }

    // ---- 3. Repeated pause/resume, which is what a session actually does —
    //         and it is the CUMULATIVE loss that makes this a defect rather
    //         than a rounding error. This is the stimulus the round's quoted
    //         figures come from: forty cycles of twenty 512-sample blocks and
    //         one 300-sample block, 421 600 samples, 8.783 s. Round 14's
    //         engine publishes 800 entries here; the audio is worth 823. ----
    {
        const auto r = run (rate, B, 20, 300, 0, Break::samePair, 40);
        check (r.samples == 421600 && r.samples / B == 823,
               "grPause: (premise) the repeated stimulus is the one the round's figures quote");
        check (r.entries == r.samples / B,
               juce::String ("grPause: forty pause/resume cycles lose nothing — " + juce::String ((int) r.entries)
                             + " entries for " + juce::String ((double) r.samples / rate, 3)
                             + " s of audio, against the " + juce::String ((int) (r.samples / B))
                             + " the audio is worth").toRawUTF8());
    }

    // ---- 4. A CHANGED pair must still drop it: those samples were recorded
    //         under a time base the new timeline does not have, and the ring
    //         clears at exactly that prepare. ----
    for (Break brk : { Break::rateChange, Break::blockChange })
    {
        const auto r = run (rate, B, 4, 300, 6, brk);
        check (r.markerEntries == 0,
               brk == Break::rateChange
                   ? "grPause: a sample-rate change drops the partial — no audio recorded under the old "
                     "rate reaches the new timeline's first entry"
                   : "grPause: a prepared-block-size change drops the partial — an entry of the new size "
                     "cannot be part-filled with samples counted against the old one");
        check (r.entries > 0,
               "grPause: (premise) the new configuration does publish entries of its own");
    }
    // …and the COUNT in flight is dropped with the statistics, which the marker
    // alone cannot see: leave `histSamples` behind across a 512 -> 256 change
    // and it already exceeds the new entry size, so the very first sample of
    // the new timeline closes an entry that stands for one sample.
    {
        anabasis::AnabasisEngine engine;
        anabasis::GrHistoryBuffer ring;
        ring.prepare (rate, B);
        engine.prepare (rate, B, 2);
        engine.setGrHistorySink (&ring);
        anabasis::EngineParameters p;
        juce::AudioBuffer<float> buf (2, B);
        buf.clear();
        const auto feed = [&] (int n)
        {
            juce::AudioBuffer<float> sub (buf.getArrayOfWritePointers(), 2, n);
            engine.process (sub, p);
        };
        feed (B);
        feed (300);                                   // 300 in flight against a 512 entry
        check (ring.prepare (rate, B / 2) && ring.available() == 0,
               "grPause: (premise) the block change cleared the ring");
        engine.prepare (rate, B / 2, 2);
        engine.syncHistoryTimeline();
        feed (B / 2 - 1);
        check (ring.available() == 0,
               "grPause: 255 samples of a 256-sample timeline publish nothing — the count in flight "
               "was dropped with the statistics, so the new configuration's first entry is a whole "
               "one and not a stub left over from the old entry size");
        feed (1);
        check (ring.available() == 1,
               "grPause: …and the 256th sample closes it");
    }

    // ---- 5. An explicit `reset()` that clears NO ring state does not drop the
    //         partial: the ring's timeline is still running, so the samples in
    //         flight still belong to it. That is not a special case for
    //         `reset()` — round 16 made it the general rule by having the
    //         engine read the ring's epoch, so what matters is whether the
    //         RING restarted, not which function was called.
    //         `testTheHistoryTimelineIsTheRingsTimeline` covers the other
    //         side: a reset that DOES clear the ring drops it. ----
    {
        const auto r = run (rate, B, 4, 300, 6, Break::explicitReset);
        check (r.markerEntries == 1 && r.markerIndex == control.markerIndex,
               "grPause: an explicit reset() clears the run state and leaves the partial history entry "
               "alone — it clears nothing in the ring, so it discards nothing from the ring's timeline");
        check (r.samples % B == 0 && r.entries == r.samples / B,
               "grPause: …so a reset conserves samples exactly as a same-configuration re-prepare does");
    }

    // ---- 6. ONE DECISION, NOT TWO. The engine keeps its partial exactly when
    //         the ring keeps its entries — asserted as the joint statement
    //         rather than as two behaviours that happen to line up, because
    //         the failure this round repaired was precisely the two of them
    //         disagreeing. `reset()` is excluded and stated separately above:
    //         it does not touch the ring at all. ----
    {
        bool joint = true;
        for (Break brk : { Break::samePair, Break::rateChange, Break::blockChange })
        {
            const auto r = run (rate, B, 4, 300, 6, brk);
            if ((r.markerEntries > 0) != ! r.ringCleared) joint = false;
        }
        check (joint,
               "grPause: the partial entry reaches an entry exactly when the ring kept its timeline — "
               "the engine's gate and the ring's clear-on-change gate are one decision on one pair");
    }
}

// ===========================================================================
//  THE PARTIAL ENTRY'S TIMELINE IS THE RING'S TIMELINE (0.2.12 round 16 — the
//  PR review's second blocking finding, "explicit resets leak old history").
//
//  Rounds 14 and 15 both had the ENGINE decide the partial entry's fate, and
//  got it wrong in opposite directions. Round 14 dropped it on every `prepare`,
//  losing a prepared block of rendered audio on every transport start. Round 15
//  mirrored `GrHistoryBuffer::prepare`'s clear-on-change comparison so the two
//  agreed — and they did, for `prepare`. They did not agree for a ring cleared
//  any OTHER way: `GrHistoryBuffer::reset()` rewinds the ring to a fresh
//  timeline without going through `prepare` at all, and a partial accumulated
//  under the old one survived into it. Measured before the repair, 48 kHz/512
//  with 511 samples in flight: the new timeline's FIRST entry closed on ONE
//  post-reset sample and carried the previous timeline's peak.
//
//  The engine no longer decides. `resetGuard` — the epoch the ring already
//  publishes — changes on every `clear` and on nothing else, so it IS the
//  timeline's identity; the engine records the epoch its partial was
//  accumulated under and discards the partial when they part company.
//
//  EVERY PASS BELOW MEASURES THE SAME STRUCTURAL FACT: how many POST-break
//  samples the first entry after the break stands for. That number is `B` for
//  a new timeline and `B - carried` for a continued one, it is exact, and it
//  does not depend on what the render pipeline happens to hold — which the
//  peak alone does, since an engine that is NOT reset keeps rendering
//  pre-break audio out of its lookahead line quite legitimately.
// ===========================================================================
namespace grepoch
{
    enum class Break { none, engineReset, ringReset, bothReset, samePair, changedPair };

    struct Run
    {
        int      firstEntryNewSamples = -1;   // post-break samples the first new entry took
        float    firstPeak = 0.0f;            // …and the statistics it carried
        int64_t  entriesBefore = 0, entriesAtBreak = 0, entriesAfter = 0;
        uint32_t epochBefore = 0, epochAfter = 0;
        int      delay = 0;
    };

    // Prime `pre` whole entries, leave `partial` samples in flight carrying a
    // LOUD marker in their render, take the break, then feed post-break audio
    // ONE SAMPLE AT A TIME until the first entry closes.
    static Run run (double rate, int B, int pre, int partial, Break brk)
    {
        Run r;
        anabasis::AnabasisEngine engine;
        anabasis::GrHistoryBuffer ring;
        ring.prepare (rate, B);
        engine.prepare (rate, B, 2);
        engine.setGrHistorySink (&ring);
        r.delay = engine.groupDelaySamples();
        anabasis::EngineParameters p;

        const float base = 0.20f, preAmp = 0.85f;
        const int   markerLen = 64;
        const int   markerIn  = pre * B - r.delay + 10;   // renders inside the partial
        int t = 0;
        juce::AudioBuffer<float> buf (2, B);
        const auto deliver = [&] (int n)
        {
            for (int i = 0; i < n; ++i, ++t)
            {
                const bool marked = t >= markerIn && t < markerIn + markerLen;
                buf.setSample (0, i, marked ? preAmp : base);
                buf.setSample (1, i, marked ? preAmp : base);
            }
            juce::AudioBuffer<float> sub (buf.getArrayOfWritePointers(), 2, n);
            engine.process (sub, p);
        };

        for (int b = 0; b < pre; ++b) deliver (B);
        if (partial > 0) deliver (partial);
        r.entriesBefore = ring.available();
        r.epochBefore   = ring.resetEpoch();

        const int newB = brk == Break::changedPair ? B / 2 : B;
        switch (brk)
        {
            // The welded entry points, which are what production uses: the
            // ring's own call with the engine's timeline sync attached, so a
            // clear cannot happen without the engine re-reading the epoch.
            case Break::none:        break;
            case Break::engineReset: engine.reset(); engine.syncHistoryTimeline();       break;
            case Break::ringReset:   engine.resetHistoryTimeline();                      break;
            case Break::bothReset:   engine.reset(); engine.resetHistoryTimeline();      break;
            case Break::samePair:    engine.prepare (rate, B, 2);
                                     engine.prepareHistoryTimeline (rate, B);            break;
            case Break::changedPair: engine.prepare (rate, newB, 2);
                                     engine.prepareHistoryTimeline (rate, newB);         break;
        }
        r.epochAfter    = ring.resetEpoch();
        r.entriesAtBreak = ring.available();
        const int64_t floor0 = r.entriesAtBreak;

        for (int n = 0; n < 4 * newB; ++n)
        {
            deliver (1);
            if (ring.available() > floor0)
            {
                r.firstEntryNewSamples = n + 1;
                r.firstPeak = ring.peek (floor0).peak;
                break;
            }
        }
        r.entriesAfter = ring.available();
        return r;
    }
}

static void testTheHistoryTimelineIsTheRingsTimeline()
{
    using grepoch::run;
    using Break = grepoch::Break;
    const double rate = 48000.0;
    const int    B    = 512;

    // ---- A. NO PARTIAL ENTRY: a break taken exactly on a boundary is
    //         indistinguishable from no break at all, however it is taken. ----
    for (Break brk : { Break::none, Break::engineReset, Break::ringReset,
                       Break::bothReset, Break::samePair })
    {
        const auto r = run (rate, B, 4, 0, brk);
        check (r.firstEntryNewSamples == B,
               "grEpoch: with nothing in flight, the first entry after ANY break stands for a whole "
               "prepared block of post-break audio");
    }

    // ---- B and C. A PARTIAL EXISTS and the ring starts a fresh timeline: the
    //         first entry of that timeline must stand for B of ITS OWN samples
    //         and carry none of the previous timeline's statistics. `B - 1` is
    //         the sharpest case — before the repair that entry closed on ONE
    //         post-reset sample. ----
    for (int partial : { 1, 100, 300, B - 1 })
        for (Break brk : { Break::ringReset, Break::bothReset })
        {
            const auto r = run (rate, B, 4, partial, brk);
            check (r.epochAfter != r.epochBefore && r.entriesBefore > 0 && r.entriesAtBreak == 0,
                   "grEpoch: (premise) the ring started a fresh timeline — new epoch, index rewound");
            check (r.firstEntryNewSamples == B,
                   juce::String ("grEpoch: …so its first entry stands for a whole prepared block of "
                                 "post-reset audio, not for the " + juce::String (B - partial)
                                 + " it would need if the partial had crossed").toRawUTF8());
        }

    // …and the statistics with it, asserted where the engine was reset too so
    // the render pipeline cannot supply the marker by legitimate means.
    for (int partial : { 300, B - 1 })
    {
        const auto r = run (rate, B, 4, partial, Break::bothReset);
        check (r.firstPeak < 0.5f,
               "grEpoch: …and it carries none of the previous timeline's statistics — the loud "
               "marker folded into the partial before the reset is gone with it");
    }

    // …BOTH statistics, not just the peak. The passes above run on sub-ceiling
    // material, where every entry's grDb is 0 and a leaked one would look
    // exactly like a fresh one. This drives real reduction into the partial and
    // then resets: the new timeline's first entry must report none of it.
    {
        anabasis::AnabasisEngine engine;
        anabasis::GrHistoryBuffer ring;
        ring.prepare (rate, B);
        engine.prepare (rate, B, 2);
        engine.setGrHistorySink (&ring);
        anabasis::EngineParameters p;
        juce::AudioBuffer<float> buf (2, B);
        int t = 0;
        const auto feedHot = [&] (int n)
        {
            for (int i = 0; i < n; ++i, ++t)
            {
                const float v = 1.60f * std::sin (0.05f * (float) t);   // well over the ceiling
                buf.setSample (0, i, v);
                buf.setSample (1, i, v);
            }
            juce::AudioBuffer<float> sub (buf.getArrayOfWritePointers(), 2, n);
            engine.process (sub, p);
        };
        feedHot (B);
        feedHot (B);
        feedHot (400);                       // …400 samples of deep reduction in flight
        check (ring.available() == 2 && ring.peek (1).grDb < -0.5f,
               "grEpoch: (premise) the partial in flight carries real gain reduction");
        engine.reset();
        engine.resetHistoryTimeline();
        buf.clear();
        int n = 0;
        for (; n < 2 * B; ++n)
        {
            juce::AudioBuffer<float> sub (buf.getArrayOfWritePointers(), 2, 1);
            engine.process (sub, p);
            if (ring.available() > 0) break;
        }
        check (n + 1 == B && ring.peek (0).grDb > -0.01f,
               "grEpoch: the reduction folded into the partial before a reset does not reach the new "
               "timeline's first entry either — the grDb statistic goes with the peak");
    }

    // ---- H. THE ACCUMULATOR'S OWN WELL-FORMEDNESS. A caller that shrinks the
    //         engine's prepared block without touching the ring leaves a count
    //         that no entry of the new size can hold. The wrapper never does
    //         this — it prepares both — but `prepare` keeps the invariant
    //         rather than letting the chunk loop publish an entry standing for
    //         more audio than its own size. ----
    {
        anabasis::AnabasisEngine engine;
        anabasis::GrHistoryBuffer ring;
        ring.prepare (rate, B);
        engine.prepare (rate, B, 2);
        engine.setGrHistorySink (&ring);
        anabasis::EngineParameters p;
        juce::AudioBuffer<float> buf (2, B);
        buf.clear();
        const auto feed = [&] (int n)
        {
            juce::AudioBuffer<float> sub (buf.getArrayOfWritePointers(), 2, n);
            engine.process (sub, p);
        };
        feed (B);
        feed (300);                          // 300 counted against a 512-sample entry
        engine.prepare (rate, 256, 2);       // …and the ring is deliberately left alone
        engine.syncHistoryTimeline();        // a no-op: the ring's epoch did not move
        int n = 0;
        for (; n < 2 * 256; ++n) { feed (1); if (ring.available() > 1) break; }
        check (n + 1 == 256,
               "grEpoch: shrinking the prepared block without touching the ring drops the partial "
               "rather than publishing an entry that stands for more audio than its own size");
    }

    // ---- D. A RESET IMMEDIATELY AFTER AN ENTRY WAS PUBLISHED. Nothing is in
    //         flight, so nothing can cross; the ring's own contract governs the
    //         completed entries and is unchanged. ----
    {
        const auto r = run (rate, B, 4, 0, Break::bothReset);
        check (r.entriesBefore == 4,
               "grEpoch: (premise) four whole entries were published before the reset");
        check (r.firstEntryNewSamples == B && r.firstPeak < 0.5f,
               "grEpoch: a reset taken immediately after an entry was published leaks nothing, and "
               "the new timeline's first entry is a whole one");
    }

    // ---- E. SAME-CONFIGURATION RE-PREPARE — round 15's repair, unchanged. A
    //         same-pair `GrHistoryBuffer::prepare` is a total no-op, so the
    //         timeline continues and the partial completes it. ----
    for (int partial : { 1, 300, B - 1 })
    {
        const auto r = run (rate, B, 4, partial, Break::samePair);
        check (r.epochAfter == r.epochBefore,
               "grEpoch: (premise) a same-pair re-prepare does not touch the ring's epoch");
        check (r.firstEntryNewSamples == B - partial,
               juce::String ("grEpoch: …so the entry in flight completes on its remaining "
                             + juce::String (B - partial) + " samples — a transport start still "
                             "loses nothing").toRawUTF8());
    }

    // ---- E2. AN ENGINE reset that does NOT accompany a ring clear. The ring's
    //          timeline continued, so the partial belongs to it and stays. This
    //          is the case round 15 established and this round must not undo. ----
    for (int partial : { 300, B - 1 })
    {
        const auto r = run (rate, B, 4, partial, Break::engineReset);
        check (r.epochAfter == r.epochBefore && r.firstEntryNewSamples == B - partial,
               "grEpoch: an engine reset that clears no ring state leaves the partial with the "
               "timeline that is still running");
    }

    // ---- F. CONFIGURATION-CHANGING PREPARE — the old partial is discarded and
    //         the new timeline's first entry is a whole entry of the NEW size. ----
    for (int partial : { 300, B - 1 })
    {
        const auto r = run (rate, B, 4, partial, Break::changedPair);
        check (r.epochAfter != r.epochBefore && r.firstEntryNewSamples == B / 2 && r.firstPeak < 0.5f,
               "grEpoch: a prepared-block-size change discards the old partial — the new timeline's "
               "first entry is a whole 256-sample one carrying only its own audio");
    }

    // ---- F2. ATTACHING A SINK is a timeline boundary of its own. An engine
    //          with no ring accumulates all the same — only `push` is guarded —
    //          so audio processed before a sink existed belongs to no timeline
    //          and must not complete the first entry of the one it is given. ----
    {
        anabasis::AnabasisEngine engine;
        anabasis::GrHistoryBuffer ring;
        ring.prepare (rate, B);
        engine.prepare (rate, B, 2);
        anabasis::EngineParameters p;
        juce::AudioBuffer<float> buf (2, B);
        buf.clear();
        const auto feed = [&] (int n)
        {
            juce::AudioBuffer<float> sub (buf.getArrayOfWritePointers(), 2, n);
            engine.process (sub, p);
        };
        feed (B);
        feed (B - 1);                       // …accumulated with nowhere to go
        check (ring.available() == 0, "grEpoch: (premise) an engine with no sink publishes nothing");
        engine.setGrHistorySink (&ring);   // adopts the ring's timeline by itself
        int n = 0;
        for (; n < 2 * B; ++n) { feed (1); if (ring.available() > 0) break; }
        check (n + 1 == B,
               "grEpoch: the first entry after a sink is attached stands for a whole prepared block "
               "of audio processed SINCE the attachment — what came before belonged to no timeline");
    }

    // ---- G. REPEATED TRANSITIONS, ownership checked after every one. ----
    {
        anabasis::AnabasisEngine engine;
        anabasis::GrHistoryBuffer ring;
        ring.prepare (rate, B);
        engine.prepare (rate, B, 2);
        engine.setGrHistorySink (&ring);
        anabasis::EngineParameters p;
        juce::AudioBuffer<float> buf (2, B);
        buf.clear();
        const auto feed = [&] (int n)
        {
            juce::AudioBuffer<float> sub (buf.getArrayOfWritePointers(), 2, n);
            engine.process (sub, p);
        };
        // Each step leaves 300 samples in flight, then takes a transition, then
        // asks how many post-transition samples the next entry needs: 512 if the
        // transition started a new timeline, 212 if it continued the old one.
        const auto probe = [&] (int expectNew, const char* what)
        {
            const int64_t before = ring.available();
            int n = 0;
            for (; n < 2 * B; ++n) { feed (1); if (ring.available() > before) break; }
            check (n + 1 == expectNew, what);
        };
        const auto sync = [&] { engine.syncHistoryTimeline(); };
        feed (B); feed (300);
        engine.reset(); sync();                           // ring untouched -> continues
        probe (B - 300, "grEpoch: (G) an engine reset alone continues the timeline");
        feed (300);
        engine.prepare (rate, B, 2); engine.prepareHistoryTimeline (rate, B);   // same pair
        probe (B - 300, "grEpoch: (G) a same-configuration re-prepare continues it too");
        feed (300);
        engine.reset(); engine.resetHistoryTimeline();    // ring cleared -> new timeline
        probe (B, "grEpoch: (G) a reset that clears the ring starts a new one");
        feed (300);
        engine.prepare (rate, 256, 2); engine.prepareHistoryTimeline (rate, 256);  // changed
        probe (256, "grEpoch: (G) …and so does a configuration change, at the new entry size");
        feed (100);
        engine.resetHistoryTimeline();                    // the ring alone, no engine reconfigure
        probe (256, "grEpoch: (G) …and a ring reset with no engine reconfiguration at all");
    }
}

// ---------------------------------------------------------------------------
// THE PRODUCER'S HALF OF THE DURATION CONTRACT (0.2.12 round 17). The ring's
// capacity is a duration at every prepared pair — `kSize · block / rate` — and
// `GrHistoryView`'s window is one slot less than that; the seconds themselves
// are pinned on the view's side
// (`testTheHistoryWindowKeepsItsSecondsAcrossThePreparedPairs`, state suite).
// What only the ENGINE can show is that those entries are the ones it
// published: at 192 kHz with a 32-sample prepared block it publishes 6000 a
// second, so one second of processed audio is already half again what the
// 4096-entry ring could hold, and the entry standing for the first block of it
// had been overwritten before the second was over. A marker in that first
// block, read back through the ring's own index, is the whole proof.
static void testTheRingKeepsASecondOfEntriesAtTheSmallestPreparedBlock()
{
    using anabasis::GrHistoryBuffer;
    using anabasis::AnabasisEngine;
    using anabasis::EngineParameters;
    const double rate = 192000.0;
    const int    B    = 32;

    AnabasisEngine engine;
    const auto ringStorage = std::make_unique<GrHistoryBuffer>();   // a megabyte: heap, never stack
    auto& ring = *ringStorage;
    ring.prepare (rate, B);
    engine.prepare (rate, B, 2);
    engine.setGrHistorySink (&ring);
    engine.prepareHistoryTimeline (rate, B);

    EngineParameters p;
    juce::AudioBuffer<float> buf (2, B);
    const int64_t total = (int64_t) rate;                 // one second of audio
    const int     marked = 6 * B;                         // the first six entries carry it
    int64_t t = 0;
    for (int64_t done = 0; done < total; done += B)
    {
        for (int i = 0; i < B; ++i, ++t)
        {
            const float v = t < marked ? 0.95f : 0.05f;
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
        }
        engine.process (buf, p);
    }

    const int64_t entries = ring.available();
    check (entries == total / B,
           "grHold: (premise) one second at 192 kHz / 32 is 6000 entries — one per prepared block, as ADR-0011's amendment has it");
    check (entries > 4095,
           "grHold: (premise) …which is more than the ring held in total before round 17");
    check (entries - 0 <= (int64_t) GrHistoryBuffer::kSize - 1,
           "grHold: every one of them is still inside the lap a reader may peek — entry 0 included");

    // …and they are still the FIRST second's audio, not later entries wearing
    // those indices. The engine's own group delay puts the marker's render
    // exactly `groupDelaySamples / B` entries in — 60 at this pair — so the
    // marker is looked for where the latency says it lands rather than at 0,
    // and finding it there is what says index 0 has not been re-used.
    const int64_t delayEntries = (int64_t) engine.groupDelaySamples() / B;
    int64_t loud = 0, firstLoud = -1, lastLoud = -1;
    for (int64_t k = 0; k < entries; ++k)
        if (ring.peek (k).peak > 0.5f)
        {
            ++loud;
            lastLoud = k;
            if (firstLoud < 0) firstLoud = k;
        }
    check (loud == marked / B && firstLoud == delayEntries
             && lastLoud == delayEntries + (int64_t) (marked / B) - 1,
           "grHold: …and the marker is still where the group delay put it, so those indices are the FIRST second's");
}




// ---------------------------------------------------------------------------
// inv 9: non-finite input never leaves the engine, and it self-heals.
static void testNoBadSamples()
{
    anabasis::AnabasisEngine engine;
    engine.prepare (48000.0, 512, 2);
    anabasis::EngineParameters p;
    juce::AudioBuffer<float> buf (2, 512);

    for (int b = 0; b < 12; ++b)
    {
        for (int n = 0; n < 512; ++n)
        {
            float v = 0.4f * std::sin (0.01f * (float) n);
            if (b == 3) v = std::numeric_limits<float>::quiet_NaN();
            if (b == 4) v = std::numeric_limits<float>::infinity();
            if (b == 5) v = 1.0e-38f;    // denormal territory
            buf.setSample (0, n, v);
            buf.setSample (1, n, v);
        }
        engine.process (buf, p);
        for (int n = 0; n < 512; ++n)
        {
            if (! std::isfinite (buf.getSample (0, n)) || ! std::isfinite (buf.getSample (1, n)))
            {
                check (false, "noBadSamples: non-finite sample left the engine");
                return;
            }
        }
    }
    check (true, "noBadSamples: all outputs finite under hostile input");
}

// ---------------------------------------------------------------------------
// inv 9, the other half: a FINITE input can still make a stage produce a
// non-finite value, and the boundaries that substitute 0.0f do not clean the
// STATE that produced it. Every stage below overflows on a legal float and
// then holds NaN for ever — the engine went permanently silent until the host
// re-prepared it, which is not "graceful degradation" by any reading.
//
// The stimulus is per stage because each overflows on a different quantity,
// and a case only reaches its stage if the ones before it do NOT overflow:
//   • EQ    — biquad gain: b0·x goes infinite once |x| is within a few dB of
//             FLT_MAX, and every later stage then sees the inf
//   • comp  — the RMS detector SQUARES its input, so ~1.8e19 is its ceiling;
//             level = inf makes the GR target -inf and the next sample's
//             -inf + inf a NaN the envelope keeps
//   • clip  — the Transistor colour model's c⁵ term, so ~5e7. Drive must stay
//             at 0: the clipper's own transfer function BOUNDS its output, so
//             a driven clipper protects the colour polynomial from this
//   • OS     — the polyphase filters carry gain, so a huge-but-finite staged
//             sample comes out of processSamplesUp infinite. Needs the PEAK
//             detector, or the compressor squares first and collapses the
//             level before it reaches the region
// Each case dies against exactly one element of the fix being reverted (the
// matching reset, or the boundary that records the substitution), which is why
// all four are here rather than one representative case.
static void testExtremeLevelDoesNotSilencePermanently()
{
    const double sr = 48000.0;
    const int block = 512;

    auto run = [&] (const char* what, float hugeValue, auto&& configure)
    {
        anabasis::AnabasisEngine engine;
        engine.prepare (sr, block, 2);
        anabasis::EngineParameters p;
        configure (p);
        juce::AudioBuffer<float> buf (2, block);

        auto tone = [&] (int b)
        {
            for (int n = 0; n < block; ++n)
            {
                const float ph = 2.0f * 3.14159265f * 220.0f * (float) (b * block + n) / (float) sr;
                const float v = 0.2f * std::sin (ph);
                buf.setSample (0, n, v);
                buf.setSample (1, n, v);
            }
        };
        auto rms = [&]
        {
            double sum = 0.0;
            for (int n = 0; n < block; ++n)
            {
                const double s = buf.getSample (0, n);
                sum += s * s;
            }
            return std::sqrt (sum / (double) block);
        };

        for (int b = 0; b < 6; ++b) { tone (b); engine.process (buf, p); }
        const double healthy = rms();

        for (int n = 0; n < block; ++n)              // one block, finite, absurd
        {
            buf.setSample (0, n, hugeValue);
            buf.setSample (1, n, hugeValue);
        }
        engine.process (buf, p);

        double after = 0.0;
        for (int b = 7; b < 207; ++b) { tone (b); engine.process (buf, p); after = rms(); }

        // Recovery, not equality: the block really happened, so the adaptive
        // trims and the limiter envelope legitimately moved. Permanent silence
        // reads 0.0 exactly, so the two are nowhere near each other.
        check (healthy > 0.05,
               (juce::String ("extremeLevel: (test premise) ") + what
                    + " runs at level before the event").toRawUTF8());
        check (after > 0.25 * healthy,
               (juce::String ("extremeLevel: recovers after ") + what).toRawUTF8());
    };

    run ("an EQ biquad overflows", 0.5f * std::numeric_limits<float>::max(),
         [] (anabasis::EngineParameters& p) { p.eqHighShelfGainDb = 12.0f; });

    run ("the compressor's RMS detector overflows", 1.0e20f,
         [] (anabasis::EngineParameters& p) { p.compDetector = 0; });

    run ("the clipper's colour polynomial overflows", 1.0e10f,
         [] (anabasis::EngineParameters& p)
         {
             p.colourModel = 3;                      // Transistor: the c⁵ term
             p.colourDepth = 1.0f;
         });

    // POST position. Calibrated, because the obvious stimulus does NOT reach
    // the bug: the limiter's attack is what bounds stage E's input, so at the
    // default 2 ms lookahead the envelope is down to ~0.008 by the time the
    // peak plays and no legal EQ boost gets the product back over FLT_MAX. At
    // 0.1 ms of lookahead the envelope only reaches ~0.29 in the ~5 samples it
    // has, and a fully boosted EQ multiplies by ~3.4. The peak detector keeps
    // the compressor's RMS square from collapsing the level first.
    run ("an EQ biquad overflows in the POST position", std::numeric_limits<float>::max(),
         [] (anabasis::EngineParameters& p)
         {
             p.eqPosition   = 1;                 // after the limiter, before the clamp
             p.lookaheadMs  = 0.1f;
             p.compDetector = 1;
             p.eqTiltDb          = 3.0f;
             p.eqLowShelfGainDb  = 12.0f;
             p.eqHighShelfGainDb = 12.0f;
             p.eqBell1GainDb     = 12.0f;
             p.eqBell2GainDb     = 12.0f;
         });

    run ("the oversampler's filters overflow", std::numeric_limits<float>::max(),
         [] (anabasis::EngineParameters& p)
         {
             p.oversample   = anabasis::OversampleFactor::x4;
             p.compDetector = 1;                     // peak: no square to overflow first
             p.colourModel  = 3;
             p.colourDepth  = 1.0f;
         });

    // ---- SUSTAINED, and on ONE CHANNEL ------------------------------------
    // The runs above all drive a SINGLE block of the extreme value into BOTH
    // channels, and that is what let the KI-009 failure hide behind them for
    // three rounds: the fault this covers is neither transient nor symmetric.
    //
    // The mechanism, and why it is the invariant rather than a knob value.
    // Every non-finite boundary in `processChunk` substitutes exactly 0.0f
    // **for the offending channel alone** — that is deliberate and correct as
    // a propagation bound. What is NOT correct is a stage that keeps
    // regenerating a non-finite value from a perfectly finite input: the
    // boundary then emits digital zero on that channel for as long as the
    // input persists, and invariant 9's repair for the stage
    // (`clip.sanitiseState()`) cannot help, because nothing about the stage's
    // STATE is wrong. The result is ONE channel permanently silent while the
    // other plays — and, because a stage only reaches the ring when its own
    // mix lets it, gated on that stage's Mix control. That is the whole of
    // KI-009's field fingerprint.
    //
    // So the assertion is the general one: a sustained finite input, however
    // absurd its magnitude, must leave BOTH channels alive at EVERY Clip Mix.
    // The mutant it kills is the unbounded colour polynomial (ClipSat's `c⁵`
    // evaluated on the raw sample once `clipDrive == 0` exact-skips the
    // clipper's own bound) — restore it and the mix == 1 rows below read
    // exactly 0.0 on the hot channel.
    {
        // UNDER THE SHIPPING RUNTIME'S FTZ/DAZ, which this suite otherwise
        // does not have. `AnabasisAudioProcessor::processBlock` opens with
        // `juce::ScopedNoDenormals`; the DSP suite calls `AnabasisEngine`
        // directly, so without this the two hottest rows would pass on a
        // SUBNORMAL limiter gain (`ceiling / peak` is ~1e-30 at 1e30 and
        // ~5.8e-39 at FLT_MAX/2) that the shipped binary flushes to zero. A
        // regression that only holds because the harness keeps denormals is
        // not a regression on the product, so the harness is made to match.
        const juce::ScopedNoDenormals noDenormals;
        const double sr2 = 48000.0;
        const int block2 = 512;
        // The magnitude ceiling is set by the LIMITER, not by this stage:
        // above ~1e30 the gain it needs is subnormal and FTZ flushes it, so
        // the hot channel goes quiet for a reason that has nothing to do with
        // the colour polynomial and nothing a bound in this stage could fix.
        // The sweep therefore stops where the claim is still true — the
        // colour path's own overflow starts four orders BELOW the bottom of
        // this range, so the mutant is caught with room to spare.
        for (const float mix : { 0.0f, 0.5f, 1.0f })
        for (const float hot : { 1.0e8f, 1.0e12f, 1.0e16f, 1.0e20f })
        {
            anabasis::AnabasisEngine engine;
            engine.prepare (sr2, block2, 2);
            anabasis::EngineParameters p;
            p.colourModel   = 3;          // Transistor — the fifth-power model
            p.colourDepth   = 1.0f;
            p.clipMix       = mix;
            p.clipDriveDb   = 0.0f;       // the clipper exact-skips: nothing bounds the residue
            p.compStereoLink = 0.0f;      // per channel, so the quiet side is not dragged along
            p.stereoLink     = 0.0f;

            juce::AudioBuffer<float> buf (2, block2);
            double sumSq[2] = { 0.0, 0.0 };
            for (int b = 0; b < 80; ++b)
            {
                for (int n = 0; n < block2; ++n)
                {
                    const float ph = 2.0f * juce::MathConstants<float>::pi
                                   * (float) (b * block2 + n) / (float) sr2;
                    buf.setSample (0, n, hot * std::sin (ph * 220.0f));   // hostile, SUSTAINED
                    buf.setSample (1, n, 0.30f * std::sin (ph * 330.0f)); // ordinary programme
                }
                engine.process (buf, p);
                if (b >= 60)
                    for (int n = 0; n < block2; ++n)
                        for (int ch = 0; ch < 2; ++ch)
                            sumSq[ch] += (double) buf.getSample (ch, n) * buf.getSample (ch, n);
            }
            const double den = 20.0 * (double) block2;
            const double rmsHot = std::sqrt (sumSq[0] / den);
            const double rmsOk  = std::sqrt (sumSq[1] / den);
            juce::String msg;
            msg << "extremeLevel: sustained " << hot << " on ONE channel at clipMix " << mix
                << " does not silence it (hot=" << (float) rmsHot << " other=" << (float) rmsOk << ")";
            check (rmsHot > 1.0e-4, msg.toRawUTF8());
            msg.clear();
            msg << "extremeLevel: …and the untouched channel still plays at clipMix " << mix;
            check (rmsOk > 1.0e-4, msg.toRawUTF8());
        }
    }
}

// ---------------------------------------------------------------------------
// inv 9, the third kind of stage: the ones that emit no audio, so NO boundary
// can see them fail. Both of these poison themselves from a FINITE input and
// then fail SILENTLY — the readings are wrong, every gate that compares them
// is false, and nothing in the engine notices. They are repaired once per
// block rather than on the recovery flag, and this test is what says so.
// ---------------------------------------------------------------------------
// inv 9 meets §5.4 Learn: a pass whose measurement overflowed must not become
// the saved reference. The damage would be permanent AND persistent — every
// trim target is derived from `refTiltDb`, `jlimit` returns NaN for a NaN
// input, the hysteresis `|tgt − state| > deadband` is false for NaN, so the
// vector never moves again; and `hasLearned()` is true, so the next save writes
// the value into the session's ADAPTIVE child.
static void testALearnPassThatOverflowedIsNotCommitted()
{
    const double sr = 48000.0;
    const int block = 512;
    anabasis::AnabasisEngine engine;
    engine.prepare (sr, block, 2);
    anabasis::EngineParameters p;
    juce::AudioBuffer<float> buf (2, block);

    auto feed = [&] (int blocks, int t0)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int n = 0; n < block; ++n)
            {
                const float v = 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                 * 220.0f * (float) (t0 + b * block + n) / (float) sr);
                buf.setSample (0, n, v); buf.setSample (1, n, v);
            }
            engine.process (buf, p);
        }
    };

    feed (20, 0);
    engine.requestLearnStart();
    feed (40, 20 * block);                          // a real pass, sums finite

    // One astronomical block WHILE the pass runs. Constant, not alternating:
    // the mean square must stay above the silence gate for the accumulation to
    // happen at all, and `ms = inf` passes it while `ms = NaN` would not. The
    // band energies both overflow, so `tiltDb` is inf/inf = NaN and the sum
    // takes it.
    constexpr float huge = 0.5f * std::numeric_limits<float>::max();
    for (int n = 0; n < block; ++n) { buf.setSample (0, n, huge); buf.setSample (1, n, huge); }
    engine.process (buf, p);

    engine.requestLearnStop();                      // commit, consumed next block top
    feed (1, 61 * block);

    check (std::isfinite (engine.adaptive().publishedRefTilt())
               && std::isfinite (engine.adaptive().publishedRefOnset()),
           "learnOverflow: the saved reference is a number");

    // …and adaptation is still alive: the trims must MOVE again, which they
    // cannot do at all once a reference is NaN.
    const float tiltTrimBefore = engine.adaptive().publishedTrimTilt();
    const float hpfTrimBefore  = engine.adaptive().publishedTrimHpf();
    feed (120, 62 * block);
    check (! juce::exactlyEqual (engine.adaptive().publishedTrimTilt(), tiltTrimBefore)
               || ! juce::exactlyEqual (engine.adaptive().publishedTrimHpf(), hpfTrimBefore),
           "learnOverflow: the trim vector still adapts after the ruined pass");

    // The other writer of the references: a RESTORE. Same rule, because a
    // session written by a build that did commit a NaN (or an edited file)
    // would otherwise re-poison a healthy engine on load.
    engine.restoreLearnedTargets (std::numeric_limits<float>::quiet_NaN(),
                                  std::numeric_limits<float>::quiet_NaN());
    feed (1, 190 * block);                          // block top consumes it
    check (! engine.adaptiveForWrapper().hasLearned(),
           "learnOverflow: a non-finite restore reads as never-learned");
    check (std::isfinite (engine.adaptive().publishedRefTilt())
               && std::isfinite (engine.adaptive().publishedRefOnset()),
           "learnOverflow: and leaves the references usable");
}

static void testExtremeLevelDoesNotBreakTheMetersOrAdaptation()
{
    const double sr = 48000.0;
    const int block = 512;
    juce::AudioBuffer<float> buf (2, block);
    auto tone = [&] (int b)
    {
        for (int n = 0; n < block; ++n)
        {
            const float ph = 2.0f * 3.14159265f * 220.0f * (float) (b * block + n) / (float) sr;
            const float v = 0.2f * std::sin (ph);
            buf.setSample (0, n, v);
            buf.setSample (1, n, v);
        }
    };

    // (a) §5.4 features. The stimulus is Nyquist AT FULL SCALE, not a constant:
    //     `bandLp += (x − bandLp)·a` overflows on the SIGN FLIP (the difference
    //     is 2·FLT_MAX), which a constant huge block never does — it only makes
    //     the band-energy SQUARE overflow, and that recovers on its own. Once
    //     `bandLp` is NaN every later block is too, `tiltDb`/`crestDb` publish
    //     NaN, and the trim hysteresis `|tgt − state| > deadband` is false for
    //     NaN, so the vector freezes at a plausible-looking value for the rest
    //     of the session.
    {
        anabasis::AnabasisEngine engine;
        engine.prepare (sr, block, 2);
        anabasis::EngineParameters p;

        for (int b = 0; b < 60; ++b) { tone (b); engine.process (buf, p); }
        const float tiltBefore = engine.adaptive().publishedTiltDb();

        constexpr float huge = std::numeric_limits<float>::max();
        for (int n = 0; n < block; ++n)
        {
            const float v = (n % 2 == 0) ? huge : -huge;
            buf.setSample (0, n, v);
            buf.setSample (1, n, v);
        }
        engine.process (buf, p);

        for (int b = 61; b < 121; ++b) { tone (b); engine.process (buf, p); }
        const float tiltAfter = engine.adaptive().publishedTiltDb();

        check (std::isfinite (tiltBefore) && tiltBefore < -5.0f,
               "meters/adaptation: (test premise) the tilt feature reads the tone before the event");
        check (std::isfinite (tiltAfter) && std::abs (tiltAfter - tiltBefore) < 2.0f,
               "meters/adaptation: the tilt feature measures again after an extreme sample");
    }

    // (b) §2.9 output meter, reached through the BYPASS leg — `render` is then
    //     the raw delay-aligned dry signal, which the engine keeps finite but
    //     does not bound, and the K-weighting shelf overflows on it (|b1| ≈ 2.7).
    //     Without the repair the meter reads silence for ever afterwards, which
    //     is worse than reading NaN: it looks like a legitimate measurement.
    //
    //     The host block is 8192 here, and that is calibration rather than a
    //     detail: the 100 ms sub-block boundary (4800 samples at 48 kHz) has to
    //     fall INSIDE the poisoned block for the ruined accumulator to reach
    //     the sliding-window ring at all. At 512 samples the per-block repair
    //     clears the accumulator first about nine times in ten, and the ring
    //     guard the meter carries looks like dead code.
    {
        const int bigBlock = 8192;
        juce::AudioBuffer<float> big (2, bigBlock);
        anabasis::AnabasisEngine engine;
        engine.prepare (sr, bigBlock, 2);
        anabasis::EngineParameters p;
        p.bypass = true;

        auto bigTone = [&] (int b)
        {
            for (int n = 0; n < bigBlock; ++n)
            {
                const float ph = 2.0f * 3.14159265f * 220.0f
                                     * (float) (b * bigBlock + n) / (float) sr;
                const float v = 0.2f * std::sin (ph);
                big.setSample (0, n, v);
                big.setSample (1, n, v);
            }
        };

        for (int b = 0; b < 40; ++b) { bigTone (b); engine.process (big, p); }
        const float lufsBefore = engine.outputLoudness().shortTermLufs();
        const float intBefore  = engine.outputLoudness().integratedLufs();

        constexpr float huge = std::numeric_limits<float>::max();
        for (int n = 0; n < bigBlock; ++n)
        {
            big.setSample (0, n, huge);
            big.setSample (1, n, huge);
        }
        engine.process (big, p);

        // Measured TWICE, and the early one is the point: the K-weighting
        // states are repaired at the next block top, but a sub-block mean
        // already folded into the ring would sit there for up to kSubRing
        // sub-blocks (~3.2 s), reading NaN the whole time and freezing the
        // §2.7 compensation with it. Three blocks ≈ 0.5 s is well inside that
        // window and well outside the one-block repair.
        for (int b = 41; b < 44; ++b) { bigTone (b); engine.process (big, p); }
        const float lufsSoon = engine.outputLoudness().shortTermLufs();

        for (int b = 44; b < 90; ++b) { bigTone (b); engine.process (big, p); }
        const float lufsAfter = engine.outputLoudness().shortTermLufs();

        check (std::isfinite (lufsBefore) && lufsBefore > -40.0f,
               "meters/adaptation: (test premise) the output meter reads the tone before the event");
        check (std::isfinite (lufsSoon),
               "meters/adaptation: the output meter reads a number within half a second of the event");
        check (std::isfinite (lufsAfter) && std::abs (lufsAfter - lufsBefore) < 1.0f,
               "meters/adaptation: the output meter measures again after an extreme sample");
        // The gated HISTOGRAM ages nothing out, so this pins the property that
        // keeps it clean — the absolute gate `lufs >= -70.0` is false for the
        // NaN this failure produces, so the block is never counted. That half
        // needs no guard; the sliding window's does, and the half-second
        // assertion above is what measures it.
        const float intAfter = engine.outputLoudness().integratedLufs();
        check (std::isfinite (intAfter) && std::abs (intAfter - intBefore) < 1.0f,
               "meters/adaptation: the integrated reading is not poisoned for the session");
    }
}

// ---------------------------------------------------------------------------
// inv 9's self-heal must degrade GRACEFULLY (inv 8). Discarding the sliding
// window costs pre-emption for W samples — documented and accepted. Snapping
// the ENVELOPE back to unity is a separate effect and not acceptable: the
// delay line still holds the loud material the old envelope was holding down,
// so the recovery hands it to the clamp at full level and flat-tops it.
// Measured as the count of clamped samples in the block AFTER the self-heal
// fires; the whole point is that it stays near the steady-state count instead
// of jumping to "most of the block".
// The stimulus matters more than the assertion here. Material sitting FAR over
// the ceiling rides at the ceiling whether it is limited or clipped, so the two
// behaviours are indistinguishable in the output — an earlier version of this
// test measured exactly that and passed against the bug. What separates them is
// a signal that is QUIET under a held-down envelope: a burst drives the gain to
// ~0.22 and a slow release holds it there, so the quiet tone that follows plays
// attenuated. Carrying the envelope keeps it attenuated; snapping to unity
// steps the level back up ~4.5x in one sample — and STAYS there, because the
// quiet tone never asks for gain reduction again.
static void testSelfHealDoesNotSnapTheEnvelope()
{
    anabasis::AnabasisEngine engine;
    const double sr = 48000.0;
    const int block = 512;
    engine.prepare (sr, block, 2);

    anabasis::EngineParameters p;
    p.limReleaseMs   = 1000.0f;                // slow: the envelope is still held later
    p.limAutoRelease = false;                  // auto's fast pole would defeat that premise
    juce::AudioBuffer<float> buf (2, block);

    const int burstBlock = 3, poisonBlock = 8;
    auto rms = [&]
    {
        double sum = 0.0;
        for (int n = 0; n < block; ++n)
        {
            const double s = buf.getSample (1, n);
            sum += s * s;
        }
        return std::sqrt (sum / (double) block);
    };

    double before = 0.0, after = 0.0;
    for (int b = 0; b < 12; ++b)
    {
        for (int n = 0; n < block; ++n)
        {
            const float ph = 2.0f * 3.14159265f * 200.0f * (float) (b * block + n) / (float) sr;
            float v = 0.4f * std::sin (ph);                      // below the ceiling on its own
            if (b == burstBlock && n >= 100 && n < 130)
                v = 4.0f;                                        // over-ceiling burst
            // Poison ONE sample of the LEFT channel; the right channel stays
            // clean, so the measurement sees the recovery, not the hole.
            buf.setSample (0, n, (b == poisonBlock && n == 0)
                                     ? std::numeric_limits<float>::quiet_NaN() : v);
            buf.setSample (1, n, v);
        }
        engine.process (buf, p);
        if (b == poisonBlock - 1) before = rms();   // held down by the burst
        if (b == poisonBlock + 1) after  = rms();   // the block after the self-heal
    }

    // 0.4 amplitude sine = 0.283 RMS unattenuated; held at ~0.22 gain it is
    // ~0.063. The premise check fails loudly if the burst never engaged.
    check (before < 0.12,
           "selfHeal: (test premise) the envelope is still holding the tone down");
    check (after < before * 1.5,
           "selfHeal: recovery carries the envelope instead of snapping it to unity");
}

// ---------------------------------------------------------------------------
// ADR-0014's boundary contract, taken literally: a RESTORED frozen-trim vector
// is the one externally authored thing that reaches the adaptive state, and the
// ADR says it is "clamped at the boundary … holds against hostile state". A
// clamp built from `juce::jlimit` does not hold against a NaN — both of its
// comparisons are false, so the value passes through untouched — and past the
// boundary it is published (so the wrapper can serialise it back out) and fed to
// `std::pow` in the release mapping. `sanitiseState()` catches it a block later;
// that is a bounded recovery, not the contract the ADR states.
static void testHostileFrozenTrimsCannotEnterTheAdaptiveState()
{
    anabasis::AdaptiveEngine a;
    a.prepare (48000.0, 512);

    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    constexpr float inf = std::numeric_limits<float>::infinity();

    // Every field poisoned differently, plus one ordinary out-of-range value so
    // the check cannot pass by rejecting the whole vector.
    anabasis::AdaptiveEngine::Trims hostile;
    hostile.releaseOctaves = nan;
    hostile.stereoLink     = -inf;
    hostile.scHpfHz        = inf;
    hostile.dynTiltDb      = 7.5f;      // legitimately out of range: must CLAMP, not reset
    a.injectTrims (hostile);

    check (std::isfinite (a.retainedTrimRelease()) && std::isfinite (a.retainedTrimLink())
            && std::isfinite (a.retainedTrimHpf()) && std::isfinite (a.retainedTrimTilt()),
           "hostileTrims: a non-finite restored trim never reaches the published vector");
    check (std::abs (a.retainedTrimTilt() - 0.5f) < 1.0e-6f,
           "hostileTrims: …and a merely out-of-range field is still clamped, not discarded");

    // Per field, not whole-struct: three good values must survive one bad one,
    // which is the opposite of `sanitiseState`'s choice and deliberately so —
    // there the four members share one poisoned pipeline, here they are four
    // independent document properties.
    anabasis::AdaptiveEngine::Trims oneBad;
    oneBad.releaseOctaves = 0.5f;
    oneBad.stereoLink     = 0.1f;
    oneBad.scHpfHz        = nan;
    oneBad.dynTiltDb      = 0.25f;
    a.injectTrims (oneBad);
    check (std::abs (a.retainedTrimRelease() - 0.5f) < 1.0e-6f
            && std::abs (a.retainedTrimLink() - 0.1f) < 1.0e-6f
            && std::abs (a.retainedTrimTilt() - 0.25f) < 1.0e-6f
            && juce::exactlyEqual (a.retainedTrimHpf(), 0.0f),
           "hostileTrims: one unreadable field takes its reset seed and the rest are kept");
}

// ---------------------------------------------------------------------------
// inv 7 second half: bypass is a delay-aligned bit-exact copy once the §2.8
// crossfade has settled.
static void testBypassNull()
{
    anabasis::AnabasisEngine engine;
    const double sr = 48000.0;
    const int block = 512;
    engine.prepare (sr, block, 2);
    const int delay = engine.groupDelaySamples();

    anabasis::EngineParameters p;
    p.bypass    = true;
    p.limGainDb = 18.0f;            // wet path would be loud — bypass must not be

    std::vector<float> in, out;
    juce::AudioBuffer<float> buf (2, block);
    uint32_t rng = 0xCAFEBABEu;
    for (int b = 0; b < 60; ++b)    // ~0.64 s: far past the 10 ms fade
    {
        for (int n = 0; n < block; ++n)
        {
            rng = rng * 1664525u + 1013904223u;
            const float v = ((float) (rng >> 8) / 8388608.0f - 1.0f) * 0.5f;
            buf.setSample (0, n, v);
            buf.setSample (1, n, v);
            in.push_back (v);
        }
        engine.process (buf, p);
        for (int n = 0; n < block; ++n)
            out.push_back (buf.getSample (0, n));
    }

    bool exact = true;
    for (size_t n = out.size() / 2; n < out.size(); ++n)
        if (! juce::exactlyEqual (out[n], in[n - (size_t) delay])) { exact = false; break; }
    check (exact, "bypass: settled bypass is a bit-exact delay-aligned copy");
}

// ---------------------------------------------------------------------------
// The LookaheadLimiter CONTRACT, pinned as a unit test: fed[t] is the sample
// playing W steps from now; the returned gain applies to the sample playing
// NOW; the window covers W+1 fed values, so the envelope attacks W early,
// holds through the instant the peak plays, and releases only afterwards.
// (The first revision attacked 10 ms early regardless of W and expired the
// playing sample one step early — both proven by simulation to pass peaks to
// the clamp under-attenuated.)
static void testLimiterWindowCoverage()
{
    anabasis::LookaheadLimiter lim;
    lim.prepare (48000.0, 480);
    lim.setRelease (1000.0f);                // slow release: early recovery is visible
    const int w = 96;                        // ceil(2 ms) at 48 kHz
    const float ceilingLin = 0.5f;

    const int spikeAt = 500;
    float envBefore = 1.0f, envAtSpikeEnter = 1.0f, envAtPlay = 1.0f, envAfter = 1.0f;
    for (int t = 0; t < 1200; ++t)
    {
        // Class defaults keep the P1 semantics exact: HPF off (floor), true
        // peak off, preserve 0 (instant attack), manual release, link 1.
        const float fed[1] = { (t == spikeAt) ? 1.0f : 0.0f };
        float g[1];
        lim.processSample (fed, 1, w, ceilingLin, g);
        const float env = g[0];
        if (t == spikeAt - 1)     envBefore      = env;
        if (t == spikeAt)         envAtSpikeEnter = env;   // the attack instant: the spike is now in the window
        if (t == spikeAt + w)     envAtPlay      = env;    // spike is the PLAYING sample now
        if (t == spikeAt + w + 1) envAfter       = env;    // spike has played: release may begin
    }
    check (juce::exactlyEqual (envBefore, 1.0f),      "limiter: no attack before the spike is visible");
    check (juce::exactlyEqual (envAtSpikeEnter, 0.5f), "limiter: instant attack the moment the spike enters the window");
    check (juce::exactlyEqual (envAtPlay, 0.5f),       "limiter: gain still held when the spike PLAYS (off-by-one guard)");
    check (envAfter > 0.5f,                            "limiter: release begins only after the spike has played");
}

// ---------------------------------------------------------------------------
// The engine-level alignment: with engaged lookahead W, the output duck must
// begin exactly W samples before the over-ceiling sample plays — not at the
// full 10 ms allowance. (The first revision fed the gain computer from the
// write end of the ring, so the duck began 480 samples early and had partly
// RELEASED by the time the peak played.)
static void testLimiterAlignment()
{
    anabasis::AnabasisEngine engine;
    const double sr = 48000.0;
    const int block = 512;
    engine.prepare (sr, block, 2);
    const int delay = engine.groupDelaySamples();        // 480

    anabasis::EngineParameters p;
    p.lookaheadMs  = 2.0f;                               // W = 96
    p.limReleaseMs = 1000.0f;                            // slow: any early release is visible
    // This test pins the WEDGE contract sample-exactly, so the three controls
    // that legitimately blur timing/depth are pinned off: the true-peak
    // estimate is ~5.5 samples late and can over-read an isolated spike,
    // transient preserve deliberately lags the attack, and auto release
    // overrides the slow manual release above. Each has its own test.
    p.truePeakMode      = false;
    p.transientPreserve = 0.0f;
    p.limAutoRelease    = false;
    // The ceiling joins them: the two checks on the peak sample are TWO-SIDED
    // (`<= ceilingLin` and `>= ceilingLin * 0.995`), so they assert that the
    // envelope arrives at the ceiling and has not released off it — a claim
    // calibrated against a known ceiling and a `steady` level chosen to sit
    // under it. Inherited, a default move retunes the window: at −0.1 the
    // 0.25 steady level sits further below the ceiling than the stimulus was
    // written for. −1 dB is that calibration.
    p.ceilingDbTp       = -1.0f;
    const int w = 96;

    const float steady  = 0.25f;
    const int   spikeIn = 2048;                          // input index of the over-ceiling sample
    const int   spikeOut = spikeIn + delay;              // where it plays

    std::vector<float> out;
    juce::AudioBuffer<float> buf (2, block);
    for (int b = 0; b * block < 6144; ++b)
    {
        for (int n = 0; n < block; ++n)
        {
            const float v = (b * block + n == spikeIn) ? 1.6f : steady;
            buf.setSample (0, n, v);
            buf.setSample (1, n, v);
        }
        engine.process (buf, p);
        for (int n = 0; n < block; ++n)
            out.push_back (buf.getSample (0, n));
    }

    check (juce::exactlyEqual (out[(size_t) (spikeOut - w - 40)], steady),
           "alignment: no duck earlier than the engaged lookahead before the peak");
    check (out[(size_t) (spikeOut - 2)] < steady,
           "alignment: duck engaged within the lookahead window");
    const float ceilingLin = std::pow (10.0f, p.ceilingDbTp / 20.0f);
    check (out[(size_t) spikeOut] <= ceilingLin * 1.0001f,
           "alignment: the peak itself is limited to the ceiling");
    check (out[(size_t) spikeOut] >= ceilingLin * 0.995f,
           "alignment: the envelope has not released early when the peak plays");
}


// ---------------------------------------------------------------------------
// REALTIME_AUDIO_POLICY, ENFORCED RATHER THAN INSPECTED (0.2.0).
//
// The policy is Priority 1 and unconditional -- no allocation, no lock, no
// blocking call on the audio thread -- and until this test the only enforcement
// was human review plus `REALTIME_SAFETY_AUDIT.md`, a hand-written per-module
// audit. That document asked for exactly this instrument, in as many words:
// "a malloc-interposition run (e.g. an RT-safety checker under the DSP suite)
// would upgrade the allocation claims from Verified-by-inspection to
// machine-verified; tracked for P6's gate". P6 closed without it.
//
// WHY A COUNTER AND NOT A SANITIZER, when RealtimeSanitizer exists and is the
// stronger tool. RTSan is Clang-only and Linux/macOS-only; the shipped Windows
// binary is built by MSVC, where it never runs. `operator new` replacement is
// standard C++ and works on every conforming implementation, so this tier
// reaches the platform the sanitizer cannot. The two are complementary and both
// ship: the `realtime` job runs RTSan over this same suite, and this guard
// stands ITSELF down there (see AllocationGuard.h) so it cannot shadow the
// sanitizer's own interceptors.
//
// IT PROVES IT CAN COUNT BEFORE IT REPORTS ZERO (TESTING_POLICY rule 5).
// "No allocations were observed" and "nothing was observing" print identically,
// and the halves genuinely differ by configuration -- the malloc interposer
// needs glibc and is compiled out under ASan, and the whole guard is compiled
// out under RTSan and valgrind. `selfCheck()` performs one known allocation of
// each kind and reports which halves moved; a half that is not live is
// DISCLOSED and skipped, never silently passed.
//
// WHAT IS ARMED. `prepare()` is REQUIRED to allocate and is deliberately
// outside every armed scope; only `AnabasisEngine::process` is measured, which
// is the whole §2 chain -- EQ, comp, clip/colour, limiter, oversampling,
// dither, the §2.8 transition layer, the §2.7/§2.9 meter taps and the §5.4
// adaptive engine. The matrix sweeps both channel counts, all five oversample
// factors, both phase modes and four parameter sets including bypass and delta,
// because a branch the suite does not execute is a branch no runtime tool sees.
//
// THE MID-STREAM REWIRE IS ITS OWN CASE, and it is the one most likely to
// allocate: changing the oversample factor between blocks re-plans JUCE's
// oversampler, and ADR-0023's §2.8 duck exists precisely so that rewire lands
// at a silent bottom. That path is driven here WITHOUT a re-prepare, which is
// the shape a user gets when they turn oversampling up while audio is running.
static void testTheAudioPathAllocatesNothing()
{
    using namespace anabasis::testing;

    const auto live = selfCheck();
    if (! live.newLive && ! live.mallocLive)
    {
        // Disclosed, not silent: this is the expected state under RTSan and
        // valgrind, where the guard compiles out and a stronger tool is
        // watching instead.
        std::printf ("note: allocation guard is compiled out in this configuration "
                     "(new=%d malloc=%d aligned=%d mallocCompiledIn=%d) -- "
                     "skipping the audio-path allocation assertions\n",
                     (int) live.newLive, (int) live.mallocLive,
                     (int) live.alignedNewLive, (int) live.mallocCompiledIn);
        return;
    }

    check (live.newLive, "alloc guard: the operator new counter is live");
    check (live.alignedNewLive, "alloc guard: the over-aligned operator new counter is live");
    // COMPILED-IN-BUT-DEAD IS A DEFECT; NOT-COMPILED-IN IS A DOCUMENTED STATE.
    // They fail for opposite reasons, so they are asserted apart: the malloc
    // half does not exist on MSVC, macOS or under ASan, and where it DOES exist
    // and does not fire, the interposition has stopped working.
    if (live.mallocCompiledIn)
        check (live.mallocLive, "alloc guard: the malloc interposer is live where it is compiled in");

    const double sr = 48000.0;
    const int    block = 256;

    // WHAT `prepare()` ALLOCATES, measured and printed rather than asserted.
    // It is not a gate -- prepare is REQUIRED to allocate -- but it is the
    // evidence that the two counters are two ROUTES rather than one counted
    // twice, and the numbers AllocationGuard.h quotes are reproducible from
    // this line. JUCE's AudioBuffer/HeapBlock take the raw-malloc path, so a
    // guard with only the `operator new` half would miss most of it.
    {
        anabasis::AnabasisEngine probe;
        resetCounts();
        {
            Armed arm;
            probe.prepare (sr, block, 2);
        }
        std::printf ("note: one AnabasisEngine::prepare(48k, 256, 2) allocates "
                     "new=%ld malloc=%ld\n", newCount.load(), mallocCount.load());
    }

    struct Variant { const char* name; void (*apply) (anabasis::EngineParameters&); };
    const Variant variants[] = {
        { "defaults", [] (anabasis::EngineParameters&) {} },
        { "pushed",   [] (anabasis::EngineParameters& p) {
              p.limGainDb = 9.0f; p.clipDriveDb = 6.0f; p.colourDepth = 0.8f;
              p.colourModel = 2; p.compRatio = 4.0f; p.compThresholdDb = -12.0f;
              p.eqTiltDb = 3.0f; p.eqPosition = 1; p.eqBell1GainDb = -4.0f;
              p.truePeakMode = true; p.ditherMode = 1; p.ditherShaping = true;
              p.limStyle = 2; p.stereoLink = 0.25f; p.transientPreserve = 0.9f; } },
        { "bypass",   [] (anabasis::EngineParameters& p) { p.bypass = true; } },
        { "delta",    [] (anabasis::EngineParameters& p) {
              p.deltaMonitor = true; p.loudnessComp = true; p.freeze = true; } },
    };
    const anabasis::OversampleFactor factors[] = {
        anabasis::OversampleFactor::off,  anabasis::OversampleFactor::x2,
        anabasis::OversampleFactor::x4,   anabasis::OversampleFactor::x8,
        anabasis::OversampleFactor::x16 };
    const anabasis::OsPhaseMode phases[] = {
        anabasis::OsPhaseMode::minimum, anabasis::OsPhaseMode::linear };

    long worstNew = 0, worstMalloc = 0, armedCalls = 0;
    int  configs = 0;
    const char* firstOffender = nullptr;

    for (int channels = 1; channels <= 2; ++channels)
        for (auto factor : factors)
            for (auto phase : phases)
                for (const auto& v : variants)
                {
                    anabasis::AnabasisEngine engine;
                    engine.prepare (sr, block, channels);      // allocation lives HERE, by design

                    anabasis::EngineParameters p;
                    v.apply (p);
                    p.oversample = factor;
                    p.osPhase    = phase;

                    juce::AudioBuffer<float> buf (channels, block);
                    resetCounts();
                    {
                        Armed arm;
                        for (int b = 0; b < 24; ++b)
                        {
                            for (int ch = 0; ch < channels; ++ch)
                            {
                                auto* d = buf.getWritePointer (ch);
                                for (int n = 0; n < block; ++n)
                                    d[n] = 0.7f * std::sin (0.031f * (float) (b * block + n)
                                                            + 0.4f * (float) ch);
                            }
                            engine.process (buf, p);
                            ++armedCalls;
                        }
                    }
                    const long n = newCount.load(), m = mallocCount.load();
                    if ((n > 0 || m > 0) && firstOffender == nullptr)
                        firstOffender = v.name;
                    worstNew    = std::max (worstNew, n);
                    worstMalloc = std::max (worstMalloc, m);
                    ++configs;
                }

    check (worstNew == 0 && worstMalloc == 0,
           "realtime: AnabasisEngine::process allocates nothing across the whole matrix");
    if (worstNew > 0 || worstMalloc > 0)
        std::printf ("       worst config '%s': new=%ld malloc=%ld\n",
                     firstOffender != nullptr ? firstOffender : "?", worstNew, worstMalloc);

    // --- the mid-stream oversample rewire, with no re-prepare ---------------
    {
        anabasis::AnabasisEngine engine;
        engine.prepare (sr, block, 2);
        anabasis::EngineParameters p;
        juce::AudioBuffer<float> buf (2, block);
        for (int ch = 0; ch < 2; ++ch)
            juce::FloatVectorOperations::fill (buf.getWritePointer (ch), 0.5f, block);

        // Prime every factor ONCE before arming: the first visit to a factor is
        // where a lazily-planned oversampler would allocate, and that is a
        // question about the PREPARE contract rather than about steady-state
        // audio. Priming here keeps this case about the rewire itself; the
        // unprimed first visit is covered by the matrix above, which prepares
        // per configuration.
        for (auto factor : factors)
        {
            p.oversample = factor;
            for (int b = 0; b < 8; ++b) engine.process (buf, p);
        }

        resetCounts();
        {
            Armed arm;
            for (int pass = 0; pass < 3; ++pass)
                for (auto factor : factors)
                {
                    p.oversample = factor;
                    engine.requestForcedDuck();
                    for (int b = 0; b < 8; ++b) { engine.process (buf, p); ++armedCalls; }
                }
        }
        check (newCount.load() == 0 && mallocCount.load() == 0,
               "realtime: a mid-stream oversample rewire allocates nothing on the audio thread");
        if (newCount.load() > 0 || mallocCount.load() > 0)
            std::printf ("       rewire: new=%ld malloc=%ld\n",
                         newCount.load(), mallocCount.load());
    }

    std::printf ("note: allocation guard armed over %ld process() calls across %d configurations "
                 "(new counter %s, malloc counter %s)\n",
                 armedCalls, configs,
                 live.newLive ? "live" : "DEAD",
                 live.mallocCompiledIn ? (live.mallocLive ? "live" : "DEAD") : "not compiled in");
}

int main()
{
    // Unbuffered stdout: CI pipes are fully buffered, so a crash mid-suite
    // used to eat every line printed before it — a failing run showed exit 1
    // and nothing else. Costs nothing measurable at this output volume.
    setvbuf (stdout, nullptr, _IONBF, 0);

    testNullWithDefaults();
    testTheAudioPathAllocatesNothing();
    testLimiterWindowCoverage();
    testLimiterAlignment();
    testCeilingIsSmoothed();
    testControlsPrimedOnPrepare();
    testGainsPrimedOnPrepare();
    testAnInvalidSampleRateCannotSizeABuffer();
    testLookaheadIsSmoothed();
    testReportedLatencyMatchesImpulse();
    testOutputNeverExceedsCeiling();
    testEqFrequencyResponse();
    testEqGainIsSmoothed();
    testEqPositionsAreDistinct();
    testCompStaticCurve();
    testCompDetectorAndMix();
    testCompStereoLink();
    testCompAutoReleaseIsTwoStage();
    testCompSidechainHpf();
    testClipDriveZeroIsBitExact();
    testClipCurveAndCompensation();
    testClipAdaaReducesAliasing();
    testLimiterPushDoesNotDriveTheClipper();
    testOfflineFlipDoesNotDuckTheRender();
    testReturnFromOfflineIsDucked();
    testOfflineEntryClearsEqStateOnAPositionChange();
    testClipSatCannotLoseAChannel();
    testClipSatCannotHideANonFiniteFromTheBoundary();
    testColourModelsBalanceAndTone();
    testDynamicTame();
    testClipMixZeroIsDry();
    testTruePeakAccuracy();
    testLimiterTruePeakMode();
    testLimiterStereoLink();
    testLimiterAutoReleaseIsTwoStage();
    testLimiterStyles();
    testLimiterTransientPreserve();
    testLimiterDetectorIsUnfiltered();
    testOsLatencyMatrix();
    testBypassNullUnderOs();
    testOsTransparency();
    testCeilingUnderOs();
    testOsReducesAliasing();
    testDitherModes();
    testDuckWrapsDiscreteRewires();
    testDuckWrapsOsLatch();
    testDuckOnWrapperRequest();
    testDuckRequestDuringBottomExtendsBottom();
    testDuckRequestDuringOutIsHeld();
    testDeltaIsCoveredByTheDuck();
    testAdaptiveRestoreLastStagedWins();
    testStaleDetectorStateIsNotReentered();
    testLimiterControlSmoothing();
    testLufsCalibration();
    testLoudnessRangeAndTheUngatedReading();
    testTheCachedLoudnessReadingsAreNeverStale();
    testRmsMeterReadsTrueLevels();
    testLufsGating();
    testLufsWindows();
    testLoudnessCompensationDoesNotAlterRender();
    testDeltaMonitor();
    testAdaptationConvergesAndHolds();
    testResetCancelsAnInFlightLearnPass();
    testStopThenStartInOneBlockKeepsBoth();
    testFreezeLatchesTrims();
    testTrimBounds();
    testAutoReleaseFollowsTheTrimScale();
    testAStagedFrozenVectorAlwaysGetsABottom();
    testMeterResetIgnoresTheStraddlingSubBlock();
    testSpectrumRingsCarryTheTaps();
    testGrHistoryEntriesFollowThePreparedBlock();
    testTheHistorySurvivesASameConfigurationRePrepare();
    testTheHistoryTimelineIsTheRingsTimeline();
    testTheRingKeepsASecondOfEntriesAtTheSmallestPreparedBlock();
    testNoBadSamples();
    testExtremeLevelDoesNotSilencePermanently();
    testExtremeLevelDoesNotBreakTheMetersOrAdaptation();
    testALearnPassThatOverflowedIsNotCommitted();
    testHostileFrozenTrimsCannotEnterTheAdaptiveState();
    testSelfHealDoesNotSnapTheEnvelope();
    testBypassNull();

    std::printf ("%s: %d checks, %d failure(s)\n",
                 failures == 0 ? "PASS" : "FAIL", checks, failures);
    return failures == 0 ? 0 : 1;
}
