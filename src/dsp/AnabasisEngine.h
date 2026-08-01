#pragma once

#include "EngineParameters.h"
#include "Latency.h"
#include "MasteringEQ.h"
#include "MasteringComp.h"
#include "ClipSat.h"
#include "LookaheadLimiter.h"
#include "CeilingClamp.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

// ============================================================================
//  AnabasisEngine — chain owner (ADR-0001: format-agnostic, sees only the
//  EngineParameters POD; never includes a plugin-client or GUI header).
//
//  P1 chain:  Input Gain → [EQ/Comp/Clip: pass-through] → LookaheadLimiter
//             → CeilingClamp → [Dither: Off] → out
//
//  Latency contract (ADR-0004): the audio path is delayed by the FULL 10 ms
//  lookahead allowance at every setting — the engaged `lookahead` value moves
//  only the gain computer's window, so a preset/A-B/undo bulk swap can never
//  change reported latency. groupDelaySamples() and the wrapper's
//  predictLatency() share Latency.h, so they cannot disagree silently.
//
//  Bypass is a delay-aligned dry path with a bit-exact-at-the-endpoints
//  crossfade (§2.8's always-running-crossfade mechanism, minimal P1 form).
//
//  Invariant 9: non-finite samples are replaced with silence at the ring
//  write and at the output, and a block that saw any resets the limiter
//  envelope — one bad buffer cannot poison the gain state.
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

    // Audio thread. Adopts the per-block POD snapshot (ADR-0011).
    void process (juce::AudioBuffer<float>& buffer, const EngineParameters& params) noexcept;

    int groupDelaySamples() const noexcept { return delaySamples; }

    // The engaged lookahead window in samples, as last handed to the
    // detector. Exposed because it is where invariant 8's "smooth,
    // band-limited" requirement for a lookahead move is observable — the
    // output is not, since the wedge and the attack/release asymmetry
    // absorb a tap step.
    int engagedWindowSamples() const noexcept
    { return engagedWindow.load (std::memory_order_relaxed); }

private:
    static constexpr int kMaxChannels = 2;

    double sr           = 48000.0;
    int    delaySamples = 480;        // maxLookaheadSamples(sr), set in prepare()
    int    ringSize     = 0;
    int    writePos     = 0;

    // Fixed 10 ms lines, sized in prepare() (REALTIME_AUDIO_POLICY rule 1).
    // wet: post-input-gain signal the limiter path reads; dry: raw input for
    // the delay-aligned bypass.
    juce::AudioBuffer<float> wetRing, dryRing;

    // CODE_STYLE §Real-time discipline: every parameter that reaches the DSP
    // is smoothed. `ceiling` is host-automatable and `lookahead` is named by
    // DSP_POLICY invariant 8 as the switchable path most likely to be skipped
    // at P1 — its move must be "a smooth, band-limited control signal", which
    // for the detector tap means gliding the offset, not stepping it.
    juce::SmoothedValue<float> inputGain      { 1.0f };   // zipper-noise rule
    juce::SmoothedValue<float> pushGain       { 1.0f };   // limGain (the macro's primary target)
    juce::SmoothedValue<float> ceilingLinear  { 0.8912509f };  // -1 dBTP default
    juce::SmoothedValue<float> windowSamples  { 96.0f };        // engaged lookahead, in samples
    bool smoothersPrimed = false;   // first block after prepare/reset snaps instead of gliding
    // Written every sample on the audio thread and readable from anywhere:
    // THREADING_POLICY requires cross-thread publication to go through an
    // atomic, and relaxed is the right ordering for a monotonic display/
    // diagnostic value that carries no payload (same rule as the meters).
    std::atomic<int> engagedWindow { 96 };

    LookaheadLimiter limiter;
    CeilingClamp     clamp;
    MasteringEQ      eq;
    MasteringComp    comp;
    ClipSat          clip;
    int eqPositionNow = 0;     // 0 Pre, 1 Post — the position the biquad state belongs to

    // §2.8 minimal form: linear output crossfade between wet and dry over
    // ~10 ms, with exact-endpoint branches so both null tests are bit-exact.
    float bypassMix     = 0.0f;       // 0 = wet, 1 = dry
    float bypassStep    = 0.0f;
    bool  bypassTarget  = false;

    // CODE_STYLE §Structure: owning classes carry the guard. This one owns two
    // heap ring buffers and the limiter's wedge; an accidental copy would
    // duplicate them silently instead of failing to compile.
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnabasisEngine)
};

} // namespace anabasis
