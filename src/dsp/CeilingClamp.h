#pragma once

#include "ClampTruePeakDetector.h"
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
//    ClampTruePeakDetector's (ClampTruePeakDetector.h), not the meter's alone:
//    true-peak meters disagree by up to ~1.4 dB on HF-rich programme, so the clamp
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
//      truePeakDelay = attack + 30        (46 samples at 48 kHz, 0.958 ms)
//
//  and the ENGINE takes that delay out of the constant 10 ms allowance rather
//  than adding it to the reported latency (ADR-0041, amending ADR-0004 for
//  TP mode only) — the reported figure never moves.
//
//  THE CEILING A FRAME IS JUDGED AGAINST is the one in force when it is
//  EMITTED, truePeakDelay steps after it enters: the engine hands each frame
//  the value its ceiling smoother will have then (the smoother is a
//  deterministic linear ramp between block-rate retargets, so the engine runs
//  a copy of it truePeakDelay steps ahead). A retarget at a block top can move
//  that future below what the frames already in flight were judged against;
//  `lowerInFlightCeilings` then lowers (never raises) their stored ceilings to
//  the new trajectory and re-derives every requirement not yet applied from
//  the stored per-segment detector readings. With a static ceiling the
//  emission-time value IS the entry-time value, and nothing here changes.
//  (ADR-0045, amending ADR-0041 decision 3. Until 0.2.15 each frame carried
//  the ceiling in force when it ENTERED, so while the ceiling descended the
//  output answered to a value truePeakDelay samples old: up to
//  20·log10(1 + (D/R)(c0/c1 − 1)) dB over the live one — +2.7 dB for an
//  instant −1 → −20 dB cut at 44.1 kHz — where the TP-off clip, which reads
//  the live value, had none. The review of PR #42.)
//
//  Per step n, all channels together (one LINKED gain, so the image does not
//  move when one side is caught):
//    r[j]  = min(1, ceil_j / tp_ch[j]) over channels, j = n−16, where ceil_j
//            is the lower of the ceilings stored with x[j] and x[j+1];
//    q[k]  = min(r[k−16 .. k+15])      — every segment that reads x[k];
//    m[k]  = min(q[k .. k+A−1])        — forward minimum over the attack;
//    ga[k] = Σ w_a·m[k−a], a = 0..A−1 — the attack ramp, a weighted mean
//            with w_a ∝ e^(λ·a), λ = 4.8/A (the OLDEST term weighs most), so
//            ga[k] ≤ q[k] because every term's window contains k;
//    g[k]  = 1 − max(1 − ga[k], β·(1 − g[k−1]), 1 − (1 + μ)·g[k−1])
//            — a one-pole release that can only ever hold the gain LOWER than
//            the attack asks for, and may raise it by at most (1 + μ) a sample;
//    out   = clip(x[k]·g[k], ±ceil_k),  k = n − truePeakDelay.
//
//  WHY THE RAMP'S SHAPE IS THE GUARANTEE (KNOWN_ISSUES KI-025). The
//  requirement keeps every gain a segment reads under that segment's r, which
//  bounds its interpolated peak only while those gains are EQUAL. With
//  y = g·x, G the largest gain in a segment's 12-tap defining window and
//  |x_k| ≤ ceil/G for every tap k of it (the 32-sample requirement window of
//  segment k contains the whole 12-tap window, and tp_k ≥ |x_k|):
//      reading(y) ≤ G·reading(x) + Σ_k (G − g_k)·|h_k|·|x_k|
//                 ≤ ceil·(G/r + Σ_k (1 − g_k/G)·|h_k|).
//  A segment whose window holds only the flat top of a ramp (G = r) has no
//  headroom, so the dips (1 − g_k/G) under its side lobes must be tiny; a
//  segment further down the ramp has headroom 1 − G/r ≥ 1 − g(j−5)/g(j−15)
//  from the 10 samples by which its 32-sample window leads its 12-tap one, so
//  the ramp may steepen there. A LINEAR ramp (the 0.2.15 boxcar, A = 8) puts
//  its steepest slope at the top and read up to +0.39 dB (Annex 2) in the
//  bound below for a 6 dB-deep requirement; the geometric weights start the
//  ramp at ~0.25 % of its depth per sample and keep the steep part where the
//  headroom is. Maximised over every input consistent with the requirements
//  (a linear programme over the 7 defining phases, the accurate kernel's
//  quarter phases and the sample, the law's gain profile given), a single
//  deeper requirement reads at most +0.011 / +0.025 / +0.032 / +0.035 dB over
//  the ceiling at 3 / 10 / 20 / 40 dB depth with A = 16 (+0.016 dB at 20 dB
//  with A = 24, the 96 kHz attack) — at any rate, the bound being per sample;
//  a second, deeper requirement starting on the first one's ramp, or a gap
//  between two, read at most +0.032 dB. The shape is a balance, not a
//  monotone knob: λ = 0.6 (a steeper bottom) reads +0.57 dB at 20 dB depth,
//  and A = 12 with λ·A = 4.8 reads +0.13 dB, so the attack floor is 16.
//  A RELEASE that runs into a later, lower requirement meets it at its own
//  slope, with no headroom on the far side, and at low rates the
//  one-pole's relative slope (1 − β)(1 − g)/g is large (the same programme:
//  +0.48 dB at 4 kHz, +0.24 dB at 8 kHz, 20 dB release into a 12 dB cap); μ
//  caps it at 1 % per sample, +0.061 dB there. μ binds only below ~−15 dB of
//  clamp reduction at 44.1 kHz (the one-pole is faster than 1 % a sample only
//  there), so ordinary reductions release exactly as before.
//
//  THE ONE PLACE THE RAMP STEPS (ADR-0045). A revision re-derives m for the
//  steps the next emissions read, so the frame emitted right after a downward
//  retarget takes its revised requirement at once, and that step lowers the
//  side lobes of segments whose main lobes already left at the old gain — the
//  same mechanism as KI-025, with no lookahead left to ease it. The step is set
//  by how far and how early a REVISED requirement reaches: with the 32-sample
//  reach of the entry-time law the next frame answered to a segment 15 samples
//  on, ~(17 + E) glide steps (E ≈ 1/(e^λ − 1) ≈ 3, the eased ramp's own
//  lookahead). A revision now applies a revised r_j only to the frames its two
//  DEFINING readings read (x[j−5 .. j+6]), judges the taps before its main
//  lobe against the glide kRevisionLead frames after the tap, and leaves the
//  entry-time requirements' full reach alone (planned with the attack's
//  lookahead, it steps nothing): about (Z + 1 + E) glide steps — 4.9 measured
//  at every rate below 66 kHz, 0.0040 at 48 kHz for a −1 → −20 dB cut on DC
//  held at the ceiling where the 0.2.15 law stepped 0.0185 (22.5); pinned by
//  testTruePeakModeBoundsTheStepAtACeilingCut. The details, and what the
//  relaxation lets through, are at lowerInFlightCeilings. THE GUARANTEE STILL
//  DEPENDS ON THE GLIDE'S SLOPE: a revision cannot reach the already-emitted
//  half of a segment that straddles the emission point, and the step's excess
//  scales with the glide step 0.9/(0.02·sr) at a full-range cut, so it grows
//  as the rate falls: the engine engages the path from kMinTruePeakRate.
//
//  EXACT PASS-THROUGH WHEN IDLE: every ring holds exactly 1.0 when nothing is
//  over, the ramp is summed on the reductions 1 − m (exactly 0 then, whatever
//  the weights round to), and the release of 1.0 is 1.0 —
//  so a true peak under the ceiling leaves the samples bit-identical, only
//  delayed. After a reduction the release snaps to exactly 1.0f once it is
//  within −120 dB of it (~13 time constants), after which the path is exact
//  again.
//
//  NON-FINITE: the engine's stage-E boundary sanitises every value before it
//  reaches this stage. A finite but astronomical input can overflow the
//  estimate: +inf gives r = 0 (the samples are silenced for the window, and
//  the gain then climbs back under the release's rise cap from its −80 dB
//  floor, ~920 samples to unity), NaN
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
    static constexpr int    kMinAttackSamples = 16;        // KI-025: the ease-in needs 16 (below)
    static constexpr double kReleaseMs        = 10.0;
    static constexpr float  kReductionSnap    = 1.0e-6f;   // −120 dB

    // KI-025 — THE ATTACK EASES IN, THE RELEASE IS RATE-LIMITED NEAR THE TOP.
    // The attack ramp is the forward minimum averaged with weights that grow
    // geometrically with age (w ∝ e^(λ·age), λ·A = kEaseSpan), not a boxcar:
    // the ramp leaves the level it starts from with a first step of
    // ~e^(−kEaseSpan)·λ ≈ 0.25 % of its depth and reaches the deeper level
    // with its steepest steps, where every segment reading them sits well under
    // its own requirement. The release may raise the gain by at most a factor
    // (1 + kReleaseRise) per sample, so a release that runs into a later,
    // lower requirement meets it at a bounded RELATIVE slope. The derivation
    // is at the class banner.
    static constexpr double kEaseSpan         = 4.8;
    static constexpr float  kReleaseRise      = 0.01f;
    static constexpr float  kRiseFloor        = 1.0e-4f;   // −80 dB
    static constexpr int    kRevisionLead     = 1;         // Z, lowerInFlightCeilings

    // The lowest rate the engine ENGAGES this path at (AnabasisEngine::prepare's
    // rail). The static figure above is per sample, so rate-free; the revision
    // step at a downward retarget is (Z + 1 + E) glide steps, and a full-range
    // cut's glide step is 0.9 / (0.02·sr), so its excess grows as the rate
    // falls. Over the live ceiling, Annex 2 (ADR-0046, the fourth PR #42 round):
    //                         8 kHz    11.025 kHz   12 kHz   16 kHz
    //   derived bound¹       +0.1008   +0.0733     +0.0672  +0.0504
    //   combined search²     +0.1213   +0.0999     +0.0930  +0.0784
    //   clamp-level search   +0.0867   +0.0673     +0.0629  +0.0509
    // ¹ a global branch-and-bound over the straddling segment's readings, for
    //   inputs not already under reduction when the cut arrives;
    // ² a search over requirement sequences (static activity plus the
    //   revision) with an exact inner maximiser — a relaxation's search value,
    //   neither a bound nor a realised input.
    // 12 kHz is the lowest common host rate every figure keeps under the
    // 0.1 dB tolerance; at 8 kHz the bound does not, and at 11.025 kHz the
    // combined search sits at the tolerance. No search found a real input over
    // it at 8 kHz — the rail follows what can be supported, not what was found.
    static constexpr double kMinTruePeakRate  = 12000.0;

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
        const double relSamples = truepeak::max2 (1.0, kReleaseMs * 0.001 * sampleRate);
        releaseKeep = (float) std::exp (-1.0 / relSamples);

        for (auto& a : audio)
            a.assign ((size_t) delay + 1, 0.0f);
        ceilings.assign ((size_t) delay + 1, 1.0f);
        requirement.assign ((size_t) ClampTruePeakDetector::kTaps, 1.0f);
        need.assign ((size_t) attack, 1.0f);
        forwardMin.assign ((size_t) attack, 1.0f);
        // The ease-in weights, by AGE of the forward minimum (0 = newest):
        // w[a] ∝ e^(λ·a), λ = kEaseSpan / A, normalised in double.
        easeWeight.assign ((size_t) attack, 0.0f);
        {
            const double lambda = kEaseSpan / (double) attack;
            double total = 0.0;
            for (int a = 0; a < attack; ++a)
                total += std::exp (lambda * (double) a);
            for (int a = 0; a < attack; ++a)
                easeWeight[(size_t) a] = (float) (std::exp (lambda * (double) a) / total);
        }
        // A revision rebuilds m for the last A steps, which reads q over
        // 2A − 1 steps, which reads r over 2A + 30 segments.
        const int hist = 2 * attack + ClampTruePeakDetector::kTaps;
        segPeak.assign ((size_t) hist, 0.0f);
        segReq.assign ((size_t) hist, 1.0f);
        segReqEntry.assign ((size_t) hist, 1.0f);
        for (auto* v : { &scratchA, &scratchB, &scratchC, &scratchQ, &scratchE })
            v->assign ((size_t) hist, 1.0f);
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
        std::fill (segPeak.begin(), segPeak.end(), 0.0f);
        std::fill (segReq.begin(), segReq.end(), 1.0f);
        std::fill (segReqEntry.begin(), segReqEntry.end(), 1.0f);
        estimator.reset();
        writePos = reqPos = needPos = minPos = histPos = 0;
        reqBelow = needBelow = minBelow = 0;
        gain = 1.0f;
        reduction = 0.0f;
        ceilingsPrimed = false;
    }

    int truePeakDelay() const noexcept { return delay; }

    // Push one frame (post-EQ, all channels) with the ceiling that will be in
    // force when it is EMITTED, `truePeakDelay()` steps from now; `frame` is
    // replaced by the frame that entered `truePeakDelay()` steps ago, reduced
    // so its true peak (as ClampTruePeakDetector reads it, the largest of its
    // three readings) stays at or under ITS OWN stored ceiling (as lowered by
    // any `lowerInFlightCeilings` since), then hard-clipped against that
    // ceiling. Allocation-free.
    void processFrameTruePeak (float* frame, int numCh, float ceilingLinear) noexcept
    {
        const int nCh  = truepeak::min2 (numCh, kMaxChannels);
        if (nCh <= 0)
            return;                     // an empty frame: nothing to delay, read or emit
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
        const float segCeil = truepeak::min2 (ceilings[(size_t) wrap (writePos - lag, size)],
                                              ceilings[(size_t) wrap (writePos - lag + 1, size)]);
        float r = 1.0f;
        for (int ch = 0; ch < nCh; ++ch)
            if (tp[ch] > segCeil)                 // NaN fails this: see the header
                r = truepeak::min2 (r, segCeil / tp[ch]);

        // The segment's reading (the channels' largest; NaN loses) and its
        // requirement, kept so a lowered ceiling can re-derive r without the
        // audio: segCeil / max(tp) IS the min over channels above, because a
        // correctly rounded division is monotone in its divisor.
        {
            float peak = 0.0f;
            for (int ch = 0; ch < nCh; ++ch)
                if (tp[ch] > peak)
                    peak = tp[ch];
            segPeak[(size_t) histPos]     = peak;
            segReq[(size_t) histPos]      = r;
            segReqEntry[(size_t) histPos] = r;     // as first judged (never revised)
            if (++histPos == (int) segReq.size())
                histPos = 0;
        }

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

        // …and its weighted A-sample mean, the ramp (KI-025: weights growing
        // with age, so the ramp eases in). Taken on the REDUCTIONS 1 − m, so a
        // window of ones answers exactly 0 without the weights' rounding, and
        // summed afresh (A multiply-adds) whenever anything in it is below 1
        // rather than as a running sum, which would drift over a long session.
        float attackReduction = 0.0f;
        {
            float& slot = forwardMin[(size_t) minPos];
            minBelow += (m < 1.0f ? 1 : 0) - (slot < 1.0f ? 1 : 0);
            slot = m;
            if (++minPos == attack)
                minPos = 0;
            if (minBelow > 0)
            {
                int s = minPos;                    // the oldest entry: age A − 1
                for (int a = attack - 1; a >= 0; --a)
                {
                    attackReduction += easeWeight[(size_t) a] * (1.0f - forwardMin[(size_t) s]);
                    if (++s == attack)
                        s = 0;
                }
            }
        }
        // The release runs on the REDUCTION (1 − g), not on the gain. Near
        // unity the gain has only 2⁻²⁴ of resolution, so a one-pole written on
        // it stalls as soon as a step is worth less than half an ulp — about
        // 240 ulps (−0.0001 dB) short of 1.0 at this time constant — and the
        // path would never be exact again. The reduction keeps full precision
        // all the way down, and snaps to exactly 0 at −120 dB.
        // KI-025: the gain rises by at most a factor (1 + kReleaseRise) per
        // sample — in the reduction domain, 1 − red ≤ (1 − red_prev)(1 + μ)
        // + μ·kRiseFloor, the floor only so that a gain silenced to exactly 0
        // (an overflowing estimate, the NON-FINITE note) can still recover.
        // Near unity the one-pole is the slower of the two, so the idle path
        // and the snap below are untouched.
        float releaseReduction = reduction * releaseKeep;
        releaseReduction = truepeak::max2 (releaseReduction,
                                           reduction - kReleaseRise * (1.0f - reduction + kRiseFloor));
        if (releaseReduction < kReductionSnap)
            releaseReduction = 0.0f;
        reduction = attackReduction > releaseReduction ? attackReduction : releaseReduction;
        gain = 1.0f - reduction;

        const int   readPos = wrap (writePos + 1, size);   // the oldest entry: n − delay
        const float c       = ceilings[(size_t) readPos];
        for (int ch = 0; ch < nCh; ++ch)
        {
            float y = audio[(size_t) ch][(size_t) readPos];
            if (gain < 1.0f)                      // gain ≤ 1 always: this IS "≠ 1"
                y *= gain;
            frame[ch] = processSample (y, c);     // the backstop
        }
        writePos = readPos;
    }

    // The linked gain applied to the frame last emitted (1 = no reduction).
    float currentGain() const noexcept { return gain; }

    // A LOWERED CEILING TRAJECTORY. `emitCeil[i]` is the ceiling in force when
    // the i-th oldest frame still in flight is emitted (i = 0: the frame the
    // next processFrameTruePeak emits; `count` ≤ truePeakDelay()). Each stored
    // ceiling is lowered to it, never raised; if any moved, every requirement
    // that still constrains a gain not yet applied is re-derived from the
    // stored detector readings — r for the segments reading a lowered frame,
    // then q over the last 2A − 1 steps and m over the last A, exactly the
    // windows the next steps read. A revised r_j now reaches only its two
    // DEFINING readings' frames, and the taps before its main lobe answer to
    // the glide kRevisionLead frames on (KI-025, the rebuild below), so every
    // gain still to be emitted satisfies ga[k] ≤ q[k] for that relaxed q; the
    // entry-time requirements keep their full reach. The release state is
    // kept: g ≤ ga holds whatever it is. Allocation-free; bounded work: `count`
    // compares when no ceiling moves, about 6A + 105 more when ceilings move
    // but no requirement does, about 40A + 290 in all when one does (A =
    // attack: ~930 at 48 kHz, ~2210 at 192 kHz), once per block at most.
    void lowerInFlightCeilings (const float* emitCeil, int count) noexcept
    {
        if (! ceilingsPrimed)
            return;                               // nothing in flight yet
        const int size = delay + 1;
        const int nIn  = truepeak::min2 (count, delay);
        int first = -1;
        for (int i = 0; i < nIn; ++i)
        {
            float& c = ceilings[(size_t) wrap (writePos + 1 + i, size)];
            if (emitCeil[i] < c)                  // NaN fails: never lowered to NaN
            {
                c = emitCeil[i];
                if (first < 0)
                    first = i;
            }
        }
        if (first < 0)
            return;

        // Offsets from the LAST step n: frame x[n − t] sits in ceilings slot
        // writePos − 1 − t; segment j = n − 16 − u (u = 0: the newest reported)
        // in history slot histPos − 1 − u and reads x[n − 16 − u], x[n − 15 − u].
        // In-flight frame i is x[n + 1 − D + i], so only u ≤ D − 16 − first moved.
        constexpr int lag = ClampTruePeakDetector::kLag;
        const int hist = (int) segReq.size();
        bool moved = false;
        for (int u = 0; u <= delay - lag - first; ++u)
        {
            const int   t       = lag + u;
            const float segCeil = truepeak::min2 (ceilings[(size_t) wrap (writePos - 1 - t, size)],
                                                  ceilings[(size_t) wrap (writePos - t, size)]);
            const int   h       = wrap (histPos - 1 - u, hist);
            const float peak    = segPeak[(size_t) h];
            if (peak > segCeil && segCeil / peak < segReq[(size_t) h])
            {
                segReq[(size_t) h] = segCeil / peak;
                moved = true;
            }
        }
        if (! moved)
            return;          // no requirement moved: q and m are functions of r alone

        // r, newest first: rr[u] = r[n − 16 − u], u = 0 .. 2A + 29.
        constexpr int taps = ClampTruePeakDetector::kTaps;
        const int nR = 2 * attack + taps - 2;
        float* rr = scratchA.data();
        for (int u = 0; u < nR; ++u)
            rr[u] = segReq[(size_t) wrap (histPos - 1 - u, hist)];

        // q, newest first, v = 0 .. 2A − 2: qq[v] = q[n − 31 − v]; frames
        // v ≤ A − 2 are still to be emitted, the rest have left (their q only
        // feeds the forward minima of the next steps' ramp).
        //
        // KI-025 — THE REVISION STEPS LESS. The step a revision forces at the
        // emission point is set by how far a REVISED requirement reaches and
        // how early. The entry-time requirements keep their full 32-sample
        // reach (planned with the attack's lookahead, it steps nothing); a
        // revised r_j reaches only the frames its two DEFINING readings read,
        // x[j−5 .. j+6] (segments k−6 .. k+5 for frame k — only the accurate
        // interpolator's taps 7..16 samples out are relaxed, and that reading
        // does not define dBTP, ADR-0043); and on the taps BEFORE its main lobe
        // it is judged against the ceiling kRevisionLead frames after the tap
        // rather than against its own, lower one:
        //     r_j(k) = min(1, max(ceil_j, min ceil[k .. k+Z]) / peak_j),  k < j,
        // so frame k answers to the glide Z frames ahead of it, not to a
        // segment up to 6 frames ahead. What that lets through is the glide's
        // fall between frame k+Z and the segment, on taps d ≥ Z before the
        // main lobe: Σ_d (d + 1 − Z)·(glide step)·|h_d| ≤ 0.81 glide steps of
        // the ceiling for Z = 1 on every defining phase (Annex 2 phase 1; 0.39
        // for Z = 2, 1.22 for Z = 0 — Z = 1 measured best). Frames already
        // emitted keep their entry-time q: lowering them can no longer protect
        // anything and would only deepen the next steps' forward minima.
        float* rrE = scratchE.data();
        for (int u = 0; u < nR; ++u)
            rrE[u] = segReqEntry[(size_t) wrap (histPos - 1 - u, hist)];
        float* qq = scratchQ.data();
        windowMin (rrE, nR, taps, scratchB.data(), scratchC.data(), qq);
        const int nQ = nR - taps + 1;             // 2A − 1
        for (int v = 0; v <= attack - 2 && v < nQ; ++v)
        {
            // min ceil[k .. k+Z], frame k = n − 31 − v sits t = 31 + v back
            float cAhead = 1.0f;
            for (int z = 0; z <= kRevisionLead; ++z)
                cAhead = truepeak::min2 (cAhead, ceilings[(size_t) wrap (writePos - 1 - (31 + v - z), size)]);
            float qv = qq[v];
            for (int u = v + 10; u <= v + 21; ++u)          // segments k − 6 .. k + 5
            {
                if (u > v + 14)                              // k ≥ j: main lobe and after
                {
                    qv = truepeak::min2 (qv, rr[u]);
                    continue;
                }
                const int   t    = lag + u;                  // k < j: before the main lobe
                const float segC = truepeak::min2 (ceilings[(size_t) wrap (writePos - 1 - t, size)],
                                                   ceilings[(size_t) wrap (writePos - t, size)]);
                const float cRel = truepeak::max2 (segC, cAhead);
                const float peak = segPeak[(size_t) wrap (histPos - 1 - u, hist)];
                if (peak > cRel)
                    qv = truepeak::min2 (qv, cRel / peak);
            }
            qq[v] = qv;
        }

        reqBelow = 0;
        for (int u = 0; u < taps; ++u)
        {
            float& slot = requirement[(size_t) wrap (reqPos - 1 - u, taps)];
            slot = rr[u];
            reqBelow += slot < 1.0f ? 1 : 0;
        }
        needBelow = 0;
        for (int v = 0; v < attack; ++v)
        {
            float& slot = need[(size_t) wrap (needPos - 1 - v, attack)];
            slot = qq[v];
            needBelow += slot < 1.0f ? 1 : 0;
        }
        // m, newest first: the m of step n − w is min qq[w .. w + A − 1].
        float* mm = scratchA.data();              // rr is no longer needed
        windowMin (qq, nQ, attack, scratchB.data(), scratchC.data(), mm);
        minBelow = 0;
        for (int w = 0; w < attack; ++w)
        {
            float& slot = forwardMin[(size_t) wrap (minPos - 1 - w, attack)];
            slot = mm[w];
            minBelow += slot < 1.0f ? 1 : 0;
        }
    }

