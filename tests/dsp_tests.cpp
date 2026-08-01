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
#include <LoudnessMeter.h>
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
// delay-aligned copy of the input. THE STIMULUS LEVEL IS LOAD-BEARING, not a
// convenience: with the adaptive trims live, the null survives because every
// STAGE is inert, not because the trims are zero — the scHpf trim can switch
// both detector HPFs on even at factory defaults, and that stays inaudible
// only while the compressor is below its knee and the limiter below its
// ceiling. The knee bottom at defaults (threshold 0 dBFS, 6 dB knee) is
// −3 dBFS ≈ 0.708 linear; the ±0.25 peak here (−12 dBFS) sits well under it
// AND under the −1 dBTP ceiling. Raise this level past −3 dBFS and the test
// starts exercising the knee against trim-driven detector filtering — a
// different (and weaker) claim. The assert below pins the margin.
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

    // Self-enforcing form of the header's stimulus constraint: peak must stay
    // under the −3 dBFS knee bottom (see the comment above for why).
    float stimPeak = 0.0f;
    for (float v : inL) stimPeak = juce::jmax (stimPeak, std::abs (v));
    check (stimPeak < 0.708f, "null: the stimulus stays below the comp knee bottom (-3 dBFS)");

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
// below the −1 dBTP ceiling, so the limiter and the clamp stay out of it and
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
// history freezes whenever tpMode is false (the OS factor flips it), and the
// detector HPF's biquad keeps its delay line when the frequency drops to the
// range floor (the adaptive scHpf trim can drive that edge). Both are
// observable as gain reduction on silence, which is impossible from a clean
// detector.
static void testStaleDetectorStateIsNotReentered()
{
    const double sr = 48000.0;
    // Differential, so the envelope's own asymptotic approach to unity (it
    // converges, it does not arrive) cannot be mistaken for the effect: the
    // SAME sequence is run with the charging passage present and replaced by
    // silence. Only stale state can separate them.
    auto minGainOnSilence = [&] (bool useTruePeak, bool chargeIt)
    {
        anabasis::LookaheadLimiter lim;
        lim.prepare (sr, 480);
        lim.setAutoRelease (false);
        lim.setRelease (1.0f);              // so the envelope releases fast
        lim.setStereoLink (1.0f);
        lim.setTransientPreserve (0.0f);
        lim.setTruePeakMode (useTruePeak);
        lim.setDetectorHpf (useTruePeak ? 20.0f : 200.0f);

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
        // 2a) command the path OFF — and KEEP THE LOUD SIGNAL RUNNING while
        //     the HPF frequency GLIDES to its floor (~960 samples at 20 ms),
        //     so the biquad state is still charged when the off edge actually
        //     fires. Feeding silence here instead would let the glide drain
        //     the state naturally and the missing-clear mutant would survive
        //     on an empty delay line — the same put-the-property-where-the-
        //     assertion-looks rule as the crest alignment above. The TP mode
        //     flips instantly (a bool), so its history freezes at the crest
        //     regardless; the extra loud samples advance nothing there.
        lim.setTruePeakMode (false);
        lim.setDetectorHpf (20.0f);
        for (int n = 4200; n < 4200 + 1100; ++n)
        {
            const float v = chargeIt ? 4.0f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                        * 60.0f * (float) n / (float) sr)
                                     : 0.0f;
            const float fr[2] = { v, v };
            lim.processSample (fr, 2, 480, ceiling, g);
        }
        // 2b) silence with the path off: the envelope releases to unity, the
        //     frozen state (if any survived the off edge) keeps its charge.
        for (int n = 0; n < 8000; ++n)
        {
            const float fr[2] = { 0.0f, 0.0f };
            lim.processSample (fr, 2, 480, ceiling, g);
        }
        // 3) turn it back ON and keep feeding silence: a clean detector sees
        //    exactly zero, a stale one interpolates/rings the old passage.
        lim.setTruePeakMode (useTruePeak);
        lim.setDetectorHpf (useTruePeak ? 20.0f : 200.0f);
        float minGain = 1.0f;
        for (int n = 0; n < 600; ++n)
        {
            const float fr[2] = { 0.0f, 0.0f };
            lim.processSample (fr, 2, 480, ceiling, g);
            minGain = juce::jmin (minGain, g[0], g[1]);
        }
        return minGain;
    };

    const float tpClean = minGainOnSilence (true, false),  tpStale = minGainOnSilence (true, true);
    const float hpClean = minGainOnSilence (false, false), hpStale = minGainOnSilence (false, true);
    // Bound reasoning, measured rather than assumed: a never-charged run
    // returns EXACTLY 1.0, a charged one stalls at 0.99999857 — the one-pole
    // release's float floor (once (1−env)·a drops below half an ULP near
    // unity the addition rounds away, so the limiter never returns to bit-
    // exact unity after any reduction; −0.00001 dB, recorded, not chased).
    // The defect being tested is nothing like that size: a stale 4.0 tap or a
    // ringing biquad against a 0.5 ceiling pins the gain near 0.125. 0.999
    // sits between the two by four orders of magnitude.
    check (tpClean > 0.9999f && hpClean > 0.9999f,
           "staleDetector: silence into a never-charged detector is unity gain (the baseline)");
    check (tpStale > 0.999f,
           "staleDetector: re-enabling true-peak mode does not resurrect the frozen tap history");
    check (hpStale > 0.999f,
           "staleDetector: re-enabling the detector HPF does not ring on its old delay line");
}

