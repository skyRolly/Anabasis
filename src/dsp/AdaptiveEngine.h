#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

// ============================================================================
//  AdaptiveEngine — §5.4: block-rate feature extraction + the bounded trim
//  vector, all on the audio thread (ADR-0011 discharged the placement).
//
//  FEATURES of the input programme (fed the delay-aligned dry frames):
//  short-term loudness proxy, crest factor, spectral tilt, transient density —
//  cheap per-sample accumulators finalised per block, published as relaxed
//  atomics (the meter row) for the UI overlay and consumed by the trim logic.
//  Features FREEZE while the programme sits under a silence gate (~−70 dBFS
//  mean square): a breakdown must not slew the trims toward the idle state.
//
//  TRIMS (policy inv 3/4): bounded deltas around the CURRENT parameter
//  values, slewed with second-scale constants + a hysteresis deadband — never
//  stepped, never a parameter write, never visible to host automation or
//  undo. The four members and their hard bounds:
//      release   ×2^t,  t ∈ [−1, +1]   (an octave either way)
//      stereoLink +t,   t ∈ [−0.2, +0.2]
//      scHpf     +t Hz, t ∈ [0, +30]
//      dynTilt   +t dB, t ∈ [0, +0.5]
//  Lookahead and the OS factor are structurally untouchable from here
//  (inv 4): this class emits only the four values above.
//
//  THE NULL SURVIVES BY CONSTRUCTION, not by gating — with one trim's
//  argument weaker than the other three's, stated rather than averaged over.
//  release/link/dynTilt multiply a unity gain or a zero-depth shelf: inert as
//  a matter of ARITHMETIC. scHpf is different in kind — it engages a real
//  second-order detector high-pass in both the compressor and the limiter, and
//  its inertness rests on the detector's output never crossing the knee bottom
//  or the ceiling. The RBJ high-pass at Q=0.707 has unity passband and can
//  only LOWER a detector level, so the argument holds for any stimulus already
//  below both thresholds — which is a property of the stimulus, and is why
//  testNullWithDefaults now asserts its own −3 dBFS precondition.
//
//  At the factory defaults every trim TARGET is inert: release/link/scHpf
//  only matter once the limiter reduces, and dynTilt feeds a shelf ClipSat
//  engages only while clipping activity is nonzero — so trims may move freely
//  on programme material without invariant 7 noticing. The bit-exact null
//  test runs with this engine live, which is the proof.
//
//  DETERMINISM (§5.4): trims are a pure slewed function of the features and
//  the reference targets — same audio + same parameters ⇒ same trims. No
//  randomness, no wall-clock.
//
//  FREEZE latches the vector: while frozen the four values hold exactly
//  (bit-repeatable behaviour thereafter — policy inv 3's Freeze clause) and
//  the capture side (serialization into the per-slot FROZEN_TRIMS) reads the
//  published atomics once the latch has settled. The RESTORE transport —
//  message → audio injection of a saved vector — is OQ-013 and remains a
//  Hard Stop: nothing here consumes FROZEN_TRIMS.
//
//  The trim MAPPING constants are ⊕ P4 drafts (tuned by ear before v0.1.0,
//  like the §5.5 curves); the SHAPE — bounded, slewed, deadbanded, frozen —
//  is the contract the tests pin.
// ============================================================================

namespace anabasis
{

class AdaptiveEngine
{
public:
    static constexpr int kMaxChannels = 2;

    // Neutral reference targets (⊕ draft; Learn re-fixes them at its commit —
    // the values a "typical" master sits near, so the factory state trims
    // toward zero on typical material). PUBLIC because the session restore
    // needs them as the missing-field defaults for the ADAPTIVE child, and a
    // second copy of the numbers in the wrapper is a drift waiting to happen.
    static constexpr float kDefaultRefOnset = 4.0f;   // transients/s
    static constexpr float kDefaultRefTilt  = -6.0f;  // hi/lo energy dB of typical programme

    struct Trims
    {
        float releaseOctaves = 0.0f;   // [-1, +1]
        float stereoLink     = 0.0f;   // [-0.2, +0.2]
        float scHpfHz        = 0.0f;   // [0, +30]
        float dynTiltDb      = 0.0f;   // [0, +0.5]
    };

    AdaptiveEngine() = default;

    void prepare (double sampleRate, int maxBlockSize)
    {
        juce::ignoreUnused (maxBlockSize);   // block length is discovered per call
        sr = sampleRate;
        aEnvFast  = onePoleMs (5.0f);
        aEnvDecay = onePoleMs (30.0f);       // fast envelope must RELEASE before the next hit
        aEnvSlow  = onePoleMs (500.0f);      // the baseline the hits punch above
        aFeature = onePoleMs (1500.0f);          // ~1.5 s feature integration
        aBand    = onePoleHz (800.0f);           // tilt split
        reset();
    }

