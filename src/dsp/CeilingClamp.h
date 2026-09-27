#pragma once

#include "TruePeak.h"
#include <juce_core/juce_core.h>   // the ownership guard macro (CODE_STYLE §Structure)
#include <algorithm>
#include <cmath>
#include <vector>

// ============================================================================
//  CeilingClamp — the final safety stage (ADR-0002 / ADR-0006, DSP_POLICY
//  invariant 4). ALWAYS the last stage before dither, in both EQ positions;
//  structurally separate from the limiter, never folded into it; base rate,
//  outside the oversampled region (it must sit downstream of a Post-position
//  EQ, which is itself outside that region).
//
//  TWO PATHS, selected by `truePeakMode` (ADR-0006 item 3):
//
//  • OFF — the sample-level hard clamp at the linear ceiling (`processSample`),
//    unchanged since P1. No delay, no state.
//
//  • ON — ADR-0041: the gain acts on the clamp's OWN true-peak estimate of its
//    own input (ADR-0006 item 2 — the limiter's detector reads a different
//    signal point once a Post EQ or the decimation filter sits between them),
//    with the sample-level hard clip kept as the backstop. Until 0.2.13 this
//    half of item 3 was never built and TP mode ran the OFF path: a sample
//    clip leaves inter-sample overs it never measured, measured at up to
//    +4.8 dB over the ceiling (audit finding DSP-001). The estimate is the
//    ClampTruePeakDetector's (TruePeak.h), not the meter's alone: true-peak
//    meters disagree by up to ~1.4 dB on HF-rich programme, so the clamp
//    holds the ceiling on the largest of three readings — an accurate 32-tap
//    interpolator (quarter points refined by a parabola), the product's own
//    4× meter, and the BS.1770 Annex 2 example filter.
//
//  THE TRUE-PEAK PATH NEEDS LOOKAHEAD, and that is physics, not a choice: the
//  estimate describing the segment between x[j] and x[j+1] arrives 16 steps
//  late and reads 32 samples, x[j−15..j+16], so the gain on any one sample is
//  constrained by the 32 segments whose interpolation reads it, the newest of
//  which is known 31 steps after the sample arrives. A gain that must also
//  ramp rather than step (A samples, the attack) needs A−1 steps more. The
//  path therefore delays its audio by
//
//      truePeakDelay = attack + 30        (42 samples at 48 kHz, 0.875 ms)
//
//  and the ENGINE takes that delay out of the constant 10 ms allowance rather
//  than adding it to the reported latency (ADR-0041, amending ADR-0004 for
//  TP mode only) — the reported figure never moves.
//
//  Per step n, all channels together (one LINKED gain, so the image does not
//  move when one side is caught):
//    r[j]  = min(1, ceil_j / tp_ch[j]) over channels, j = n−16, where ceil_j
//            is the lower of the ceilings stored with x[j] and x[j+1];
//    q[k]  = min(r[k−16 .. k+15])      — every segment that reads x[k];
//    m[k]  = min(q[k .. k+A−1])        — forward minimum over the attack;
//    ga[k] = mean(m[k−A+1 .. k])       — linear attack ramp, ga[k] ≤ q[k]
//            because every term's window contains k;
//    g[k]  = 1 − max(1 − ga[k], β·(1 − g[k−1]))   — a one-pole release that
//            can only ever hold the gain LOWER than the attack asks for;
//    out   = clip(x[k]·g[k], ±ceil_k),  k = n − truePeakDelay.
//
//  EXACT PASS-THROUGH WHEN IDLE: every ring holds exactly 1.0 when nothing is
//  over, the mean of A ones is exactly 1.0f, and the release of 1.0 is 1.0 —
//  so a true peak under the ceiling leaves the samples bit-identical, only
//  delayed. After a reduction the release snaps to exactly 1.0f once it is
//  within −120 dB of it (~13 time constants), after which the path is exact
//  again.
//
//  NON-FINITE: the engine's stage-E boundary sanitises every value before it
//  reaches this stage. A finite but astronomical input can overflow the
//  estimate: +inf gives r = 0 (the samples are silenced for the window), NaN
//  fails the `tp > ceil` test and leaves the requirement at 1, and the
//  backstop bounds the sample either way — so no finite input produces a
//  non-finite output, and the only recursive value, the reduction, is a max
//  of values in [0, 1] ordered so that a NaN operand loses.
// ============================================================================

namespace anabasis
{

class CeilingClamp
{
public:
    static constexpr int kMaxChannels = 2;

