#pragma once

#include "EngineParameters.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

// ============================================================================
//  ClipSat — the §2.4 clipper/saturation (colour) stage.
//
//  Four sub-blocks in a fixed order (one chain stage from the outside, per
//  DSP_POLICY invariant 1):
//
//  1. CLIP — continuously variable hard↔soft KNEE morph with first-order
//     ADAA. The curve family (odd-symmetric, |input| a, knee width w =
//     `clipShape`, hard edge normalised to 1):
//         a ≤ 1−w        f = a                      (exact linear region)
//         1−w < a < 1+w  f = a − (a−(1−w))²/(4w)    (C1 quadratic knee)
//         a ≥ 1+w        f = 1                      (saturated)
//     w = 0 is a pure hard clip, w = 1 curves from zero upward — the brief's
//     hard↔soft morph, with a closed-form antiderivative for ADAA (the
//     transfer-curve view reads THESE coefficients — one source of truth).
//     Drive is level-compensated: y = f(x·g)/g, so the linear-region body
//     passes at unity gain and driving harder shaves peaks (crest reduction)
//     instead of getting louder — the loudness lives in `limGain`, which the
//     macro raises alongside. Drive at EXACTLY 0 dB skips the sub-block
//     entirely: the bit-identity contract (Anamorph driveTanh precedent).
//
//  2. COLOUR — the model's harmonic residue, scaled by `colourDepth`:
//     out = c + depth·r where r is built from odd (c³, Transistor ⅗c³+⅖c⁵)
//     and even (c²) components weighted by the model and by `colourBalance`
//     (−1 all-even … +1 all-odd), tilted by `colourTone` (one-pole 2 kHz
//     split: −1 dark … +1 bright), then DC-blocked (c² makes DC; a ~5 Hz
//     one-pole removes it). depth == 0 contributes NOTHING — exact identity
//     regardless of model — and Clean's weights are {0, 0}, so Clean is the
//     null model at every depth (DESIGN §2.4). Model weights are named
//     constants: P6 listening-test material, shape pinned by test.
//
//  3. DYNAMIC TAME (`dynTilt`, 0–2 dB) — the program-dependent one-band high
//     shelf DESIGN §2.2 places INSIDE this stage. Program-dependence is
//     mechanical and deterministic in the input (the §5.4 determinism rule):
//     the detector is the clipper's own activity — per-sample clip depth
//     (|u|−|f(u)|)/|u|, max across channels, through a fast-rise/slow-fall
//     envelope — so the shelf cuts up to `dynTilt` dB at ~6 kHz exactly when
//     clipping is generating harshness, and is EXACTLY idle (skipped) when
//     nothing clips or dynTilt is 0. First-order shelf: y = x + (g−1)·hp(x),
//     no coefficient table to rebuild per sample.
//
//  4. MIX — the stage's parallel dry/wet blend (`clipMix`), exact endpoints.
//
//  Sits inside the oversampled region once §3's oversampler lands (ADR-0003);
//  at OS Off it runs at base rate, which is that configuration's contract.
// ============================================================================

namespace anabasis
{

class ClipSat
{
public:
    static constexpr int kMaxChannels = 2;

