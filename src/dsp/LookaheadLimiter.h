#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

// ============================================================================
//  LookaheadLimiter — P1 "basic limiter" (DESIGN §11): a stereo-linked peak
//  limiter whose gain computer sees `lookahead` ms ahead of the audio tap.
//
//  CONTRACT (this is what the engine's tap offset and the coverage test pin):
//  the value fed at step t must be the signal sample that plays W steps from
//  now, where W = windowLengthSamples(). The returned gain applies to the
//  sample playing NOW. The sliding maximum therefore covers W+1 fed values —
//  the playing sample and the W upcoming ones — so the envelope attacks
//  exactly `lookahead` early, holds while the peak is still in the window
//  (including the instant it plays), and releases only after it has played.
//  An earlier revision fed the just-written input (10 ms early regardless of
//  the engaged lookahead) and expired the playing sample one step early; both
//  let peaks reach the clamp under-attenuated.
//
//  Implementation: monotonic-wedge sliding maximum, O(1) amortised,
//  allocation-free after prepare(). P2+ replaces this with the full styles /
//  transient-preserve / true-peak design (DESIGN §2.5) under the same contract:
//  unity gain for material below the ceiling (inv 7's null), never above it
//  after the clamp (inv 4).
// ============================================================================

namespace anabasis
{

class LookaheadLimiter
{
public:
    void prepare (double sampleRate, int maxWindowSamples)
    {
        sr = sampleRate;
        maxWindow = maxWindowSamples;
        // W+1 window entries plus one spare slot so head==tail stays "empty".
        wedgeValues.assign ((size_t) maxWindowSamples + 2, 0.0f);
        wedgeIndices.assign ((size_t) maxWindowSamples + 2, 0);
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
        if (windowSamples > maxWindow)
            windowSamples = maxWindow;      // sized for the 10 ms allowance in prepare()
        // One-pole release toward gain 1. Time constant from the limRelease
        // parameter (§4.2 row 28); P1 ignores Auto and the style switch.
        releaseAlpha = 1.0f - std::exp (-1.0f / (float) (releaseMs * 0.001 * sr));
    }

    // The engine's detector-tap offset: delaySamples - windowLengthSamples()
    // is how far behind the write head the fed sample must be read so that
    // fed[t] is exactly the sample playing W steps from now.
    int windowLengthSamples() const noexcept { return windowSamples; }

    float processSample (float stereoMaxMagnitude) noexcept
    {
        // Expire entries older than the playing sample: keep indices
        // >= writeCount - windowSamples, so after the push below the window
        // holds W+1 entries — the playing sample through the newest fed one.
        while (head != tail && wedgeIndices[head] < writeCount - windowSamples)
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
    int    maxWindow     = 480;
    int    windowSamples = 96;
    float  ceilingLinear = 0.8912509f;
    float  releaseAlpha  = 0.01f;
    float  envelope      = 1.0f;
};

} // namespace anabasis
