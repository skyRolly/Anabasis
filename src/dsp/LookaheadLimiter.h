#pragma once

#include "TruePeak.h"
#include <juce_core/juce_core.h>   // the ownership guard macro (CODE_STYLE §Structure)
#include <juce_audio_basics/juce_audio_basics.h>   // SmoothedValue (the three control glides)
#include <cmath>
#include <cstdint>
#include <vector>

// ============================================================================
//  LookaheadLimiter — the §2.5 limiter: stereo-linkable lookahead peak/true-
//  peak limiter with styles, transient preservation and a two-stage release.
//
//  CONTRACT (unchanged from P1 — the engine's tap offset and the coverage
//  test pin it): the frame fed at step t must be the signal that plays W
//  steps from now, where W is the window length passed in. The returned gain
//  applies to the frame playing NOW. The sliding maximum covers W+1 fed
//  values, so the envelope attacks up to `lookahead` early, holds while the
//  peak is in-window (including the instant it plays), and releases only
//  after it has played.
//
//  Detector chain, per channel: sidechain HPF (shared `scHpfFreq`, brief §3;
//  the RANGE FLOOR (20 Hz) means NO detector filtering — an exact skip, so
//  the default detector is the sample itself, byte-for-byte) → magnitude:
//  |x|, or the ADR-0003 true-peak estimate in true-peak mode (the ceiling is
//  then dBTP-aware; the estimate runs ~5.5 samples late, inside the minimum
//  window — see TruePeak.h) → stereo link: level = link·max(all) +
//  (1−link)·own → per-channel monotonic-wedge sliding maximum → gain
//  computer.
//
//  Envelope, per channel:
//  - Attack: INSTANT (state = needed) at transientPreserve 0 — the exactness
//    the wedge-contract tests rely on. transientPreserve > 0 slews the attack
//    (τ up to ~1.5 ms), deliberately letting the front of a hit poke through
//    to the downstream clamp — that is what the control means, and why the
//    clamp is the guarantee and the limiter the shaper (ADR-0002/0006).
//  - Release: manual = one pole at `limRelease`. Auto = TWO parallel poles
//    (fast ≈ 40 ms, slow ≈ 600 ms) averaged — the §2.5 dual-stage: a
//    transient's reduction returns mostly through the fast pole, sustained
//    reduction is held by the slow one. Constants are P6 listening material;
//    the SHAPE is pinned by test (bounds disjoint for any single pole).
//  - Styles (limStyle): Transparent = neutral. Punchy = transient preserve
//    ×1.5 and release TIME ×0.8 — hits poke, recovery snappier. Loud =
//    release TIME ×0.5 — the envelope gets out of the way fastest. Style
//    modifies CONSTANTS only, never the topology, so every invariant test
//    covers all three.
// ============================================================================

namespace anabasis
{

class LookaheadLimiter
{
public:
    static constexpr int kMaxChannels = 2;

    // The macro below user-declares the copy constructor, which suppresses
    // the implicit default one.
    LookaheadLimiter() = default;

    void prepare (double sampleRate, int maxWindowSamples)
    {
        sr = sampleRate;
        maxWindow = maxWindowSamples;
        // W+1 window entries, one slot burned so head==tail stays "empty",
        // and one spare: at windowSamples == maxWindow the retained set fills
        // the structure EXACTLY (the same zero-slack coincidence the dry ring
        // had), held only by the processSample clamp. The spare turns a
        // future clamp mistake into a wrong window instead of a corrupted
        // wedge.
        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            wedgeValues[ch].assign ((size_t) maxWindowSamples + 3, 0.0f);
            wedgeIndices[ch].assign ((size_t) maxWindowSamples + 3, 0);
        }
        truePeak.prepare();
        aRelFast = onePoleMs (kAutoFastMs);
        aRelSlow = onePoleMs (kAutoSlowMs);
        linkSm.reset (sampleRate, 0.020);
        preserveSm.reset (sampleRate, 0.020);
        hpfFreqSm.reset (sampleRate, 0.020);
        reset();
    }

