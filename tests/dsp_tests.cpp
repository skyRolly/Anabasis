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
int main()
{
    testNullWithDefaults();
    testReportedLatencyMatchesImpulse();
    testOutputNeverExceedsCeiling();
    testNoBadSamples();
    testBypassNull();

    std::printf ("%s: %d checks, %d failure(s)\n",
                 failures == 0 ? "PASS" : "FAIL", checks, failures);
    return failures == 0 ? 0 : 1;
}
