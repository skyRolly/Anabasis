#pragma once

#include "EngineParameters.h"
#include <cmath>

// ============================================================================
//  Latency — THE single source of the reported-latency arithmetic (ADR-0004).
//
//      reportedLatency = maxLookaheadSamples(10 ms, sr)   <- CONSTANT allowance
//                      + osLatency(factor, phaseMode)     <- 0 while factor Off
//
//  The lookahead contributes its MAXIMUM always: the limiter reads at a
//  variable offset inside a fixed 10 ms delay line and the engine pads the
//  difference, so presets / A/B / undo (which all carry `lookahead`) never
//  move host PDC (ADR-0004 decision item 1).
//
//  Both the engine's actual delay and the wrapper's predictLatency() call
//  these functions, so the reported figure and the real group delay cannot
//  drift apart without failing testReportedLatencyMatchesImpulse.
// ============================================================================

namespace anabasis
{

inline constexpr double kMaxLookaheadMs = 10.0;   // DEVELOPMENT_BRIEF §4.3: lookahead 0.5–10 ms
inline constexpr double kMinLookaheadMs = 0.5;    // no zero/off position (ADR-0004 / OQ-010)

inline int maxLookaheadSamples (double sampleRate) noexcept
{
    return (int) std::ceil (kMaxLookaheadMs * 0.001 * sampleRate);
}

// The oversampler's contribution, in BASE samples — a pure function of
// (factor, phaseMode), no signal-dependent term (ADR-0004 item 2), and
// INTEGERS by construction: the engine builds every juce::dsp::Oversampling
// instance with useIntegerLatency = true, whose internal fractional-delay
// compensator rounds the cascade's group delay up to a whole base sample.
//
// The values are getLatencyInSamples() MEASURED against the pinned JUCE tree
// (f8f8864…) in that mode. A JUCE bump that changes either filter design
// fails testReportedLatencyMatchesImpulse across the OS matrix — that is
// RISK-001's tripwire doing its job, not an inconvenience to suppress.
// prepare() also asserts table == getLatencyInSamples() per instance, so a
// drift is caught at the first debug run even before the matrix test.
//
// Sample-rate note: these are filter group delays in SAMPLES, which is why
// the function ignores its rate argument; the same cascade at 96 kHz delays
// the same number of samples (a shorter time — the honest physics).
inline int osLatencySamples (OversampleFactor factor, OsPhaseMode phase, double sampleRate) noexcept
{
    (void) sampleRate;
    if (factor == OversampleFactor::off)
        return 0;
    static constexpr int kMin[4] = { 4, 6, 6, 6 };       // IIR polyphase + integer-latency pad
    static constexpr int kLin[4] = { 49, 61, 65, 67 };   // FIR equiripple + integer-latency pad
    const int idx = (int) factor - 1;                    // x2..x16 → 0..3
    return phase == OsPhaseMode::minimum ? kMin[idx] : kLin[idx];
}

// The dry-ring / bypass-alignment headroom the engine must allocate for.
inline constexpr int kMaxOsLatencySamples = 67;

// The effective factor is not always the selected one: at Force Max an offline
// bounce renders at 16x, and the reported figure under isNonRealtime() uses
// the FORCED factor (ADR-0004 item 5 — this is why setNonRealtime() is a PDC
// recompute trigger).
inline OversampleFactor effectiveFactor (const EngineParameters& p) noexcept
{
    return (p.forceMaxOffline && p.nonRealtime) ? OversampleFactor::x16 : p.oversample;
}

inline int predictLatencySamples (const EngineParameters& p, double sampleRate) noexcept
{
    return maxLookaheadSamples (sampleRate)
         + osLatencySamples (effectiveFactor (p), p.osPhase, sampleRate);
}

} // namespace anabasis
