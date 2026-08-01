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
//  now, where W is the window length passed in. The returned gain applies to the
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

    // Full reset — window AND envelope. For prepare()/reset(), where the
    // signal is discontinuous anyway and unity is the correct starting gain.
    void reset() noexcept
    {
        resetWindow();
        envelope = 1.0f;
    }

    // Window-only reset, for the engine's invariant-9 self-heal MID-STREAM.
    // Snapping the envelope back to unity there is a separate effect from
    // emptying the window and a worse one: a block that was 10 dB gain-reduced
    // would jump to unity with no release ramp, and the ceiling clamp turns
    // that into hard clipping rather than a recovery — breaking the invariant-8
    // click-free claim on the one path that is supposed to be a graceful
    // degradation. The envelope is carried across instead, and only sanitised:
    // it is provably finite today (the engine sanitises every ring write, so
    // the detector never feeds a non-finite magnitude), but the self-heal is
    // defence in depth and a guard that trusts its own reachability argument
    // is not one.
    void resetWindow() noexcept
    {
        head = tail = 0;
        writeCount = 0;
        if (! std::isfinite (envelope))
            envelope = 1.0f;
    }

    // Release is the only genuinely per-block input: it sets a time constant,
    // not a level, so a block-boundary change cannot step the output.
    void setRelease (float releaseMs) noexcept
    {
        releaseAlpha = 1.0f - std::exp (-1.0f / (float) (releaseMs * 0.001 * sr));
    }

    // Window length and ceiling arrive PER SAMPLE: both are level-affecting
    // controls, so CODE_STYLE's "every parameter that reaches the DSP is
    // smoothed" and DSP_POLICY invariant 8 (which names the lookahead as the
    // switchable path most likely to be skipped at P1, and requires its move
    // to be "a smooth, band-limited control signal") apply to them. The engine
    // owns the smoothers and hands the instantaneous values down; taking them
    // per block instead let a ceiling drag step the gain and let a lookahead
    // move jump the detector tap by hundreds of samples in one boundary.
    float processSample (float stereoMaxMagnitude, int windowSamples, float ceilingLinear) noexcept
    {
        if (windowSamples < 1)          windowSamples = 1;
        if (windowSamples > maxWindow)  windowSamples = maxWindow;

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

    // CODE_STYLE §Structure requires the guard on owning classes — this one
    // owns the two wedge vectors, and a copy would heap-allocate on a class
    // that sits on the audio path. Spelled with `= delete` rather than
    // JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR deliberately: the leaf DSP
    // headers (this one, CeilingClamp, Latency, EngineParameters) are JUCE-free
    // by construction and only AnabasisEngine — which carries the full macro —
    // pulls juce_audio_basics. The copy diagnostic is identical; the leak
    // detector's half is inert for a class that is only ever a value member.
    LookaheadLimiter (const LookaheadLimiter&)            = delete;
    LookaheadLimiter& operator= (const LookaheadLimiter&) = delete;
    LookaheadLimiter()                                    = default;

private:
    size_t next (size_t i) const noexcept { return i + 1 >= wedgeValues.size() ? 0 : i + 1; }
    size_t prev (size_t i) const noexcept { return i == 0 ? wedgeValues.size() - 1 : i - 1; }

    std::vector<float>   wedgeValues;
    std::vector<int64_t> wedgeIndices;
    size_t  head = 0, tail = 0;
    int64_t writeCount = 0;

    double sr           = 48000.0;
    int    maxWindow    = 480;
    float  releaseAlpha = 0.01f;
    float  envelope     = 1.0f;
};

} // namespace anabasis