private:
    static int attackFor (double sampleRate) noexcept
    {
        return truepeak::max2 (kMinAttackSamples, (int) std::lround (kAttackMs * 0.001 * sampleRate));
    }

    static int wrap (int i, int size) noexcept
    {
        return i < 0 ? i + size : (i >= size ? i - size : i);
    }

    // y[i] = min (a[i .. i + w − 1]), i = 0 .. n − w, in about 3n operations
    // (van Herk / Gil-Werman: prefix and suffix minima over blocks of w).
    static void windowMin (const float* a, int n, int w, float* pre, float* suf, float* y) noexcept
    {
        for (int i = 0; i < n; ++i)
            pre[i] = (i % w == 0) ? a[i] : truepeak::min2 (pre[i - 1], a[i]);
        for (int i = n - 1; i >= 0; --i)
            suf[i] = (i == n - 1 || i % w == w - 1) ? a[i] : truepeak::min2 (suf[i + 1], a[i]);
        for (int i = 0; i + w <= n; ++i)
            y[i] = truepeak::min2 (suf[i], pre[i + w - 1]);
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
            lo = truepeak::min2 (lo, e);
        return lo;
    }

    ClampTruePeakDetector estimator;
    std::vector<float> audio[kMaxChannels];
    std::vector<float> ceilings, requirement, need, forwardMin, easeWeight;
    std::vector<float> segPeak, segReq, segReqEntry;   // per segment, newest at histPos − 1
    std::vector<float> scratchA, scratchB, scratchC, scratchQ, scratchE;   // lowerInFlightCeilings
    int   attack = kMinAttackSamples, delay = kMinAttackSamples + kRequirementLead - 1;
    int   writePos = 0, reqPos = 0, needPos = 0, minPos = 0, histPos = 0;
    int   reqBelow = 0, needBelow = 0, minBelow = 0;   // entries under 1 in each window
    float gain = 1.0f, reduction = 0.0f, releaseKeep = 0.0f;
    bool  ceilingsPrimed = false;