    ClipSat() = default;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto* s : smoothers())
            s->reset (sampleRate, 0.020);
        aToneLp   = onePole (2000.0f);
        aTameLp   = onePole (6000.0f);
        aDcBlock  = onePole (5.0f);
        aActFast  = onePoleMs (5.0f);
        aActSlow  = onePoleMs (150.0f);
        reset();
    }

    // Latched-factor support: the region runs at sr·N and every one-pole here
    // is rate-derived. Plain float recomputes, no allocation — legal at the
    // latch boundary on the audio thread. Smoother ramps snap to their
    // targets (SmoothedValue::reset does), which is what a latch/reset
    // boundary means.
    void setRate (double newRate) noexcept
    {
        sr = newRate;
        for (auto* s : smoothers())
            s->reset (newRate, 0.020);
        aToneLp  = onePole (2000.0f);
        aTameLp  = onePole (6000.0f);
        aDcBlock = onePole (5.0f);
        aActFast = onePoleMs (5.0f);
        aActSlow = onePoleMs (150.0f);
    }

    void reset() noexcept
    {
        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            adaaPrev[ch] = 0.0f;
            toneLp[ch] = dcState[ch] = tameLp[ch] = 0.0f;
        }
        activityEnv = 0.0f;
        primed = false;
    }

    void setPerBlock (const EngineParameters& p) noexcept
    {
        auto set = [this] (juce::SmoothedValue<float>& s, float v) noexcept
        {
            if (primed) s.setTargetValue (v);
            else        s.setCurrentAndTargetValue (v);
        };
        set (driveDb, juce::jlimit (0.0f, 24.0f, p.clipDriveDb));
        set (shape,   juce::jlimit (0.0f, 1.0f,  p.clipShape));
        set (depth,   juce::jlimit (0.0f, 1.0f,  p.colourDepth));
        set (balance, juce::jlimit (-1.0f, 1.0f, p.colourBalance));
        set (tone,    juce::jlimit (-1.0f, 1.0f, p.colourTone));
        set (tameDb,  juce::jlimit (0.0f, 2.0f,  p.dynTiltDb));
        set (mix,     juce::jlimit (0.0f, 1.0f,  p.clipMix));

        // colourModel is a discrete rewire (duck-routed once §2.8 lands —
        // ADR-0010's same-day note names it with eqPosition).
        model  = juce::jlimit (0, 3, p.colourModel);
        primed = true;
    }

    // One call per sample with all channel samples, in place.
    void processSample (float* chans, int numChannels) noexcept
    {
        const int nCh = juce::jmin (numChannels, kMaxChannels);

        const float dDb = driveDb.getNextValue();
        const float w   = shape.getNextValue();
        const float dep = depth.getNextValue();
        const float bal = balance.getNextValue();
        const float ton = tone.getNextValue();
        const float tam = tameDb.getNextValue();
        const float M   = mix.getNextValue();

        const bool clipOn = ! juce::exactlyEqual (dDb, 0.0f);
        const float g     = clipOn ? std::pow (10.0f, dDb * (1.0f / 20.0f)) : 1.0f;
        const float invG  = 1.0f / g;

        float wet[kMaxChannels];
        float activityRaw = 0.0f;

        for (int ch = 0; ch < nCh; ++ch)
        {
            const float dry = chans[ch];
            float c = dry;

            // -- 1. drive → ADAA clip → compensate ---------------------------
            if (clipOn)
            {
                const float u  = dry * g;
                const float u1 = adaaPrev[ch];
                adaaPrev[ch] = u;
                const float du = u - u1;
                float shaped;
                if (std::abs (du) > 1.0e-4f)
                    shaped = (antiderivative (u, w) - antiderivative (u1, w)) / du;
                else
                    shaped = transfer (0.5f * (u + u1), w);
                c = shaped * invG;

                const float au = std::abs (u);
                if (au > 1.0e-9f)
                    activityRaw = juce::jmax (activityRaw,
                                              (au - std::abs (transfer (u, w))) / au);
            }
            else
            {
                // The ADAA memory must track the signal even while skipped, or
                // the first driven sample differences against a stale value.
                adaaPrev[ch] = dry;
            }

            // -- 2. colour ---------------------------------------------------
            // Deliberately NOT kept warm on the skipped branch, unlike the
            // ADAA memory above and the tame filter below: those two filter
            // the SIGNAL, so a stale state differences or splices against
            // real audio. `toneLp`/`dcState` filter the colour RESIDUE `r`,
            // which is added as `dep * r` — and `dep` reaches nonzero only
            // through its own 20 ms smoother, so any stale-state transient is
            // O(dep) at the moment it could be heard, i.e. zero. Warming them
            // would mean computing the whole colour polynomial on every
            // skipped sample to feed filters whose output is multiplied by 0.
            if (dep > 0.0f && model != 0)
            {
                const float oddPart  = model == 3 ? 0.6f * c * c * c + 0.4f * c * c * c * c * c
                                                  : c * c * c;
                const float evenPart = c * c;
                float r = (1.0f + bal) * kModelOdd[model]  * oddPart
                        + (1.0f - bal) * kModelEven[model] * evenPart;

                toneLp[ch] += (r - toneLp[ch]) * aToneLp;             // 2 kHz split
                r = (1.0f - ton) * toneLp[ch] + (1.0f + ton) * (r - toneLp[ch]);

                dcState[ch] += (r - dcState[ch]) * aDcBlock;          // DC out of c²
                r -= dcState[ch];

                c += dep * r;
            }

            wet[ch] = c;
        }

        // -- 3. dynamic tame (one shared gain — the image stays put) ---------
        activityEnv += (activityRaw - activityEnv)
                     * (activityRaw > activityEnv ? aActFast : aActSlow);
        const float tameGainDb = -tam * juce::jlimit (0.0f, 1.0f, activityEnv);
        if (tameGainDb < -0.01f)
        {
            const float gLin = std::pow (10.0f, tameGainDb * (1.0f / 20.0f));
            for (int ch = 0; ch < nCh; ++ch)
            {
                tameLp[ch] += (wet[ch] - tameLp[ch]) * aTameLp;       // 6 kHz split
                wet[ch] += (gLin - 1.0f) * (wet[ch] - tameLp[ch]);
            }
        }
        else
        {
            for (int ch = 0; ch < nCh; ++ch)
                tameLp[ch] += (wet[ch] - tameLp[ch]) * aTameLp;       // keep state warm
        }

        // -- 4. mix, exact endpoints. At full wet the assignment is the wet
        // value itself, and when every sub-block was skipped wet IS the input
        // float — the bit-exact identity path. At 0 the dry sample is never
        // touched.
        for (int ch = 0; ch < nCh; ++ch)
        {
            if (juce::exactlyEqual (M, 1.0f))
                chans[ch] = wet[ch];
            else if (! juce::exactlyEqual (M, 0.0f))
                chans[ch] = chans[ch] + (wet[ch] - chans[ch]) * M;
        }
    }

    // The §2.4 one-source-of-truth hook: the curve view renders from the same
    // transfer the DSP applies.
    static float transfer (float x, float w) noexcept
    {
        const float a = std::abs (x);
        const float s = x < 0.0f ? -1.0f : 1.0f;
        if (w < 1.0e-4f)
            return a >= 1.0f ? s : x;
        if (a <= 1.0f - w)
            return x;
        if (a >= 1.0f + w)
            return s;
        const float t = a - (1.0f - w);
        return s * (a - t * t / (4.0f * w));
    }