// ---------------------------------------------------------------------------
// Invariant 8 at the limiter's own control boundary: stereo link, transient
// preserve and the detector HPF are LEVEL-affecting (link blends the detector
// level, preserve selects the attack alpha, the HPF moves the detector
// spectrum), so a per-block step in any of them must glide, not jump. The
// release/style/autoRelease setters stay unsmoothed by design — there the
// envelope IS the smoother.
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
        lim.setDetectorHpf (20.0f);
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
        lim.setDetectorHpf (20.0f);
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

    {   // DETECTOR HPF: 1 kHz content limited with the filter far above it
        // (3 kHz → detector ~0.1, no limiting), stepped to the floor (off →
        // detector 0.9 → gain ~0.55). Unsmoothed, the detector level and the
        // gain jump in one sample; smoothed, the frequency glides and the
        // gain follows the ramp.
        anabasis::LookaheadLimiter lim;
        lim.prepare (sr, 480);
        lim.setAutoRelease (false);
        lim.setRelease (200.0f);
        lim.setStereoLink (1.0f);
        lim.setTransientPreserve (0.0f);
        lim.setTruePeakMode (false);
        lim.setDetectorHpf (3000.0f);

        float g[2] = { 1.0f, 1.0f };
        auto feed = [&] (int from, int count, auto&& each)
        {
            for (int n = from; n < from + count; ++n)
            {
                const float v = 0.9f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                 * 1000.0f * (float) n / (float) sr);
                const float fr[2] = { v, v };
                lim.processSample (fr, 2, 480, 0.5f, g);
                each();
            }
        };
        feed (0, 3000, []{});
        const float before = g[0];

        lim.setDetectorHpf (20.0f);                    // the step under test
        float maxDelta = 0.0f, prev = g[0];
        feed (3000, 3000, [&]
        {
            maxDelta = juce::jmax (maxDelta, std::abs (g[0] - prev));
            prev = g[0];
        });
        check (before > 0.95f,  "limSmooth/hpf: with the HPF above the content there is no limiting");
        check (g[0] < 0.60f,    "limSmooth/hpf: the change ARRIVES (full detector level engages the limiter)");
        check (maxDelta < 0.05f,
               "limSmooth/hpf: an HPF step to the floor glides (unsmoothed = ~0.4 in one sample)");
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
    testLimiterPushDoesNotDriveTheClipper();
    testOfflineFlipDoesNotDuckTheRender();
    testReturnFromOfflineIsDucked();
    testOfflineEntryClearsEqStateOnAPositionChange();
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
    testLufsGating();
    testLufsWindows();
    testLoudnessCompensationDoesNotAlterRender();
    testDeltaMonitor();
    testAdaptationConvergesAndHolds();
    testFreezeLatchesTrims();
    testTrimBounds();
    testNoBadSamples();
    testSelfHealDoesNotSnapTheEnvelope();
    testBypassNull();

    std::printf ("%s: %d checks, %d failure(s)\n",
                 failures == 0 ? "PASS" : "FAIL", checks, failures);
    return failures == 0 ? 0 : 1;
}
