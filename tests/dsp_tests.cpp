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
#include <Latency.h>
#include <juce_dsp/juce_dsp.h>
#include <cstdio>
#include <cmath>

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
// delay-aligned copy of the input. Stimulus level respects §4.2's note: below
// the default ceiling (-1 dBTP) and the comp knee region.
static void testNullWithDefaults()
{
    anabasis::AnabasisEngine engine;
    const double sr = 48000.0;
    const int block = 512, blocks = 40;
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
            rng = rng * 1664525u + 1013904223u;                       // deterministic
            const float v = ((float) (rng >> 8) / 8388608.0f - 1.0f) * 0.25f;   // ±0.25
            buf.setSample (0, n, v);
            buf.setSample (1, n, -v);
            inL.push_back (v);
        }
        engine.process (buf, p);
        for (int n = 0; n < block; ++n)
            outL.push_back (buf.getSample (0, n));
    }

    bool exact = true;
    for (size_t n = (size_t) delay; n < outL.size(); ++n)
        if (! juce::exactlyEqual (outL[n], inL[n - (size_t) delay])) { exact = false; break; }
    check (exact, "null: output is a bit-exact copy delayed by the allowance");
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
    p.ceilingDbTp = -12.0f;                 // 0.251 linear, far from the 0.891 default
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
                                                 * 100.0f * (b * 512 + n) / (float) sr);
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
    {   // At exactly threshold with a 12 dB knee: GR = (invR−1)·(W/2)²/2W = −1.125 dB
        p.compKneeDb = 12.0f;
        const float outDb = compOutputRmsDb (p, 0.14142f, sr);   // RMS −20.00
        check (near (outDb, -20.0f - 1.125f, 0.4f),
               "comp: soft knee applies the quadratic reduction at the threshold");
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
// §2.4 colour: Clean is the null model at EVERY depth; depth 0 is exact with
// every model; balance swings the odd/even ratio; tone tilts the residue.
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
// Brief §3: the shared sidechain HPF also serves the limiter detector — a
// 30 Hz tone over the ceiling ducks hard at the floor (= no filtering) and
// far less at 300 Hz. The floor is an EXACT skip: the default detector is the
// tapped sample itself, byte-for-byte.
static void testLimiterDetectorHpf()
{
    const double sr = 48000.0;
    auto settled = [&] (float hpfHz)
    {
        anabasis::LookaheadLimiter lim;
        lim.prepare (sr, 480);
        lim.setRelease (400.0f);
        lim.setDetectorHpf (hpfHz);
        float g[2] = { 1.0f, 1.0f };
        for (int t = 0; t < 48000; ++t)
        {
            const float v = 1.0f * std::sin (2.0f * juce::MathConstants<float>::pi
                                             * 30.0f * (float) t / (float) sr);
            const float fed[2] = { v, v };
            lim.processSample (fed, 2, 96, 0.5f, g);
        }
        return g[0];
    };
    const float atFloor = settled (20.0f), at300 = settled (300.0f);
    check (atFloor < 0.6f, "limHpf: at the floor a loud 30 Hz tone ducks hard");
    check (at300 > atFloor + 0.2f, "limHpf: at 300 Hz the same tone ducks far less");
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


int main()
{
    testNullWithDefaults();
    testLimiterWindowCoverage();
    testLimiterAlignment();
    testCeilingIsSmoothed();
    testControlsPrimedOnPrepare();
    testGainsPrimedOnPrepare();
    testLookaheadIsSmoothed();
    testReportedLatencyMatchesImpulse();
    testOutputNeverExceedsCeiling();
    testEqFrequencyResponse();
    testEqGainIsSmoothed();
    testEqPositionsAreDistinct();
    testCompStaticCurve();
    testCompDetectorAndMix();
    testCompAutoReleaseIsTwoStage();
    testCompSidechainHpf();
    testClipDriveZeroIsBitExact();
    testClipCurveAndCompensation();
    testClipAdaaReducesAliasing();
    testColourModelsBalanceAndTone();
    testDynamicTame();
    testClipMixZeroIsDry();
    testTruePeakAccuracy();
    testLimiterTruePeakMode();
    testLimiterStereoLink();
    testLimiterAutoReleaseIsTwoStage();
    testLimiterStyles();
    testLimiterTransientPreserve();
    testLimiterDetectorHpf();
    testOsLatencyMatrix();
    testBypassNullUnderOs();
    testOsTransparency();
    testCeilingUnderOs();
    testOsReducesAliasing();
    testDitherModes();
    testNoBadSamples();
    testSelfHealDoesNotSnapTheEnvelope();
    testBypassNull();

    std::printf ("%s: %d checks, %d failure(s)\n",
                 failures == 0 ? "PASS" : "FAIL", checks, failures);
    return failures == 0 ? 0 : 1;
}
