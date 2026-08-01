#pragma once

#include <cmath>

// ============================================================================
//  CeilingClamp — the final safety stage (ADR-0002 / ADR-0006, DSP_POLICY
//  invariant 4). ALWAYS the last stage before dither, in both EQ positions;
//  structurally separate from the limiter, never folded into it; base rate,
//  outside the oversampled region (it must sit downstream of a Post-position
//  EQ, which is itself outside that region).
//
//  P1: sample-level hard clamp at the linear ceiling. The true-peak-driven
//  gain half of ADR-0006 item 3 arrives with the TruePeak tap at P2; ADR-0006
//  itself records that the backstop alone cannot meet a dBTP tolerance — the
//  guarantee is claimed only for the estimate + backstop combination.
// ============================================================================

namespace anabasis
{

class CeilingClamp
{
public:
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
};

} // namespace anabasis