    void reset() noexcept
    {
        trims = {};
        for (int ch = 0; ch < kMaxChannels; ++ch)
            bandLp[ch] = 0.0f;
        envFast = envSlow = 0.0f;
        msAvg = peakAvg = 1.0e-6f;
        loEnergy = hiEnergy = 1.0e-9f;
        onsetRate = 0.0f;
        onsetHold = 0;
        blockPeak = 0.0f; blockMs = 0.0; blockLo = 0.0; blockHi = 0.0;
        blockOnsets = 0; blockFill = 0;

        // An IN-FLIGHT Learn pass does not survive a reset. A reset reaches
        // here only through prepare() — a sample-rate or block-size change,
        // since the processor does not override AudioProcessor::reset() — and
        // that is a discontinuity in the very material the pass is measuring.
        // The features themselves are zeroed above, so continuing to
        // accumulate would commit a
        // reference mixed from before and after it, plus a stretch of
        // re-converging onset rate. The pass is CANCELLED rather than paused:
        // `learnBlocks == 0` makes the next commit a no-op, which is the
        // already-documented empty-pass case (nothing to commit leaves the
        // existing reference alone).
        //
        // NOT cleared: `learned`, `refOnsetRate`, `refTiltDb`. Those are
        // session state — the answer a previous commit or a session restore
        // established — and a rate change is not a reason to forget it.
        learnActive.store (false, std::memory_order_release);
        learnOnsSum  = 0.0;
        learnTiltSum = 0.0;
        learnBlocks  = 0;

        publishTrims();
        pubCrestDb.store (0.0f, std::memory_order_relaxed);
        pubTiltDb.store (0.0f, std::memory_order_relaxed);
        pubOnsetRate.store (0.0f, std::memory_order_relaxed);
    }

    // Per sample, the delay-aligned dry (input) frame.
    void pushFrame (const float* x, int numCh) noexcept
    {
        const int nCh = juce::jmin (numCh, kMaxChannels);
        float mono = 0.0f, lo = 0.0f;
        for (int ch = 0; ch < nCh; ++ch)
        {
            mono += std::abs (x[ch]);
            bandLp[ch] += (x[ch] - bandLp[ch]) * aBand;
            lo += bandLp[ch] * bandLp[ch];
        }
        mono /= (float) nCh;

        float hi = 0.0f;
        for (int ch = 0; ch < nCh; ++ch)
        {
            const float h = x[ch] - bandLp[ch];
            hi += h * h;
        }

        blockPeak = juce::jmax (blockPeak, mono);
        blockMs  += (double) mono * mono;
        blockLo  += (double) lo;
        blockHi  += (double) hi;

        // Onset detector: a fast envelope punching >6 dB (2.0×) above the
        // slow one, with a 50 ms re-arm hold, counts one transient.
        envFast += (mono - envFast) * (mono > envFast ? aEnvFast : aEnvDecay);
        envSlow += (mono - envSlow) * aEnvSlow;
        if (onsetHold > 0)
            --onsetHold;
        else if (envFast > envSlow * 2.0f && envFast > 1.0e-4f)
        {
            ++blockOnsets;
            onsetHold = (int) (0.050 * sr);
        }
        ++blockFill;
    }

