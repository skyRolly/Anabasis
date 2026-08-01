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

// P1: no oversampling region exists yet, so every factor's oversampler delay
// is genuinely zero. When the P2 oversampling lands (ADR-0003), this becomes a
// pure function of (factor, phaseMode) — per-factor IIR/FIR group delays — and
// MUST stay free of any signal-dependent term (ADR-0004 item 2).
inline int osLatencySamples (OversampleFactor factor, OsPhaseMode phase, double sampleRate) noexcept
{
    (void) factor; (void) phase; (void) sampleRate;
    return 0;
}

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