    // Voicing constants of the true-peak path (ADR-0041). The ATTACK sets the
    // lookahead the path takes from the allowance, so it is kept short: the
    // limiter upstream does the shaping, this stage only makes the promise.
    // The RELEASE never affects the ceiling (g ≤ ga always) — only how quickly
    // a caught peak's reduction lets go.
    static constexpr double kAttackMs         = 0.25;
    static constexpr int    kMinAttackSamples = 8;
    static constexpr double kReleaseMs        = 10.0;
    static constexpr float  kReductionSnap    = 1.0e-6f;   // −120 dB

    // The estimator's 16-step lag + its 32-sample interpolation span, less
    // one: the step at which the last segment reading x[k] is known,
    // relative to k.
    static constexpr int kRequirementLead = ClampTruePeakDetector::kTaps - 1;   // 31

    CeilingClamp() = default;

    // ---- OFF path ----------------------------------------------------------
    // The ceiling arrives PER SAMPLE from the engine's smoother, and it is the
    // SAME instantaneous value the limiter's gain computer used for this
    // sample — so the clamp is a backstop for a limiter that failed, never a
    // second, differently-timed threshold that could clip a correctly limited
    // signal while the control glides.
    float processSample (float x, float ceilingLinear) const noexcept
    {
        if (x >  ceilingLinear) return  ceilingLinear;
        if (x < -ceilingLinear) return -ceilingLinear;
        return x;   // untouched below the ceiling — bit-exact for inv 7's null
    }

    // ---- ON path -----------------------------------------------------------
    // The delay the true-peak path adds at `sampleRate` — a pure function, so
    // the engine can size its composition from it before prepare() runs here.
    static int truePeakDelayFor (double sampleRate) noexcept
    {
        return attackFor (sampleRate) + kRequirementLead - 1;
    }

    // Allocates; message thread / prepare only.
    void prepare (double sampleRate)
    {
        attack = attackFor (sampleRate);
        delay  = attack + kRequirementLead - 1;
        const double relSamples = juce::jmax (1.0, kReleaseMs * 0.001 * sampleRate);
        releaseKeep = (float) std::exp (-1.0 / relSamples);

        for (auto& a : audio)
            a.assign ((size_t) delay + 1, 0.0f);
        ceilings.assign ((size_t) delay + 1, 1.0f);
        requirement.assign ((size_t) ClampTruePeakDetector::kTaps, 1.0f);
        need.assign ((size_t) attack, 1.0f);
        forwardMin.assign ((size_t) attack, 1.0f);
        estimator.prepare();
        reset();
    }

    // Audio thread (the engine calls it from reset() and from the silent-bottom
    // latch); allocation-free.
    void reset() noexcept
    {
        for (auto& a : audio)
            std::fill (a.begin(), a.end(), 0.0f);
        std::fill (ceilings.begin(), ceilings.end(), 1.0f);
        std::fill (requirement.begin(), requirement.end(), 1.0f);
        std::fill (need.begin(), need.end(), 1.0f);
        std::fill (forwardMin.begin(), forwardMin.end(), 1.0f);
        estimator.reset();
        writePos = reqPos = needPos = minPos = 0;
        reqBelow = needBelow = minBelow = 0;
        gain = 1.0f;
        reduction = 0.0f;
        ceilingsPrimed = false;
    }

    int truePeakDelay() const noexcept { return delay; }