    // Once per processed block: finalise features, slew the trims.
    void finishBlock (bool freeze) noexcept
    {
        if (blockFill <= 0)
            return;
        const double blockDur = blockFill / sr;
        const double ms = blockMs / blockFill;

        // Silence gate: features (and therefore trims) HOLD on silence.
        const bool audible = ms > 1.0e-7;        // ~ -70 dBFS mean square
        if (audible)
        {
            const float a = 1.0f - std::pow (1.0f - aFeature, (float) blockFill);
            msAvg    += ((float) ms        - msAvg)    * a;
            peakAvg  += (blockPeak         - peakAvg)  * a;
            loEnergy += ((float) (blockLo / blockFill) - loEnergy) * a;
            hiEnergy += ((float) (blockHi / blockFill) - hiEnergy) * a;
            const float instRate = (float) (blockOnsets / juce::jmax (1.0e-3, blockDur));
            onsetRate += (instRate - onsetRate) * a;
        }

        const float crestDb = 20.0f * std::log10 (juce::jmax (peakAvg, 1.0e-6f)
                                                  / juce::jmax (std::sqrt (msAvg), 1.0e-6f));
        const float tiltDb  = 10.0f * std::log10 (juce::jmax (hiEnergy, 1.0e-9f)
                                                  / juce::jmax (loEnergy, 1.0e-9f));
        pubCrestDb.store (crestDb, std::memory_order_relaxed);
        pubTiltDb.store (tiltDb, std::memory_order_relaxed);
        pubOnsetRate.store (onsetRate, std::memory_order_relaxed);

        if (learnActive.load (std::memory_order_relaxed) && audible)
        {
            learnOnsSum  += onsetRate;
            learnTiltSum += tiltDb;
            ++learnBlocks;
        }

        blockPeak = 0.0f; blockMs = 0.0; blockLo = 0.0; blockHi = 0.0;
        blockOnsets = 0;

        if (! freeze && audible)
        {
            // Targets from the features (⊕ draft mapping, referenced to the
            // neutral targets below; the deadband is inv 3's hysteresis).
            Trims target;
            target.releaseOctaves = juce::jlimit (-1.0f, 1.0f,
                                                  (refOnsetRate - onsetRate) * 0.15f);
            target.stereoLink     = juce::jlimit (-0.2f, 0.2f,
                                                  (onsetRate - refOnsetRate) * 0.03f);
            target.scHpfHz        = juce::jlimit (0.0f, 30.0f,
                                                  (refTiltDb - tiltDb) * 6.0f);
            target.dynTiltDb      = juce::jlimit (0.0f, 0.5f,
                                                  (tiltDb - refTiltDb) * 0.1f);

            // Second-scale slew toward the target, per block, with a deadband.
            const float slew = 1.0f - std::pow (1.0f - onePoleMs (2000.0f),
                                                (float) blockFill);
            auto step = [slew] (float& state, float tgt, float deadband) noexcept
            {
                if (std::abs (tgt - state) > deadband)
                    state += (tgt - state) * slew;
            };
            step (trims.releaseOctaves, target.releaseOctaves, 0.05f);
            step (trims.stereoLink,     target.stereoLink,     0.01f);
            step (trims.scHpfHz,        target.scHpfHz,        1.0f);
            step (trims.dynTiltDb,      target.dynTiltDb,      0.02f);
            publishTrims();
        }
        blockFill = 0;
    }

    // -- §5.4 Learn: explicit start → integrated-style accumulation of the
    //    feature set → explicit commit fixes the reference targets. Audio-
    //    thread calls (the engine consumes the wrapper's command atomics at
    //    the block top). Never runs silently: idle unless started.
    void startLearn() noexcept
    {
        learnActive.store (true, std::memory_order_release);
        learnOnsSum  = 0.0;
        learnTiltSum = 0.0;
        learnBlocks  = 0;
    }

    // Commit = the analysed passage becomes the reference: material like it
    // now trims toward ZERO, which is the whole point (Learn feeds the
    // reference targets, never the output stage — a maximizer must not
    // auto-match its output level).
    void commitLearn() noexcept
    {
        learnActive.store (false, std::memory_order_release);
        // learnBlocks counts only blocks that passed the silence gate, so a
        // start→stop over silence commits NOTHING and leaves an earlier
        // learned state live — which the next save then serializes. That is
        // the correct DSP behaviour (an empty pass must not wipe good
        // references) but it is indistinguishable from success to the caller.
        // The P5 Learn UI owes a failed/empty-pass readout; there is
        // deliberately no signal back to the wrapper at P4.
        if (learnBlocks > 0)
        {
            refOnsetRate = (float) (learnOnsSum  / learnBlocks);
            refTiltDb    = (float) (learnTiltSum / learnBlocks);
            publishRefs();
            // Refs first, flag RELEASE-stored after: a reader whose ACQUIRE
            // load of `learned` sees true is guaranteed to see the refs it
            // is about to serialize (getStateInformation runs off-thread).
            learned.store (true, std::memory_order_release);
        }
    }

    bool  isLearning() const noexcept    { return learnActive.load (std::memory_order_acquire); }
    bool  hasLearned() const noexcept    { return learned.load (std::memory_order_acquire); }

    // Session restore of learned targets (ADAPTIVE child), audio thread only
    // (the wrapper stages the pair and the engine consumes it at block top —
    // the release/acquire flag ordering on that hand-off is in
    // AnabasisEngine::restoreLearnedTargets). Deliberately distinct from
    // OQ-013's frozen-trim vector, whose four members are coherence-critical
    // (half-restored = permanently wrong); nothing here weakens that Hard Stop.
    void setLearnedTargets (float onsetRateIn, float tiltDbIn) noexcept
    {
        refOnsetRate = onsetRateIn;
        refTiltDb    = tiltDbIn;
        publishRefs();
        learned.store (true, std::memory_order_release);   // refs first — see commitLearn()
    }

