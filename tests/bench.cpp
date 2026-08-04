// ============================================================================
//  AnabasisBench — the DESIGN §9 benchmark procedure (the one Anamorph only
//  prescribes): OFF by default (ANABASIS_BUILD_BENCH), shipped-Release flags,
//  an SR × block × OS × mode matrix, ns/sample + worst single block, median
//  of ≥5 runs, machine recorded in the output. Results land in
//  docs/architecture/PERFORMANCE_BUDGET.md and docs/TEST_REPORT.md — never
//  quoted without the machine + method next to them (C2).
//
//  Harness per docs/procedures/TESTING.md: plain console app, no framework.
// ============================================================================

#include <AnabasisEngine.h>
#include <LoudnessMeter.h>
#include <TruePeak.h>
#include <juce_dsp/juce_dsp.h>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <thread>

#if JUCE_MAC || JUCE_IOS
 #include <sys/sysctl.h>
#elif JUCE_WINDOWS
 #include <windows.h>
#endif

namespace
{
    struct Cell
    {
        double sr;
        int block;
        anabasis::OversampleFactor os;
        bool working;                 // false = factory defaults (null path)
    };

    const char* osName (anabasis::OversampleFactor f)
    {
        switch ((int) f)
        {
            case 1: return "2x"; case 2: return "4x";
            case 3: return "8x"; case 4: return "16x";
            default: return "Off";
        }
    }