public:
    // Non-copyable, spelled out rather than through JUCE's ownership macro:
    // this is one of the engine's JUCE-free leaf headers (ClampTruePeakDetector.h
    // says why), and `tests/realtime_effects.cpp` compiles it with no JUCE on
    // the include path.
    CeilingClamp (const CeilingClamp&) = delete;
    CeilingClamp& operator= (const CeilingClamp&) = delete;
};

// ============================================================================
//  EngagementTail — how true-peak mode is ENGAGED while audio plays (ADR-0041,
//  amended in the PR #42 review).
//
//  THE DEFECT IT CLOSES. Engaging TP changes the clamp's share of the latency
//  allowance, so the composition is latched at a silent point (ADR-0041
//  decision 5). It used to be reached by the §2.8 duck's ~6 ms out-leg — and
//  for that out-leg the OLD composition was still running, so the output kept
//  coming from the sample-only clamp after the user had asked for dBTP:
//  measured up to +4.7 dB (product meter) / +5.5 dB (BS.1770 Annex 2 filter)
//  over the requested ceiling in the first ~2 ms after the toggle (HF-heavy
//  programme into a +12 dB Post shelf; the PR #42 review worklog).
//
//  WHY NOT KEEP A FADE OF THE AUDIO. The samples the out-leg emits were
//  already in the pipeline, and the old composition has no lookahead at the
//  clamp. Making them TP-safe from the first post-toggle sample without one is
//  impossible without either a step in gain (a click) or more latency
//  (ADR-0004): an interpolated true peak depends on samples not yet produced.
//
//  WHAT THE ENGINE DOES INSTEAD. At the toggle it latches the TP composition
//  immediately (the processed path goes silent while the rings refill), and
//  this class continues the LAST EMITTED FRAME as a raised-cosine decay to
//  zero over ~6 ms — value-continuous at the junction, so low-frequency
//  material does not click (measured: a 100 Hz tone's transition splatter is
//  ~56 dB below an instant mute's). Everything that makes an inter-sample
//  peak — the high-frequency detail of the old chain — stops at the toggle.
//
//  THE GUARANTEE, and how it is kept: no true-peak reading of the output from
//  the toggle on exceeds the requested ceiling. A decay is smooth, so its own
//  readings never exceed its first value; the readings that straddle the
//  toggle also contain the last 31 emitted samples, and those are CHECKED here
//  with the clamp's own detector (all three readings) before the first tail
//  sample is emitted. If they would exceed the ceiling the whole tail is scaled
//  down by bisection — the only case in which the junction takes a small step.
//  Scale 0 (plain silence) always fits the ceiling the history was emitted
//  under: the history can put at most 0.56 of its own peak into a reading at or
//  after the toggle (the largest history-tap L1 norm over the three readings'
//  phases), 0.59 with the refinement bound. When the SAME block also lowers the
//  ceiling (a preset or A/B swap that turns TP on), the check is against the
//  new, lower value — measured on hostile programme, cuts of 3 to 10 dB at the
//  toggle held the new ceiling from the toggle on. Were silence ever not to
//  meet it (a cut deeper than ~4.6 dB under adversarial near-Nyquist history),
//  the decay would be silent and what remains is the old audio's own ringing,
//  under the smoothed ceiling the stage enforces during any ceiling move.
//
//  Realtime: fixed storage (no allocation after `prepare`), bounded work — the
//  check replays 63 frames through a private detector, at most 13 times, once
//  per engagement.
// ============================================================================
class EngagementTail
{
public:
    static constexpr int    kMaxChannels = CeilingClamp::kMaxChannels;
    static constexpr int    kHistory     = ClampTruePeakDetector::kTaps;   // 32 emitted frames
    static constexpr double kLengthMs    = 6.0;      // the §2.8 duck's out-leg, the one it replaces
    static constexpr int    kSearchSteps = 12;       // scale resolution 1/4096

