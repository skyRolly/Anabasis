#pragma once

#include "EngineParameters.h"
#include "Latency.h"
#include "MasteringEQ.h"
#include "MasteringComp.h"
#include "ClipSat.h"
#include "LookaheadLimiter.h"
#include "CeilingClamp.h"
#include "LoudnessMeter.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <memory>

// ============================================================================
//  AnabasisEngine — chain owner (ADR-0001: format-agnostic, sees only the
//  EngineParameters POD; never includes a plugin-client or GUI header).
//
//  P2 chain, per DSP_POLICY invariant 1 / ADR-0002/0003:
//
//    base rate:  Input Gain → EQ(Pre) → Compressor
//    OS region:  [up ×N] → Clipper/Sat → limiter push → 10 ms lookahead line
//                → LookaheadLimiter → [down ×N]
//    base rate:  EQ(Post) → CeilingClamp → Dither → bypass crossfade → out
//
//  Latency contract (ADR-0004): the audio path is delayed by the FULL 10 ms
//  lookahead allowance at every setting (the lookahead line runs INSIDE the
//  region at N× rate, delaying delaySamples·N OS samples = exactly 10 ms of
//  base samples) plus the oversampler's integer group delay from Latency.h's
//  measured table. groupDelaySamples() stays the base allowance; the wrapper
//  adds the OS term through the same predictLatencySamples() the engine's
//  dry-ring alignment uses, so reported and actual cannot drift silently.
//
//  Oversampling (ADR-0003/0011): every factor × phase instance is constructed
//  and initProcessing'd at prepare(); a runtime factor/phase change LATCHES at
//  a block boundary AT THE §2.8 DUCK'S SILENT BOTTOM — it selects among
//  existing objects, allocates nothing, and resets the region state while the
//  output gain is zero. useIntegerLatency keeps every configuration's group
//  delay a whole base sample, which is what lets the bypass stay a bit-exact
//  integer-delay null.
//
//  §2.8 transition layer: asymmetric raised-cosine duck (~6 ms out / ~28 ms
//  in) for every discrete rewire — eqPosition, colourModel, OS factor/phase,
//  and wrapper-requested bulk swaps (requestForcedDuck before A/B, preset,
//  session load). Engine rewires execute only at the silent bottom; wrapper
//  swaps land as smoothed parameter glides under the duck's envelope.
//
//  Bypass is a delay-aligned dry path (base-rate ring, offset = allowance +
//  osLatency) with a bit-exact-at-the-endpoints crossfade.
//
//  Invariant 9: non-finite samples are replaced with silence at the staging
//  and output boundaries, and a block that saw any discards the limiter's
//  sliding window (envelope carried — see LookaheadLimiter::resetWindow).
// ============================================================================

namespace anabasis
{

class AnabasisEngine
{
public:
    // Explicit because the non-copyable guard below is a user-declared
    // constructor, which suppresses the implicit default one.
    AnabasisEngine() = default;

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset() noexcept;

    // Audio thread. Adopts the per-block POD snapshot (ADR-0011). Blocks
    // larger than the prepared maximum are processed in prepared-size chunks,
    // so a host that violates its own declared maximum degrades to extra
    // chunk overhead instead of unprocessed audio.
    void process (juce::AudioBuffer<float>& buffer, const EngineParameters& params) noexcept;

    int groupDelaySamples() const noexcept { return delaySamples; }

    // §2.8: the forced-duck request — the THREADING_POLICY momentary-request
    // row (payload-free single atomic, exchange-consumed at the block top).
    // The wrapper calls this BEFORE every bulk swap (A/B, preset, session
    // load); the engine also self-requests for its own discrete rewires
    // (eqPosition, colourModel, OS factor/phase), which additionally apply
    // ONLY at the silent bottom.
    void requestForcedDuck() noexcept { duckRequested.store (true, std::memory_order_relaxed); }

    // The engaged lookahead window in BASE samples, as last handed to the
    // detector. Exposed because it is where invariant 8's "smooth,
    // band-limited" requirement for a lookahead move is observable — the
    // output is not, since the wedge and the attack/release asymmetry
    // absorb a tap step.
    int engagedWindowSamples() const noexcept
    { return engagedWindow.load (std::memory_order_relaxed); }

    // The block's deepest limiter gain (linear, ≤ 1) — the §2.9 GR meter tap.
    // Same publication class as engagedWindow: relaxed atomic, monotonic
    // display data, written once per process() call on the audio thread.
    float lastBlockMinGain() const noexcept
    { return grMinLinear.load (std::memory_order_relaxed); }

private:
    void latchOsConfig (int factorIdx, int phaseIdx) noexcept;
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int num,
                       const EngineParameters& p, bool eqPre, bool eqPost) noexcept;

    static constexpr int kMaxChannels = 2;
    static constexpr int kMaxOsFactorLog2 = 4;   // 16×