private:
    static float antiderivative (float x, float w) noexcept
    {
        const float a = std::abs (x);
        if (w < 1.0e-4f)
            return a <= 1.0f ? 0.5f * a * a : a - 0.5f;
        if (a <= 1.0f - w)
            return 0.5f * a * a;
        if (a >= 1.0f + w)
        {
            const float e = 1.0f + w;
            return 0.5f * e * e - (2.0f / 3.0f) * w * w + (a - e);
        }
        const float t = a - (1.0f - w);
        return 0.5f * a * a - t * t * t / (12.0f * w);
    }

    float onePole (float hz) const noexcept
    { return 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) sr); }
    float onePoleMs (float ms) const noexcept
    { return 1.0f - std::exp (-1.0f / (float) (ms * 0.001 * sr)); }

    // Model residue weights {Clean, Tape, Tube, Transistor} — Clean is {0,0}
    // BY DEFINITION (the null model); the other six numbers are P6
    // listening-test material.
    static constexpr float kModelOdd[4]  = { 0.0f, 0.60f, 0.25f, 0.90f };
    static constexpr float kModelEven[4] = { 0.0f, 0.15f, 0.60f, 0.10f };

    std::array<juce::SmoothedValue<float>*, 7> smoothers() noexcept
    { return { &driveDb, &shape, &depth, &balance, &tone, &tameDb, &mix }; }

    juce::SmoothedValue<float> driveDb { 0.0f }, shape { 0.5f }, depth { 0.0f },
                               balance { 0.0f }, tone { 0.0f }, tameDb { 0.0f },
                               mix { 1.0f };

    float adaaPrev[kMaxChannels] = {};
    float toneLp[kMaxChannels] = {}, dcState[kMaxChannels] = {}, tameLp[kMaxChannels] = {};
    float activityEnv = 0.0f;
    float aToneLp = 0.1f, aTameLp = 0.3f, aDcBlock = 0.001f, aActFast = 0.01f, aActSlow = 0.001f;
    int    model  = 1;
    bool   primed = false;
    double sr     = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipSat)
};

} // namespace anabasis