    EngagementTail() = default;

    // Message thread / prepare only.
    void prepare (double sampleRate)
    {
        length = truepeak::max2 (1, (int) std::lround (kLengthMs * 0.001 * sampleRate));
        verifier.prepare();
        reset();
    }

    void reset() noexcept
    {
        for (auto& h : history)
            for (auto& v : h)
                v = 0.0f;
        histPos = 0;
        left = 0;
        pos  = 0;
    }

    // Every emitted processed frame (after the duck, before dither): the
    // waveform a tail has to continue. Audio thread.
    void pushEmitted (const float* frame, int numCh) noexcept
    {
        const int nCh = truepeak::min2 (numCh, kMaxChannels);
        for (int ch = 0; ch < kMaxChannels; ++ch)
            history[ch][histPos] = ch < nCh ? frame[ch] : 0.0f;
        if (++histPos == kHistory)
            histPos = 0;                                // histPos now holds the oldest frame
    }

    // At a TP-on engagement: start the decay from the last emitted frame,
    // scaled so no reading of the output from here on exceeds `ceilingLinear`.
    void start (float ceilingLinear) noexcept
    {
        const int last = histPos == 0 ? kHistory - 1 : histPos - 1;
        for (int ch = 0; ch < kMaxChannels; ++ch)
            from[ch] = history[ch][last];
        scale = 1.0f;
        if (! fits (1.0f, ceilingLinear))
        {
            float lo = 0.0f, hi = 1.0f;
            for (int i = 0; i < kSearchSteps; ++i)
            {
                const float mid = 0.5f * (lo + hi);
                if (fits (mid, ceilingLinear))
                    lo = mid;
                else
                    hi = mid;
            }
            scale = lo;
        }
        pos  = 0;
        left = length;
    }

