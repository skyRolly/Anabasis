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
//  release and link only shape an envelope that never leaves 1.0: inert as a
//  matter of ARITHMETIC. scHpf is different in kind — it engages a real
//  second-order detector high-pass in both the compressor and the limiter, and
//  its inertness rests on the detector's output never crossing the knee bottom
//  or the ceiling. The RBJ high-pass at Q=0.707 has unity passband and can
//  only LOWER a detector level, so the argument holds for any stimulus already
//  below both thresholds — which is a property of the stimulus, and is why
//  testNullWithDefaults now asserts its own −3 dBFS precondition.
//
//  dynTilt is a THIRD mechanism again, and the weakest to state loosely: its
//  target does NOT sit at zero — with the features un-converged
//  (loEnergy == hiEnergy at init ⇒ tiltDb 0 against kDefaultRefTilt −6) it
//  goes straight to its +0.5 dB clamp. The null holds because ClipSat's
//  `activityEnv` starts at EXACTLY 0.0f and its update keeps it bit-zero
//  while nothing clips, so the tame branch is never taken. That is a state
//  argument, not a coefficient one: seeding the activity detector non-zero,
//  or giving it a floor, would break invariant 7 without touching this file.
//
//  So: trims may move freely on programme material without invariant 7
//  noticing, by three different arguments. The bit-exact null test runs with
//  this engine live, which is the proof — and the three mechanisms are why it
//  is a proof of something, rather than a coincidence.
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
        // Seeded so the PUBLISHED crest starts at 0 dB, not at a physically
        // impossible one: crest is 20·log10(peakAvg / sqrt(msAvg)), so the two
        // seeds must be a square apart to mean "no measurement yet". Seeding
        // both at 1e-6 read as -60 dB until the 1.5 s integrator climbed out
        // of it — invisible today (nothing consumes crest; the trims use onset
        // rate and tilt) and a wrong number on the P5 readout tomorrow.
        msAvg   = 1.0e-12f;
        peakAvg = 1.0e-6f;
        loEnergy = hiEnergy = 1.0e-9f;
        onsetRate = 0.0f;
        onsetHold = 0;
        blockPeak = 0.0f; blockMs = 0.0; blockLo = 0.0; blockHi = 0.0;
        blockOnsets = 0; blockFill = 0;

        // An IN-FLIGHT Learn pass does not survive a reset. A reset reaches
        // here only through prepare() — a sample-rate or block-size change,
        // since the processor does not override AudioProcessor::reset() — and
        // that is a discontinuity in the very material the pass is measuring.
        // The features themselves are zeroed above, so continuing to accumulate
        // would commit a reference mixed from before and after it, plus a
        // stretch of
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

    // Invariant 9, the unconditional half. Called once per block by the
    // engine, NOT from the recovery flag, because this class poisons itself
    // from a FINITE input and produces no non-finite audio to detect: it is
    // fed the delay-aligned dry signal, which the engine keeps finite but does
    // not bound, and `lo += bandLp*bandLp` overflows in float at ~1.8e19.
    // From there `loEnergy` goes inf, then `inf + (finite − inf)·a` = NaN on
    // the next block, and every consequence is SILENT: `tiltDb`/`crestDb`
    // publish NaN, and the hysteresis test `|tgt − state| > deadband` is false
    // for NaN, so the trim vector freezes at whatever it held — for the rest
    // of the session, looking like a perfectly plausible set of trims.
    //
    // Non-finite members are returned to their reset() seeds rather than to
    // zero: the seeds mean "no measurement yet", which is exactly true after
    // one has been thrown away, and the crest pair must stay a square apart.
    void sanitiseState() noexcept
    {
        for (int ch = 0; ch < kMaxChannels; ++ch)
            if (! std::isfinite (bandLp[ch]))
                bandLp[ch] = 0.0f;
        if (! std::isfinite (envFast) || ! std::isfinite (envSlow))
            envFast = envSlow = 0.0f;
        if (! std::isfinite (msAvg) || ! std::isfinite (peakAvg))
        {
            msAvg   = 1.0e-12f;
            peakAvg = 1.0e-6f;
        }
        if (! std::isfinite (loEnergy) || ! std::isfinite (hiEnergy))
            loEnergy = hiEnergy = 1.0e-9f;
        if (! std::isfinite (onsetRate))
            onsetRate = 0.0f;
        if (! std::isfinite (blockPeak) || ! std::isfinite (blockMs)
            || ! std::isfinite (blockLo) || ! std::isfinite (blockHi))
        {
            blockPeak = 0.0f; blockMs = 0.0; blockLo = 0.0; blockHi = 0.0;
        }
        if (! std::isfinite (trims.releaseOctaves) || ! std::isfinite (trims.stereoLink)
            || ! std::isfinite (trims.scHpfHz) || ! std::isfinite (trims.dynTiltDb))
            trims = {};

        // A Learn pass that accumulated a NaN feature is already ruined, and
        // its commit would write that NaN into `refTiltDb` — which the next
        // save serializes into the session's ADAPTIVE child, making the damage
        // outlive the session. Cancelled exactly the way reset() cancels one:
        // `learnBlocks == 0` makes the next commit the documented empty-pass
        // no-op, and `learned` / the references are session state and stay.
        if (! std::isfinite (learnOnsSum) || ! std::isfinite (learnTiltSum))
        {
            learnActive.store (false, std::memory_order_release);
            learnOnsSum  = 0.0;
            learnTiltSum = 0.0;
            learnBlocks  = 0;
        }
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
            // These are the ~1.5 s INTEGRATED features, not this block's
            // instantaneous ones, and startLearn() deliberately does not
            // re-seed them: re-seeding would make the first second of every
            // pass a re-convergence ramp, which is a worse bias than the one
            // it removes. The bias it leaves: a pass carries roughly 1.5 s of
            // whatever was playing BEFORE the user pressed Learn, weighted
            // down exponentially. Immaterial for a multi-second pass (the
            // test's is ~5 s) and material for a very short one — so the P5
            // Learn grammar owes a MINIMUM PASS LENGTH rather than this owing
            // a re-seed. Recorded here because the sums look instantaneous.
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
            const float ons  = (float) (learnOnsSum  / learnBlocks);
            const float tilt = (float) (learnTiltSum / learnBlocks);
            // A pass that measured through an overflow is REFUSED here rather
            // than repaired later, and the check lives at the writer for a
            // reason: sanitiseState() cancels such a pass, but it runs later in
            // the same block than the staged Learn command is consumed, so a
            // commit landing on the block AFTER the overflow gets in first.
            // Making the commit safe regardless of call order is the fix; the
            // ordering is not something a future edit should have to preserve.
            // Refusing means the same outcome as the empty pass above — the
            // existing references stay, nothing is published, `learned` is not
            // raised — which matters because a committed NaN reference would be
            // PERMANENT (every trim target derives from it, and `jlimit` and
            // the hysteresis both pass NaN through untouched) and PERSISTENT
            // (`hasLearned()` true means the next save writes it into the
            // session's ADAPTIVE child).
            if (std::isfinite (ons) && std::isfinite (tilt))
            {
                refOnsetRate = ons;
                refTiltDb    = tilt;
                publishRefs();
                // Refs first, flag RELEASE-stored after: a reader whose ACQUIRE
                // load of `learned` sees true is guaranteed to see the refs it
                // is about to serialize (getStateInformation runs off-thread).
                learned.store (true, std::memory_order_release);
            }
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
        // The other writer of the references, and the other way a non-finite
        // one could get in: a session file written by a build that did commit
        // one (or an edited file). COMPATIBILITY_POLICY's read rule — a value
        // that cannot be read is the default — applied to a value that can be
        // read and cannot be used.
        if (! std::isfinite (onsetRateIn) || ! std::isfinite (tiltDbIn))
        {
            clearLearnedTargets();
            return;
        }
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
