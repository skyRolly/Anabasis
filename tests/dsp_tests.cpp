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
    anabasis::AnabasisEngine engine;
    const double sr = 48000.0;
    engine.prepare (sr, 512, 2);
    anabasis::EngineParameters p;
    p.limGainDb = 12.0f;
    const float ceilingLin = std::pow (10.0f, p.ceilingDbTp / 20.0f);

    juce::AudioBuffer<float> buf (2, 512);
    float maxOut = 0.0f;
    for (int b = 0; b < 100; ++b)
    {
        for (int n = 0; n < 512; ++n)
        {
            const float v = 0.9f * std::sin (2.0 * juce::MathConstants<double>::pi
                                             * 97.0 * (b * 512 + n) / sr);
            buf.setSample (0, n, v);
            buf.setSample (1, n, v);
        }
        engine.process (buf, p);
        maxOut = juce::jmax (maxOut, buf.getMagnitude (0, 512));
    }
    check (maxOut <= ceilingLin * 1.0001f, "ceiling: sample peak never exceeds the clamp");
    check (maxOut > 0.5f * ceilingLin,     "ceiling: the limiter is actually engaged");
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
        const float fed = (t == spikeAt) ? 1.0f : 0.0f;
        const float env = lim.processSample (fed, w, ceilingLin);
        if (t == spikeAt - 1)     envBefore      = env;
        if (t == spikeAt)         envAtSpikeEnter = env;   // spike enters window: playing sample is t - 0? no: this IS the attack instant
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
    testNoBadSamples();
    testBypassNull();

    std::printf ("%s: %d checks, %d failure(s)\n",
                 failures == 0 ? "PASS" : "FAIL", checks, failures);
    return failures == 0 ? 0 : 1;
}