    bool  active() const noexcept          { return left > 0; }
    float appliedScale() const noexcept    { return scale; }

    // The tail's value for channel `ch` at the current step; `advance` once
    // per frame after every channel has read it.
    float value (int ch) const noexcept    { return scale * from[ch] * shape (pos); }
    void  advance() noexcept               { if (left > 0) { --left; ++pos; } }

private:
    // 1 at the junction, falling to ~0 over `length` samples.
    float shape (int k) const noexcept
    {
        if (k >= length)
            return 0.0f;                                // the decay has ended
        return 0.5f * (1.0f + std::cos (truepeak::kPi * (float) k / (float) length));
    }

    // Would a tail at `s` keep every reading from the toggle on within the
    // ceiling? The decay's own samples must (s·|from| ≤ ceiling: a smooth decay
    // reads no higher than its first value), and so must every reading whose
    // window straddles the toggle — replayed here through the clamp's own
    // detector. Frames 0..31 are the history, frame 32 is the first tail
    // sample; the detector reports segment m − 16 at step m, so the segments at
    // or after the toggle whose windows still reach the history are the ones
    // reported at steps 48..62.
    bool fits (float s, float ceilingLinear) noexcept
    {
        for (int ch = 0; ch < kMaxChannels; ++ch)
            if (s * std::abs (from[ch]) > ceilingLinear)
                return false;
        verifier.reset();
        float frame[kMaxChannels] = {};
        float tp[kMaxChannels]    = {};
        for (int i = 0; i < kHistory; ++i)
        {
            const int idx = (histPos + i) % kHistory;           // oldest first
            for (int ch = 0; ch < kMaxChannels; ++ch)
                frame[ch] = history[ch][idx];
            verifier.processFrame (frame, kMaxChannels, tp);
        }
        for (int k = 0; k < kHistory - 1; ++k)
        {
            for (int ch = 0; ch < kMaxChannels; ++ch)
                frame[ch] = s * from[ch] * shape (k);
            verifier.processFrame (frame, kMaxChannels, tp);
            if (k >= ClampTruePeakDetector::kLag)
                for (int ch = 0; ch < kMaxChannels; ++ch)
                    if (tp[ch] > ceilingLinear)
                        return false;
        }
        return true;
    }

    ClampTruePeakDetector verifier;
    float history[kMaxChannels][kHistory] = {};
    float from[kMaxChannels] = {};
    float scale = 1.0f;
    int   histPos = 0, length = 1, left = 0, pos = 0;

public:
    EngagementTail (const EngagementTail&) = delete;
    EngagementTail& operator= (const EngagementTail&) = delete;
};

} // namespace anabasis
