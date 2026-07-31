#pragma once

#include <cmath>
#include <vector>

// ============================================================================
//  LookaheadLimiter — P1 "basic limiter" (DESIGN §11): a stereo-linked peak
//  limiter whose gain computer sees `lookahead` ms ahead of the audio tap.
//
//  The AUDIO delay is owned by AnabasisEngine's fixed 10 ms line and never
//  moves (ADR-0004: constant allowance). This class only computes the gain
//  envelope: a sliding-window maximum over the engaged lookahead window
//  (monotonic-wedge, O(1) amortised, allocation-free after prepare), instant
//  attack toward the required gain, exponential release.
//
//  P2+ replaces this with the full styles/transient-preserve/true-peak design
//  (DESIGN §2.5); the CONTRACT it must keep is exactly this one: unity gain
//  for material below the ceiling (inv 7's null), never above it after the
//  clamp (inv 4).
// ============================================================================

namespace anabasis
{

class LookaheadLimiter
{
public:
    void prepare (double sampleRate, int maxWindowSamples)
    {
        sr = sampleRate;
        wedgeValues.assign ((size_t) maxWindowSamples + 1, 0.0f);
        wedgeIndices.assign ((size_t) maxWindowSamples + 1, 0);
        reset();
    }

    void reset() noexcept
    {
        head = tail = 0;
        writeCount = 0;
        envelope = 1.0f;
    }

    void setPerBlock (float ceilingLinearIn, float lookaheadMs, float releaseMs) noexcept
    {
        ceilingLinear = ceilingLinearIn;
        windowSamples = (int) std::ceil (lookaheadMs * 0.001 * sr);
        if (windowSamples < 1)
            windowSamples = 1;
        if ((size_t) windowSamples + 1 > wedgeValues.size())
            windowSamples = (int) wedgeValues.size() - 1;   // sized for 10 ms max in prepare()
        // One-pole release toward gain 1. Time constant from the limRelease
        // parameter (§4.2 row 28); P1 ignores Auto and the style switch.
        releaseAlpha = 1.0f - std::exp (-1.0f / (float) (releaseMs * 0.001 * sr));
    }

    // Feed the stereo-max magnitude of the sample ENTERING the delay line;
    // returns the gain for the sample LEAVING it. The wedge front is the
    // maximum over the most recent `windowSamples` inputs — i.e. the audio
    // the output tap is about to see, which is what makes the attack
    // pre-emptive rather than reactive.
    float processSample (float stereoMaxMagnitude) noexcept
    {
        // Drop expired entries from the front.
        while (head != tail && wedgeIndices[head] <= writeCount - windowSamples)
            head = next (head);
        // Drop dominated entries from the back, then push.
        while (head != tail && wedgeValues[prev (tail)] <= stereoMaxMagnitude)
            tail = prev (tail);
        wedgeValues[tail]  = stereoMaxMagnitude;
        wedgeIndices[tail] = writeCount;
        tail = next (tail);
        ++writeCount;

        const float peak   = wedgeValues[head];
        const float needed = (peak > ceilingLinear && peak > 0.0f) ? ceilingLinear / peak : 1.0f;

        if (needed < envelope)
            envelope = needed;                                    // instant attack (pre-emptive)
        else if (envelope < 1.0f)
            envelope += (needed - envelope) * releaseAlpha;       // exponential release

        return envelope;
    }

private:
    size_t next (size_t i) const noexcept { return i + 1 >= wedgeValues.size() ? 0 : i + 1; }
    size_t prev (size_t i) const noexcept { return i == 0 ? wedgeValues.size() - 1 : i - 1; }

    std::vector<float>   wedgeValues;
    std::vector<int64_t> wedgeIndices;
    size_t  head = 0, tail = 0;
    int64_t writeCount = 0;

    double sr            = 48000.0;
    int    windowSamples = 96;
    float  ceilingLinear = 0.8912509f;
    float  releaseAlpha  = 0.01f;
    float  envelope      = 1.0f;
};

} // namespace anabasis