    void clearLearnedTargets() noexcept   // "absent ADAPTIVE = never learned"
    {
        refOnsetRate = kDefaultRefOnset;
        refTiltDb    = kDefaultRefTilt;
        publishRefs();
        learned.store (false, std::memory_order_release);
    }

    float publishedRefOnset() const noexcept { return pubRefOnset.load (std::memory_order_relaxed); }
    float publishedRefTilt()  const noexcept { return pubRefTilt.load (std::memory_order_relaxed); }

    // -- published readouts (relaxed atomics — the meter row): the Advanced
    //    view's delta overlay and the P5 feature display read these ----------
    float publishedCrestDb() const noexcept    { return pubCrestDb.load (std::memory_order_relaxed); }
    float publishedTiltDb() const noexcept     { return pubTiltDb.load (std::memory_order_relaxed); }
    float publishedOnsetRate() const noexcept  { return pubOnsetRate.load (std::memory_order_relaxed); }
    float publishedTrimRelease() const noexcept { return pubTrimRel.load (std::memory_order_relaxed); }
    float publishedTrimLink() const noexcept    { return pubTrimLink.load (std::memory_order_relaxed); }
    float publishedTrimHpf() const noexcept     { return pubTrimHpf.load (std::memory_order_relaxed); }
    float publishedTrimTilt() const noexcept    { return pubTrimTilt.load (std::memory_order_relaxed); }

private:
    // AUDIO-THREAD ONLY, and private so that is true by construction rather
    // than by convention: `trims` is a plain struct this class mutates in
    // finishBlock(), and the wrapper hands a const reference to this whole
    // object to message-thread callers (adaptiveReadout). Everything public
    // above is an atomic; a plain-struct getter beside them is the shape that
    // produced the `learned` and `learnActive` races. The engine — the only
    // in-tree caller, on the audio thread — reaches it through this friendship;
    // the P5 UI reads publishedTrim*() like every other display value.
    friend class AnabasisEngine;
    const Trims& currentTrims() const noexcept { return trims; }

    void publishTrims() noexcept
    {
        pubTrimRel.store  (trims.releaseOctaves, std::memory_order_relaxed);
        pubTrimLink.store (trims.stereoLink,     std::memory_order_relaxed);
        pubTrimHpf.store  (trims.scHpfHz,        std::memory_order_relaxed);
        pubTrimTilt.store (trims.dynTiltDb,      std::memory_order_relaxed);
    }

    float onePoleMs (float ms) const noexcept
    { return 1.0f - std::exp (-1.0f / (float) (ms * 0.001 * sr)); }
    float onePoleHz (float hz) const noexcept
    { return 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) sr); }

    float refOnsetRate = kDefaultRefOnset;
    float refTiltDb    = kDefaultRefTilt;

    void publishRefs() noexcept
    {
        pubRefOnset.store (refOnsetRate, std::memory_order_relaxed);
        pubRefTilt.store (refTiltDb,     std::memory_order_relaxed);
    }

    // Atomic for the same reason `learned` is: the wrapper hands a const
    // reference to message-thread callers (adaptiveReadout), and the P5 Learn
    // UI will poll this for its indicator. Audio-thread writers use release,
    // audio-thread readers relaxed — only the public getter pays for acquire.
    std::atomic<bool> learnActive { false };
    std::atomic<bool> learned { false };   // written on audio thread, read by getStateInformation
    double learnOnsSum = 0.0, learnTiltSum = 0.0;
    int64_t learnBlocks = 0;

    double sr = 48000.0;

    float bandLp[kMaxChannels] = {};
    float envFast = 0.0f, envSlow = 0.0f;
    float aEnvFast = 0.01f, aEnvDecay = 0.003f, aEnvSlow = 0.001f,
          aFeature = 0.0001f, aBand = 0.1f;

    float  msAvg = 0.0f, peakAvg = 0.0f, loEnergy = 0.0f, hiEnergy = 0.0f, onsetRate = 0.0f;
    int    onsetHold = 0;
    float  blockPeak = 0.0f;
    double blockMs = 0.0, blockLo = 0.0, blockHi = 0.0;
    int    blockOnsets = 0, blockFill = 0;

    Trims trims;

    std::atomic<float> pubCrestDb { 0.0f }, pubTiltDb { 0.0f }, pubOnsetRate { 0.0f };
    std::atomic<float> pubTrimRel { 0.0f }, pubTrimLink { 0.0f },
                       pubTrimHpf { 0.0f }, pubTrimTilt { 0.0f };
    std::atomic<float> pubRefOnset { kDefaultRefOnset }, pubRefTilt { kDefaultRefTilt };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdaptiveEngine)
};

} // namespace anabasis