    // The MACHINE half of C2 ("a number without its machine and method is not a
    // measurement", PERFORMANCE_BUDGET.md). `ANABASIS_BUILD_BENCH` is a plain
    // CMake option with no platform guard, so this has to answer on every
    // platform a developer can configure it on — and the refresh rule
    // prescribes exactly the workflow that is NOT the CI Linux job: a local
    // re-measure before quoting a number. Reading `/proc/cpuinfo` and returning
    // "(unknown cpu)" everywhere else made C2 fail silently, producing a
    // results table that looks complete and identifies nothing.
    //
    // One lookup per platform, each the platform's own canonical source, and
    // NO silent fallback — see `main`.
    std::string cpuModel()
    {
       #if JUCE_LINUX || JUCE_BSD
        std::ifstream in ("/proc/cpuinfo");
        std::string line;
        while (std::getline (in, line))
            if (line.rfind ("model name", 0) == 0)
                return line.substr (line.find (':') + 2);
       #elif JUCE_MAC || JUCE_IOS
        // sysctl's own name for the part, which is what Apple's own tools quote.
        std::size_t len = 0;
        if (sysctlbyname ("machdep.cpu.brand_string", nullptr, &len, nullptr, 0) == 0 && len > 1)
        {
            std::string out (len - 1, '\0');
            if (sysctlbyname ("machdep.cpu.brand_string", out.data(), &len, nullptr, 0) == 0)
                return out;
        }
       #elif JUCE_WINDOWS
        // The registry string the CPUID brand feeds; available without COM or
        // a WMI query, which a bench harness has no business taking on.
        HKEY key {};
        if (RegOpenKeyExA (HKEY_LOCAL_MACHINE,
                           "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                           0, KEY_READ, &key) == ERROR_SUCCESS)
        {
            char buf[256] {};
            DWORD size = sizeof (buf), type = 0;
            const bool ok = RegQueryValueExA (key, "ProcessorNameString", nullptr, &type,
                                              reinterpret_cast<LPBYTE> (buf), &size) == ERROR_SUCCESS
                            && type == REG_SZ;
            RegCloseKey (key);
            if (ok)
                return std::string (buf);
        }
       #endif
        return {};   // EMPTY means "not identified" — main() refuses to run
    }

} // namespace

int main()
{
    // C2, enforced rather than hoped for: a table whose machine line is empty
    // is not a measurement, and the failure mode this replaces was that it
    // still PRINTED — a complete-looking result identifying nothing, which is
    // worse than no result because it can be pasted into the budget document.
    //
    // TWO identity sources, tried in order, and the ORDER is the fix: the
    // automatic lookup, then `ANABASIS_BENCH_CPU`. The override is the
    // documented, supported way to run on a platform whose lookup is not
    // written yet, so it must not read as a failure — the previous shape
    // printed the whole C2 refusal to stderr and *then* accepted the override,
    // which tells an operator following the documentation that something went
    // wrong when nothing did. The refusal now belongs to exactly one state:
    // neither source answered.
    std::string machine = cpuModel();
    if (machine.empty())
        if (const char* forced = std::getenv ("ANABASIS_BENCH_CPU"); forced != nullptr && *forced != 0)
            machine = forced;

    if (machine.empty())
    {
        std::fprintf (stderr,
                      "AnabasisBench: could not identify this CPU, so the run would violate "
                      "PERFORMANCE_BUDGET.md C2 (a number without its machine is not a "
                      "measurement).\nAdd a lookup for this platform in cpuModel(), or set "
                      "ANABASIS_BENCH_CPU to the model string and re-run.\n");
        return 2;
    }

    std::printf ("AnabasisBench — machine: %s, %d cores, %s %s\n",
                 machine.c_str(),
                 (int) std::thread::hardware_concurrency(),
#if defined (__clang__)
                 "clang", __clang_version__
#elif defined (__GNUC__)
                 "gcc", __VERSION__
#else
                 "cc", "?"
#endif
                 );
    std::printf ("method: 5 runs/cell, 1 s audio each, median ns/sample of the "
                 "per-block timed region; worst block = max single process() call\n\n");
    std::printf ("| SR | block | OS | mode | ns/sample (median) | worst block (us) | %% of realtime |\n");
    std::printf ("|---|---|---|---|---|---|---|\n");

    const anabasis::OversampleFactor factors[] = {
        anabasis::OversampleFactor::off,
        anabasis::OversampleFactor::x4,
        anabasis::OversampleFactor::x16,
    };

    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (int block : { 64, 512 })
            for (auto os : factors)
                for (bool working : { false, true })
                {
                    Cell c { sr, block, os, working };

                    std::vector<double> nsPerSample;
                    double worstUs = 0.0;
                    for (int r = 0; r < 5; ++r)
                    {
                        // Inline measurement, deliberately not factored into a
                        // helper: the accounting has to sum ONLY the timed
                        // process() region, and a helper that also owned the
                        // stimulus would invite counting the generator with it.
                        // (This comment used to cite a `run()` that no longer
                        // exists — it was factored away and the reference was
                        // not.)
                        anabasis::AnabasisEngine engine;
                        engine.prepare (c.sr, c.block, 2);
                        anabasis::EngineParameters p;
                        p.oversample = c.os;
                        if (c.working)
                        {
                            p.limGainDb = 9.0f;   p.compThresholdDb = -10.0f;
                            p.compRatio = 1.75f;  p.clipDriveDb = 2.6f;
                            p.colourDepth = 0.35f; p.eqTiltDb = 1.0f;
                            p.eqHighShelfGainDb = 1.5f; p.scHpfFreqHz = 60.0f;
                        }
                        juce::AudioBuffer<float> buf (2, c.block);
                        const int blocks = juce::jmax (1, (int) (1.0 * c.sr / c.block));
                        uint32_t rng = 0x12345u;
                        double sumNs = 0.0;
                        int64_t samples = 0;
                        for (int b = 0; b < blocks; ++b)
                        {
                            for (int n = 0; n < c.block; ++n)
                            {
                                rng = rng * 1664525u + 1013904223u;
                                const float noise = ((float) (rng >> 8) / 8388608.0f - 1.0f) * 0.05f;
                                const float tone  = 0.2f * std::sin (
                                    2.0f * juce::MathConstants<float>::pi * 220.0f
                                    * (float) (b * c.block + n) / (float) c.sr);
                                buf.setSample (0, n, tone + noise);
                                buf.setSample (1, n, tone - noise);
                            }
                            const auto b0 = std::chrono::steady_clock::now();
                            engine.process (buf, p);
                            const auto b1 = std::chrono::steady_clock::now();
                            const double ns = (double) std::chrono::duration_cast<
                                std::chrono::nanoseconds> (b1 - b0).count();
                            sumNs += ns;
                            samples += c.block;
                            worstUs = std::max (worstUs, ns / 1000.0);
                        }
                        nsPerSample.push_back (sumNs / (double) samples);
                    }
                    std::sort (nsPerSample.begin(), nsPerSample.end());
                    const double median = nsPerSample[nsPerSample.size() / 2];
                    // % of one core at this SR: (ns/sample) * SR / 1e9.
                    const double pct = median * c.sr / 1.0e7;
                    std::printf ("| %.0f | %d | %s | %s | %.1f | %.1f | %.2f%% |\n",
                                 c.sr, c.block, osName (c.os),
                                 c.working ? "working" : "defaults",
                                 median, worstUs, pct);
                    std::fflush (stdout);
                }
    // ---- Per-stage section: the DESIGN §9 allocation, measured -------------
    // Each module runs standalone at 48 kHz on the same stimulus the matrix
    // uses, in its `working` configuration. These are the numbers that let
    // PERFORMANCE_BUDGET.md's allocation table be measured instead of ⊕ —
    // module cost in isolation, not attribution inside the running chain
    // (cache and inlining differ there; the whole-engine matrix stays the
    // budget authority).
    std::printf ("\n| stage (48 kHz, standalone) | ns/sample (median) | %% of realtime |\n");
    std::printf ("|---|---|---|\n");

    auto stageRow = [] (const char* name, auto&& perSampleFn)
    {
        std::vector<double> med;
        for (int r = 0; r < 5; ++r)
        {
            uint32_t rng = 0x9876u;
            constexpr int n = 48000;
            const auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < n; ++i)
            {
                rng = rng * 1664525u + 1013904223u;
                const float noise = ((float) (rng >> 8) / 8388608.0f - 1.0f) * 0.05f;
                const float tone  = 0.2f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                     * 220.0f * (float) i / 48000.0f);
                float frame[2] = { tone + noise, tone - noise };
                perSampleFn (frame, i);
            }
            const auto t1 = std::chrono::steady_clock::now();
            med.push_back ((double) std::chrono::duration_cast<std::chrono::nanoseconds> (
                               t1 - t0).count() / (double) n);
        }
        std::sort (med.begin(), med.end());
        const double m = med[med.size() / 2];
        std::printf ("| %s | %.1f | %.2f%% |\n", name, m, m * 48000.0 / 1.0e7);
        std::fflush (stdout);
    };

