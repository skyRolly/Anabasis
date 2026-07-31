#pragma once

#include "EngineParameters.h"
#include "Latency.h"
#include "LookaheadLimiter.h"
#include "CeilingClamp.h"
#include <juce_audio_basics/juce_audio_basics.h>

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
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset() noexcept;

    // Audio thread. Adopts the per-block POD snapshot (ADR-0011).
    void process (juce::AudioBuffer<float>& buffer, const EngineParameters& params) noexcept;

    int groupDelaySamples() const noexcept { return delaySamples; }

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

    juce::SmoothedValue<float> inputGain { 1.0f };   // zipper-noise rule (CODE_STYLE)
    juce::SmoothedValue<float> pushGain  { 1.0f };   // limGain (the macro's primary target)

    LookaheadLimiter limiter;
    CeilingClamp     clamp;

    // §2.8 minimal form: linear output crossfade between wet and dry over
    // ~10 ms, with exact-endpoint branches so both null tests are bit-exact.
    float bypassMix     = 0.0f;       // 0 = wet, 1 = dry
    float bypassStep    = 0.0f;
    bool  bypassTarget  = false;
};

} // namespace anabasis