    // Push one frame (post-EQ, all channels) with the ceiling it was limited
    // against; `frame` is replaced by the frame that entered `truePeakDelay()`
    // steps ago, reduced so its true peak (as ClampTruePeakDetector reads it,
    // the largest of its three readings) stays at or under ITS OWN ceiling,
    // then hard-clipped against that ceiling. Allocation-free.
    void processFrameTruePeak (float* frame, int numCh, float ceilingLinear) noexcept
    {
        const int nCh  = juce::jmin (numCh, kMaxChannels);
        const int size = delay + 1;

        // The ring's pre-roll carries no ceiling of its own: adopt the first
        // one, so a segment straddling the reset boundary is judged against the
        // ceiling actually in force rather than a placeholder.
        if (! ceilingsPrimed)
        {
            std::fill (ceilings.begin(), ceilings.end(), ceilingLinear);
            ceilingsPrimed = true;
        }

        for (int ch = 0; ch < nCh; ++ch)
            audio[(size_t) ch][(size_t) writePos] = frame[ch];
        ceilings[(size_t) writePos] = ceilingLinear;

        // r[j], j = n − 16: the detector's reading of x[j] and of the waveform
        // between x[j] and x[j+1]; the lower of their two ceilings governs.
        constexpr int lag = ClampTruePeakDetector::kLag;
        float tp[kMaxChannels] = {};
        estimator.processFrame (frame, nCh, tp);
        const float segCeil = juce::jmin (ceilings[(size_t) wrap (writePos - lag, size)],
                                          ceilings[(size_t) wrap (writePos - lag + 1, size)]);
        float r = 1.0f;
        for (int ch = 0; ch < nCh; ++ch)
            if (tp[ch] > segCeil)                 // NaN fails this: see the header
                r = juce::jmin (r, segCeil / tp[ch]);

        // THE THREE WINDOWS BELOW ARE SCANNED ONLY WHILE THEY HOLD SOMETHING
        // BELOW 1. Each ring keeps a count of its entries under unity, so the
        // common case — nothing near the ceiling, nothing still ramping — is a
        // few compares, and the idle answer (exactly 1, exactly zero reduction)
        // is produced without arithmetic that could round it.
        //
        // q[k], k = n − 31: every segment whose interpolation reads x[k].
        const float q = pushAndMin (requirement, reqPos, reqBelow, r);

        // m[k], k = n − 30 − A: the forward minimum over the attack…
        const float m = pushAndMin (need, needPos, needBelow, q);

        // …and its A-sample mean, the ramp. Summed afresh (A adds) whenever
        // anything in it is below 1, rather than as a running sum, which would
        // drift over a long session.
        float sum = (float) attack;
        {
            float& slot = forwardMin[(size_t) minPos];
            minBelow += (m < 1.0f ? 1 : 0) - (slot < 1.0f ? 1 : 0);
            slot = m;
            if (++minPos == attack)
                minPos = 0;
            if (minBelow > 0)
            {
                sum = 0.0f;
                for (const float v : forwardMin)
                    sum += v;
            }
        }
        // The release runs on the REDUCTION (1 − g), not on the gain. Near
        // unity the gain has only 2⁻²⁴ of resolution, so a one-pole written on
        // it stalls as soon as a step is worth less than half an ulp — about
        // 240 ulps (−0.0001 dB) short of 1.0 at this time constant — and the
        // path would never be exact again. The reduction keeps full precision
        // all the way down, and snaps to exactly 0 at −120 dB.
        const float attackReduction = 1.0f - sum / (float) attack;
        float releaseReduction = reduction * releaseKeep;
        if (releaseReduction < kReductionSnap)
            releaseReduction = 0.0f;
        reduction = attackReduction > releaseReduction ? attackReduction : releaseReduction;
        gain = 1.0f - reduction;

        const int   readPos = wrap (writePos + 1, size);   // the oldest entry: n − delay
        const float c       = ceilings[(size_t) readPos];
        for (int ch = 0; ch < nCh; ++ch)
        {
            float y = audio[(size_t) ch][(size_t) readPos];
            if (! juce::exactlyEqual (gain, 1.0f))
                y *= gain;
            frame[ch] = processSample (y, c);     // the backstop
        }
        writePos = readPos;
    }

    // The linked gain applied to the frame last emitted (1 = no reduction).
    float currentGain() const noexcept { return gain; }

private:
    static int attackFor (double sampleRate) noexcept
    {
        return juce::jmax (kMinAttackSamples, (int) std::lround (kAttackMs * 0.001 * sampleRate));
    }

    static int wrap (int i, int size) noexcept
    {
        return i < 0 ? i + size : (i >= size ? i - size : i);
    }

    // Write `v` into a sliding window (a ring) and return the window's
    // minimum, keeping `below` = the number of entries under 1 so that a
    // window of ones answers without a scan.
    static float pushAndMin (std::vector<float>& ring, int& pos, int& below, float v) noexcept
    {
        float& slot = ring[(size_t) pos];
        below += (v < 1.0f ? 1 : 0) - (slot < 1.0f ? 1 : 0);
        slot = v;
        if (++pos == (int) ring.size())
            pos = 0;
        if (below == 0)
            return 1.0f;
        float lo = 1.0f;
        for (const float e : ring)
            lo = juce::jmin (lo, e);
        return lo;
    }

    ClampTruePeakDetector estimator;
    std::vector<float> audio[kMaxChannels];
    std::vector<float> ceilings, requirement, need, forwardMin;
    int   attack = kMinAttackSamples, delay = kMinAttackSamples + kRequirementLead - 1;
    int   writePos = 0, reqPos = 0, needPos = 0, minPos = 0;
    int   reqBelow = 0, needBelow = 0, minBelow = 0;   // entries under 1 in each window
    float gain = 1.0f, reduction = 0.0f, releaseKeep = 0.0f;
    bool  ceilingsPrimed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CeilingClamp)
};

} // namespace anabasis