    {
        anabasis::MasteringEQ eq;
        eq.prepare (48000.0);
        anabasis::EngineParameters p;
        p.eqTiltDb = 1.0f; p.eqLowShelfGainDb = 2.0f; p.eqHighShelfGainDb = 1.5f;
        p.eqBell1GainDb = 1.0f; p.eqBell2GainDb = -1.0f;
        eq.setTargets (p);
        stageRow ("EQ (all six sections engaged)", [&] (float* f, int)
        {
            eq.tick();
            f[0] = eq.processSample (0, f[0]);
            f[1] = eq.processSample (1, f[1]);
        });
    }
    {
        anabasis::MasteringComp comp;
        comp.prepare (48000.0);
        anabasis::EngineParameters p;
        p.compThresholdDb = -10.0f; p.compRatio = 1.75f; p.scHpfFreqHz = 60.0f;
        comp.setPerBlock (p);
        stageRow ("Compressor (RMS, HPF on)", [&] (float* f, int) { comp.processSample (f, 2); });
    }
    {
        anabasis::ClipSat clip;
        clip.prepare (48000.0);
        anabasis::EngineParameters p;
        p.clipDriveDb = 2.6f; p.colourDepth = 0.35f; p.dynTiltDb = 1.0f;
        clip.setPerBlock (p);
        stageRow ("Clipper/ADAA + colour + tame", [&] (float* f, int) { clip.processSample (f, 2); });
    }
    {
        anabasis::LookaheadLimiter lim;
        lim.prepare (48000.0, 480 + 3);
        lim.setRelease (100.0f);
        lim.setAutoRelease (true);
        lim.setStereoLink (1.0f);
        lim.setTransientPreserve (0.5f);
        lim.setDetectorHpf (60.0f);
        lim.setTruePeakMode (true);
        stageRow ("Limiter + TP detector (W=96)", [&] (float* f, int)
        {
            float gains[2] = { 1.0f, 1.0f };
            lim.processSample (f, 2, 96, 0.891f, gains);
            f[0] *= gains[0]; f[1] *= gains[1];
        });
    }
    {
        anabasis::LoudnessMeter meter;
        meter.prepare (48000.0);
        anabasis::TruePeakEstimator tp;
        tp.prepare();
        anabasis::AdaptiveEngine adaptive;
        adaptive.prepare (48000.0, 512);
        stageRow ("Metering + features (1x meter + TP + adaptive)", [&] (float* f, int)
        {
            meter.processFrame (f, 2);
            float tpo[2];
            tp.processFrame (f, 2, tpo);
            adaptive.pushFrame (f, 2);
        });
    }

    return 0;
}