    // Full reset — window AND envelope. For prepare()/reset(), where the
    // signal is discontinuous anyway and unity is the correct starting gain.
    void reset() noexcept
    {
        resetWindow();
        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            envFast[ch] = envSlow[ch] = 1.0f;
            hpfZ1[ch] = hpfZ2[ch] = 0.0f;
        }
        truePeak.reset();
        primed = false;   // the next block's setters adopt without a glide
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
        for (int ch = 0; ch < kMaxChannels; ++ch)
            head[ch] = tail[ch] = 0;
        for (auto& e : envFast) if (! std::isfinite (e)) e = 1.0f;
        for (auto& e : envSlow) if (! std::isfinite (e)) e = 1.0f;
        writeCount = 0;
    }

    // Latched-factor support: alphas are rate-derived; the manual release and
    // the detector HPF are refreshed by the per-block setters that follow the
    // latch anyway. No allocation.
    void setRate (double newRate) noexcept
    {
        sr = newRate;
        aRelFast = onePoleMs (kAutoFastMs);
        aRelSlow = onePoleMs (kAutoSlowMs);
        // reset() SNAPS each smoother to its target — a latch is a silent-
        // bottom event (the engine ducks around it), so no glide is owed.
        linkSm.reset (newRate, 0.020);
        preserveSm.reset (newRate, 0.020);
        hpfFreqSm.reset (newRate, 0.020);
        recomputeHpf();
    }

    // -- per-block settings ---------------------------------------------------
    // Release/style/autoRelease are rates and modes: a boundary change cannot
    // step the output because the envelope is the smoother. Link, transient
    // preserve and the detector HPF are NOT in that class — link blends the
    // detector LEVEL directly, preserve selects the attack alpha, and the HPF
    // moves the detector spectrum — so those three glide internally (20 ms at
    // the engaged rate, invariant 8), primed on the first block like the
    // engine's own smoothers.
    void setRelease (float releaseMs) noexcept
    { aRelManual = onePoleMs (juce::jmax (1.0f, releaseMs)); }

    void setAutoRelease (bool b) noexcept        { autoRelease = b; }
    void setStyle (int s) noexcept               { style = juce::jlimit (0, 2, s); }
    void setTransientPreserve (float t) noexcept
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        if (primed) preserveSm.setTargetValue (t);
        else        preserveSm.setCurrentAndTargetValue (t);
    }
    void setStereoLink (float l) noexcept
    {
        l = juce::jlimit (0.0f, 1.0f, l);
        if (primed) linkSm.setTargetValue (l);
        else        linkSm.setCurrentAndTargetValue (l);
    }
    void setTruePeakMode (bool b) noexcept
    {
        // The estimator's 12-tap history only advances while the mode is ON
        // (processSample skips it otherwise), so re-enabling would interpolate
        // across samples that are minutes old — bogus peaks, and the gain
        // reduction they cause. The engine flips this from the OS factor, so
        // the edge is reachable in normal use; clear the history on it.
        if (b && ! tpMode)
            truePeak.reset();
        tpMode = b;
    }

    void setDetectorHpf (float freqHz) noexcept
    {
        // Smoothed like the compressor's copy of the SAME scHpfFreq value:
        // the coefficients follow the glide per sample in processSample.
        if (primed)
            hpfFreqSm.setTargetValue (freqHz);
        else
        {
            hpfFreqSm.setCurrentAndTargetValue (freqHz);
            recomputeHpf();
        }
    }

    // -- per sample: the tapped FRAME in, per-channel gains out --------------
    void processSample (const float* taps, int numChannels, int windowSamples,
                        float ceilingLinear, float* gainsOut) noexcept
    {
        primed = true;   // from here on, setter changes glide instead of adopting
        const int nCh = juce::jmin (numChannels, kMaxChannels);
        if (windowSamples < 1)          windowSamples = 1;
        if (windowSamples > maxWindow)  windowSamples = maxWindow;

        // Advance the control glides one step (the engaged rate — OS rate
        // when a factor is latched). The HPF re-derives its coefficients per
        // step while gliding, exactly as MasteringComp does with this value.
        const float link     = linkSm.getNextValue();
        const float preserve = preserveSm.getNextValue();
        if (hpfFreqSm.isSmoothing())
        {
            hpfFreqSm.getNextValue();
            recomputeHpf();
        }

        // Detector: HPF (optional) → magnitude (sample or true peak).
        float mags[kMaxChannels] = {};
        float det[kMaxChannels];
        for (int ch = 0; ch < nCh; ++ch)
        {
            float d = taps[ch];
            if (hpfOn)
            {
                const float y = hb0 * d + hpfZ1[ch];
                hpfZ1[ch] = hb1 * d - ha1 * y + hpfZ2[ch];
                hpfZ2[ch] = hb2 * d - ha2 * y;
                d = y;
            }
            det[ch] = d;
        }
        if (tpMode)
            truePeak.processFrame (det, nCh, mags);
        else
            for (int ch = 0; ch < nCh; ++ch)
                mags[ch] = std::abs (det[ch]);

        // Stereo link, then the per-channel sliding maxima.
        float maxMag = 0.0f;
        for (int ch = 0; ch < nCh; ++ch)
            maxMag = juce::jmax (maxMag, mags[ch]);

        // Style scales release TIME (Loud = half the time), which in the
        // alpha domain is a multiplier ABOVE one — the first draft multiplied
        // alpha by 0.5 and made Loud the slowest style, caught by test.
        const float relScale = style == 2 ? 2.0f : (style == 1 ? 1.25f : 1.0f);
        const float effPres  = juce::jlimit (0.0f, 1.0f,
                                             preserve * (style == 1 ? 1.5f : 1.0f));
        // preserve² keeps the control gentle at the bottom of its range;
        // 1.5 ms at full — the poke-through window the clamp absorbs. The
        // map is DELIBERATELY discontinuous at exactly 0 (instant attack vs
        // ~0.05 ms for any positive value): the wedge-contract tests rely on
        // the exactness at 0, and the smoothed `preserve` only ever LANDS on
        // 0 at the end of a glide, so the jump is between a one-sample and a
        // ~2.4-sample attack — recorded, not chased.
        const float aAtk = effPres <= 0.0f ? 1.0f
                          : onePoleMs (0.05f + 1.45f * effPres * effPres);

        for (int ch = 0; ch < nCh; ++ch)
        {
            const float level = juce::exactlyEqual (link, 1.0f)
                                    ? maxMag
                                    : link * maxMag + (1.0f - link) * mags[ch];

            // Expire entries older than the playing sample, drop dominated
            // entries, push — the retained set is [t−W, t] = W+1 entries.
            auto& wv = wedgeValues[ch];
            auto& wi = wedgeIndices[ch];
            size_t& h = head[ch];
            size_t& t = tail[ch];
            while (h != t && wi[h] < writeCount - windowSamples)
                h = next (h);
            while (h != t && wv[prev (t)] <= level)
                t = prev (t);
            wv[t] = level;
            wi[t] = writeCount;
            t = next (t);

            const float peak   = wv[h];
            const float needed = (peak > ceilingLinear && peak > 0.0f)
                                     ? ceilingLinear / peak : 1.0f;

            auto stepEnv = [&] (float& env, float aRel) noexcept
            {
                if (needed < env)
                    env = juce::exactlyEqual (aAtk, 1.0f)
                              ? needed
                              : env + (needed - env) * aAtk;
                else if (env < 1.0f)
                    env += (needed - env) * aRel * relScale;
            };
            if (autoRelease)
            {
                stepEnv (envFast[ch], aRelFast);
                stepEnv (envSlow[ch], aRelSlow);
                gainsOut[ch] = 0.5f * (envFast[ch] + envSlow[ch]);
            }
            else
            {
                stepEnv (envFast[ch], aRelManual);
                envSlow[ch] = envFast[ch];      // no stale state on a mode flip
                gainsOut[ch] = envFast[ch];
            }
        }
        ++writeCount;
    }