    double sr           = 48000.0;
    int    delaySamples = 480;        // maxLookaheadSamples(sr), set in prepare()
    int    maxBlock     = 512;
    int    numChans     = 2;

    // OS-rate lookahead line (wet) — allocated for 16× — and the base-rate
    // dry line for the bypass, with kMaxOsLatencySamples of extra depth.
    juce::AudioBuffer<float> wetRing, dryRing, staging;
    int ringSizeOs   = 0;             // logical size for the CURRENT factor
    int writePosOs   = 0;
    int delayOs      = 480;           // delaySamples · osN
    int dryRingSize  = 0;
    int dryWritePos  = 0;

    // Per-base-sample control values, filled in stage A and indexed by the
    // region at OS rate (i >> osShift): the same instantaneous ceiling the
    // gain computer uses reaches the clamp, exactly as before.
    std::vector<float> ceilArr;
    std::vector<int>   wArr;

    juce::SmoothedValue<float> inputGain      { 1.0f };
    juce::SmoothedValue<float> pushGain       { 1.0f };
    juce::SmoothedValue<float> ceilingLinear  { 0.8912509f };  // -1 dBTP default
    juce::SmoothedValue<float> windowSamples  { 96.0f };        // engaged lookahead, BASE samples
    bool smoothersPrimed = false;
    std::atomic<int> engagedWindow { 96 };
    std::atomic<float> grMinLinear { 1.0f };
    float grMinThisCall = 1.0f;

    LookaheadLimiter limiter;
    CeilingClamp     clamp;
    MasteringEQ      eq;
    MasteringComp    comp;
    ClipSat          clip;

    // §2.8 transition ducker: asymmetric raised cosine, ~6 ms out / ~28 ms
    // in. Gain advances per base sample in stage E and multiplies the
    // PROCESSED path only (bypass stays a bit-exact null). Engine-side
    // rewires are held in the applied* fields until the bottom; the POD the
    // stages see carries the APPLIED values, so nothing rewires at full gain.
    enum class DuckState { idle, out, bottom, in };
    std::atomic<bool> duckRequested { false };
    DuckState duckState = DuckState::idle;
    float duckGain = 1.0f, duckPhase = 0.0f;
    float duckOutInc = 0.0f, duckInInc = 0.0f;
    int   appliedEqPos = 0, appliedModel = 1;   // == the POD defaults

    // Oversampling: [factorLog2 − 1][phase] — all eight built at prepare().
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplers[kMaxOsFactorLog2][2];
    juce::dsp::Oversampling<float>* osActive = nullptr;   // null = Off
    int latchedFactorIdx = -1;        // -1 Off, 0..3 = 2×..16×
    int latchedPhaseIdx  = 0;
    int osN = 1, osShift = 0;
    int osLatBase = 0;                // Latency.h table value for the latched config

    // §2.7 loudness-compensated monitoring + delta. MONITOR-ONLY functions
    // (DSP_POLICY invariant 10): both are inert whenever nonRealtime is set,
    // so the render is untouched — that is the tested contract, not a hope.
    // Measure: K-weighted short-term loudness of the delay-aligned dry vs the
    // processed path (two always-fed LoudnessMeters; the measure FREEZES when
    // either side's momentary drops under the BS.1770 −70 LUFS absolute gate,
    // chosen over a dBFS gate because a mastering plugin meets quiet
    // classical passages). Predict: stateless floor from the deterministic
    // gain lift (inputGain + limGain + average measured GR), only ever
    // LOWERING monitor gain — cranking the macro pre-ducks instantly, no
    // ratchet. Applied = min(measure, predict), smoothed 200 ms, POST-mix so
    // the bypass leg carries the same compensation (the §2.7 loudness-matched
    // bypass). Delta = (delay-aligned dry − processed) behind its own
    // always-running ~10 ms crossfade.
    LoudnessMeter dryMeter, wetMeter;
    float compMeasureDb = 0.0f;              // frozen on silence
    juce::SmoothedValue<float> monitorGain { 1.0f };
    float deltaMix = 0.0f, deltaStep = 0.0f;
    bool  deltaTarget = false;

    // Dither (§4.5): TPDF at the target LSB, optional first-order noise
    // shaping, deterministic xorshift so an offline render is repeatable.
    uint32_t rngState = 0x9E3779B9u;
    float    ditherErr[kMaxChannels] = {};

    // §2.8 minimal form: linear output crossfade between wet and dry over
    // ~10 ms, with exact-endpoint branches so both null tests are bit-exact.
    float bypassMix     = 0.0f;       // 0 = wet, 1 = dry
    float bypassStep    = 0.0f;
    bool  bypassTarget  = false;

    // CODE_STYLE §Structure: owning classes carry the guard. This one owns the
    // rings, the staging buffer and eight oversampler instances; an accidental
    // copy would duplicate them silently instead of failing to compile.
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnabasisEngine)
};

} // namespace anabasis
