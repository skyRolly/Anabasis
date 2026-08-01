#pragma once

#include "TruePeak.h"
#include <juce_core/juce_core.h>   // the ownership guard macro (CODE_STYLE §Structure)
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
        // W+1 window entries plus one spare slot so head==tail stays "empty".
        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            wedgeValues[ch].assign ((size_t) maxWindowSamples + 2, 0.0f);
            wedgeIndices[ch].assign ((size_t) maxWindowSamples + 2, 0);
        }
        truePeak.prepare();
        aRelFast = onePoleMs (kAutoFastMs);
        aRelSlow = onePoleMs (kAutoSlowMs);
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
        {
            head[ch] = tail[ch] = 0;
            for (auto& e : envFast) if (! std::isfinite (e)) e = 1.0f;
            for (auto& e : envSlow) if (! std::isfinite (e)) e = 1.0f;
        }
        writeCount = 0;
    }

    // -- per-block settings (rates and modes, not levels — the same rule as
    //    the compressor's time constants: a boundary change cannot step the
    //    output because the envelope is the smoother) ------------------------
    void setRelease (float releaseMs) noexcept
    { aRelManual = onePoleMs (juce::jmax (1.0f, releaseMs)); }

    void setAutoRelease (bool b) noexcept        { autoRelease = b; }
    void setStyle (int s) noexcept               { style = juce::jlimit (0, 2, s); }
    void setTransientPreserve (float t) noexcept { preserve = juce::jlimit (0.0f, 1.0f, t); }
    void setStereoLink (float l) noexcept        { link = juce::jlimit (0.0f, 1.0f, l); }
    void setTruePeakMode (bool b) noexcept       { tpMode = b; }

    void setDetectorHpf (float freqHz) noexcept
    {
        // Range floor = OFF (exact skip), same semantic as the compressor's
        // detector after this change; a 2nd-order 20 Hz HPF at the floor
        // would perturb the default detector for no musical effect and cost
        // the wedge tests their exactness.
        hpfOn = freqHz > 20.001f;
        if (! hpfOn)
            return;
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

    // -- per sample: the tapped FRAME in, per-channel gains out --------------
    void processSample (const float* taps, int numChannels, int windowSamples,
                        float ceilingLinear, float* gainsOut) noexcept
    {
        const int nCh = juce::jmin (numChannels, kMaxChannels);
        if (windowSamples < 1)          windowSamples = 1;
        if (windowSamples > maxWindow)  windowSamples = maxWindow;

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
        // 1.5 ms at full — the poke-through window the clamp absorbs.
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
    float  preserve     = 0.0f, link = 1.0f;
    int    style        = 0;
    bool   autoRelease  = false, tpMode = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookaheadLimiter)
};

} // namespace anabasis