private:
    size_t next (size_t i) const noexcept { return i + 1 >= wedgeValues[0].size() ? 0 : i + 1; }
    size_t prev (size_t i) const noexcept { return i == 0 ? wedgeValues[0].size() - 1 : i - 1; }

    float onePoleMs (float ms) const noexcept
    { return 1.0f - std::exp (-1.0f / (float) (ms * 0.001 * sr)); }

    void recomputeHpf() noexcept
    {
        // Range floor = OFF (exact skip), same semantic as the compressor's
        // detector: a 2nd-order 20 Hz HPF at the floor would perturb the
        // default detector for no musical effect and cost the wedge tests
        // their exactness.
        const float freqHz = hpfFreqSm.getCurrentValue();
        const bool wasOn = hpfOn;
        hpfOn = freqHz > 20.001f;
        if (! hpfOn)
        {
            // Off means off: leaving the biquad's state behind would have an
            // on→off→on cycle (the adaptive scHpf trim can drive one) re-enter
            // the filter with a stale delay line and ring on the first samples.
            if (wasOn)
                for (int ch = 0; ch < kMaxChannels; ++ch)
                    hpfZ1[ch] = hpfZ2[ch] = 0.0f;
            return;
        }
        const float f    = juce::jlimit (20.0f, (float) (0.49 * sr), freqHz);
        const float w0   = juce::MathConstants<float>::twoPi * f / (float) sr;
        const float cosw = std::cos (w0);
        const float alpha = std::sin (w0) / (2.0f * 0.7071068f);
        const float inv  = 1.0f / (1.0f + alpha);
        hb0 = ((1.0f + cosw) * 0.5f) * inv;
        hb1 = (-(1.0f + cosw)) * inv;
        hb2 = hb0;
        ha1 = (-2.0f * cosw) * inv;
        ha2 = (1.0f - alpha) * inv;
    }

    static constexpr float kAutoFastMs = 40.0f, kAutoSlowMs = 600.0f;

    std::vector<float>   wedgeValues[kMaxChannels];
    std::vector<int64_t> wedgeIndices[kMaxChannels];
    size_t  head[kMaxChannels] = {}, tail[kMaxChannels] = {};
    int64_t writeCount = 0;

    TruePeakEstimator truePeak;

    // Detector HPF (normalised biquad) + per-channel state.
    float hb0 = 1.0f, hb1 = 0.0f, hb2 = 0.0f, ha1 = 0.0f, ha2 = 0.0f;
    float hpfZ1[kMaxChannels] = {}, hpfZ2[kMaxChannels] = {};
    bool  hpfOn = false;

    float envFast[kMaxChannels] = { 1.0f, 1.0f };
    float envSlow[kMaxChannels] = { 1.0f, 1.0f };

    double sr           = 48000.0;
    int    maxWindow    = 480;
    float  aRelManual   = 0.01f, aRelFast = 0.01f, aRelSlow = 0.001f;
    juce::SmoothedValue<float> linkSm { 1.0f }, preserveSm { 0.0f }, hpfFreqSm { 20.0f };
    bool   primed       = false;
    int    style        = 0;
    bool   autoRelease  = false, tpMode = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookaheadLimiter)
};

} // namespace anabasis
