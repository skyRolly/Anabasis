#pragma once

#include "EngineParameters.h"
#include "Latency.h"
#include "MasteringEQ.h"
#include "MasteringComp.h"
#include "ClipSat.h"
#include "LookaheadLimiter.h"
#include "CeilingClamp.h"
#include "LoudnessMeter.h"
#include "TruePeak.h"
#include "AdaptiveEngine.h"
#include "ScopeBuffer.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <memory>

// ============================================================================
//  AnabasisEngine — chain owner (ADR-0001: format-agnostic, sees only the
//  EngineParameters POD; never includes a plugin-client or GUI header).
//
//  P2 chain, per DSP_POLICY invariant 1 / ADR-0002/0003:
//
//    base rate:  Input Gain → EQ(Pre) → Compressor
//    OS region:  [up ×N] → Clipper/Sat → limiter push → 10 ms lookahead line
//                → LookaheadLimiter → [down ×N]
//    base rate:  EQ(Post) → CeilingClamp → Dither → bypass crossfade → out
//
//  Latency contract (ADR-0004): the audio path is delayed by the FULL 10 ms
//  lookahead allowance at every setting (the lookahead line runs INSIDE the
//  region at N× rate, delaying delaySamples·N OS samples = exactly 10 ms of
//  base samples) plus the oversampler's integer group delay from Latency.h's
//  measured table. groupDelaySamples() stays the base allowance; the wrapper
//  adds the OS term through the same predictLatencySamples() the engine's
//  dry-ring alignment uses, so reported and actual cannot drift silently.
//
//  Oversampling (ADR-0003/0011): every factor × phase instance is constructed
//  and initProcessing'd at prepare(); a runtime factor/phase change LATCHES at
//  a block boundary AT THE §2.8 DUCK'S SILENT BOTTOM — it selects among
//  existing objects, allocates nothing, and resets the region state while the
//  output gain is zero. useIntegerLatency keeps every configuration's group
//  delay a whole base sample, which is what lets the bypass stay a bit-exact
//  integer-delay null.
//
//  §2.8 transition layer: asymmetric raised-cosine duck (~6 ms out / ~28 ms
//  in) for every discrete rewire — eqPosition, colourModel, OS factor/phase,
//  and wrapper-requested bulk swaps (requestForcedDuck before A/B, preset,
//  session load). Engine rewires execute only at the silent bottom; wrapper
//  swaps land as smoothed parameter glides under the duck's envelope.
//
//  Bypass is a delay-aligned dry path (base-rate ring, offset = allowance +
//  osLatency) with a bit-exact-at-the-endpoints crossfade.
//
//  Invariant 9: non-finite samples are replaced with silence at the staging
//  and output boundaries, and a block that saw any discards the limiter's
//  sliding window (envelope carried — see LookaheadLimiter::resetWindow).
// ============================================================================

namespace anabasis
{

class AnabasisEngine
{
public:
    // Explicit because the non-copyable guard below is a user-declared
    // constructor, which suppresses the implicit default one.
    AnabasisEngine() = default;

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset() noexcept;

    // Audio thread. Adopts the per-block POD snapshot (ADR-0011). Blocks
    // larger than the prepared maximum are processed in prepared-size chunks,
    // so a host that violates its own declared maximum degrades to extra
    // chunk overhead instead of unprocessed audio.
    //
    // Returns FALSE when the call short-circuited (no samples, no channels, or
    // not prepared) and therefore left every meter tap holding the previous
    // block's values. The wrapper asks rather than re-deriving the condition:
    // a re-derivation drifts the moment this early return grows a term, and it
    // did — the first version of the publish guard missed `ringSizeOs <= 0`.
    bool process (juce::AudioBuffer<float>& buffer, const EngineParameters& params) noexcept;

    int groupDelaySamples() const noexcept { return delaySamples; }

    // §2.8: the forced-duck request — the THREADING_POLICY momentary-request
    // row (payload-free single atomic, exchange-consumed at the block top).
    // The wrapper calls this BEFORE every bulk swap (A/B, preset, session
    // load); the engine also self-requests for its own discrete rewires
    // (eqPosition, colourModel, OS factor/phase), which additionally apply
    // ONLY at the silent bottom.
    void requestForcedDuck() noexcept { duckRequested.store (true, std::memory_order_relaxed); }

    // §5.4 Learn commands — ONE atomic word, not two flags and not a record.
    // Two independent flags consumed in a fixed order cannot express what the
    // user did: a stop followed by a start inside one block left `startLearn`
    // running first, which zeroed the accumulator the stop was about to commit,
    // and then `commitLearn` no-opped on `learnBlocks == 0` — the finished pass
    // never committed AND the new one never began.
    //
    // A code PLUS a flag cannot either, and that was the second version of this
    // path (a staged record, ADR-0012's row): the writer published `code` and
    // then `pending` as two stores, so a consumer whose `exchange` landed
    // BETWEEN them ran the already-visible new code and left the writer to
    // re-raise `pending` behind it — the same command delivered twice. For a
    // bare commit that is harmless (it re-commits identical values), but a
    // commitThenStart delivered twice commits the pass its own first delivery
    // started ONE BLOCK earlier: `learnBlocks == 1`, so the saved reference is
    // measured from a single block of audio and then serialized. The payload
    // is two bits wide, so there is nothing to stage — the code IS the flag,
    // `kLearnNone` means nothing pending, and one store cannot be split. That
    // moves this edge from ADR-0012's staged-record row to the single
    // lock-free scalar row it always fitted (THREADING_POLICY), which is a
    // NARROWING: no new mechanism, one fewer window.
    //
    // The ordering information lives on the WRITER's thread, so that is where
    // the pair is composed: a start arriving on top of an unconsumed commit
    // becomes ONE commitThenStart command, and every other sequence degrades to
    // last-writer-wins. Reading the word back to compose is ADR-0012
    // condition 5, unchanged.
    //
    // TWO NARROW RESIDUALS, stated rather than implied by "last-writer-wins"
    // (both need two commands inside one ~10 ms block; both are in ADR-0012's
    // Known limits):
    //  • start→stop collapses to a bare commit, so if a pass was ALREADY
    //    running the commit lands on ITS statistics rather than on the
    //    just-started-and-aborted one. A valid reference, not the one the
    //    user's two clicks described. (With nothing running it is the
    //    documented empty-pass no-op, which is the only case the first
    //    version of this comment covered.)
    //  • the consumer clears the word a few instructions before it acts on it,
    //    so a start landing in that window reads `kLearnNone`, composes as a
    //    bare start, and the outstanding commit is dropped. This is the
    //    OPPOSITE direction of the re-delivery above, and it is the one that
    //    survives: closing it means reading the code before clearing it, which
    //    trades a dropped command for a duplicated one.
    void requestLearnStart() noexcept
    {
        const int outstanding = learnCmd.load (std::memory_order_acquire);
        const bool commitOutstanding = outstanding == kLearnCommit
                                    || outstanding == kLearnCommitThenStart;
        learnCmd.store (commitOutstanding ? kLearnCommitThenStart : kLearnStart,
                        std::memory_order_release);
    }
    void requestLearnStop() noexcept
    {
        learnCmd.store (kLearnCommit, std::memory_order_release);
    }

    // ADR-0014 frozen-trim restore (OQ-013 resolved 2026-08-02, owner-
    // approved): the four-scalar vector crosses on ADR-0012's staged-record
    // row — payload relaxed first, one flag release-stored after, consumed
    // with exchange(acquire) at a block top — and is APPLIED at the §2.8
    // duck's silent bottom (or the offline direct-adopt), which is where
    // DESIGN §7 places every restore-driven discontinuity. Only the wrapper
    // stages one, and only for a freeze-ON slot.
    void restoreFrozenTrims (float relOct, float link, float hpf, float tilt) noexcept
    {
        stagedFrozen[0].store (relOct, std::memory_order_relaxed);
        stagedFrozen[1].store (link,   std::memory_order_relaxed);
        stagedFrozen[2].store (hpf,    std::memory_order_relaxed);
        stagedFrozen[3].store (tilt,   std::memory_order_relaxed);
        // Stage generation, stored BEFORE the flag so the consumer's acquire
        // orders it with the payload. See frozenRestorePending() for why the
        // flag alone cannot answer the writer's question. `fetch_add` rather
        // than load+store: `setStateInformation` is not promised on the message
        // thread (KI-003), so two stagers can in principle overlap, and a lost
        // increment would read as "already applied".
        frozenStageSeq.fetch_add (1u, std::memory_order_relaxed);
        frozenPending.store (true, std::memory_order_release);
        // NOTE the absence of a duck request here. The bottom is the only place
        // this vector can land, so a staged record must always get one — but
        // the guarantee is asserted where the record is CONSUMED (process()'s
        // block top sets `duckAsked` from it), not by a second store beside
        // this flag. A second store would be a second thing to observe: the
        // consumer reads `duckRequested` a dozen lines before this flag, so a
        // block could take the record and miss the request. Deriving the
        // request from the record cannot go out of order with it.
    }
    // "Is the engine's published trim vector still older than the last record
    // I staged?" — which is about APPLICATION, not consumption. `frozenPending`
    // is cleared by the block-top exchange, but the vector is only injected
    // (and therefore published) at the duck's silent bottom up to ~34 ms later;
    // a save landing in that window read the PRE-restore trims and overwrote
    // the loaded copy with them. The two counters close it exactly: the
    // consumer remembers the generation it took and stores it back only after
    // injectTrims, so equality means "published == last staged".
    //
    // The applied side is RELEASE/ACQUIRE, not relaxed, and the distinction is
    // the whole point of the pair: unlike the display counters on
    // THREADING_POLICY's staleness row, this one GATES a read of other atomics
    // (`publishedTrim*`). Relaxed on both sides would let a reader observe
    // "settled" while the four published trims injectTrims wrote just before it
    // were still the old ones — the same stale capture, one reordering later.
    // The acquire here pairs with the release in the application sites.
    //
    // A record staged and then dropped by prepare()/reset() cannot strand this:
    // those clear neither `havePendingFrozen` nor the flag, and the unprimed
    // direct-adopt branch applies the pending copy on the first block after
    // either.
    bool frozenRestorePending() const noexcept
    {
        return frozenStageSeq.load (std::memory_order_relaxed)
            != frozenAppliedSeq.load (std::memory_order_acquire);
    }

    // Learned-target restore (session load, ADAPTIVE child): host-hidden
    // session state through the mirror pattern — consumed at the block top.
    // ONE staged record — payload (`pendingLearned` discriminator + the two
    // refs) stored first, the single flag RELEASE-stored after; the consumer
    // exchanges the flag with ACQUIRE, so a block that sees it reads THIS
    // call's record, never a torn one. Two independent flags with a fixed
    // consumption order were the earlier form and had a real defect: two
    // restores between audio blocks (a learned session, then an un-learned
    // one) left the LAST loaded session holding the FIRST one's references.
    // Last writer wins is the only correct rule here, and one flag is how it
    // is expressed.
    void restoreLearnedTargets (float onsetRate, float tiltDb) noexcept
    {
        pendingRefOnset.store (onsetRate, std::memory_order_relaxed);
        pendingRefTilt.store (tiltDb, std::memory_order_relaxed);
        pendingLearned.store (true, std::memory_order_relaxed);
        adaptivePending.store (true, std::memory_order_release);
    }
    void restoreNeverLearned() noexcept
    {
        pendingLearned.store (false, std::memory_order_relaxed);
        adaptivePending.store (true, std::memory_order_release);
    }

    // True while a staged record has not yet been consumed by a block top.
    // The WRITER side reads this (message thread, same thread that staged it)
    // to answer "is the engine's learned state still older than the session I
    // just loaded?" — getStateInformation needs that, because a host can load
    // and re-save with no audio in between.
    bool adaptiveRestorePending() const noexcept
    { return adaptivePending.load (std::memory_order_acquire); }

    AdaptiveEngine& adaptiveForWrapper() noexcept { return adaptiveEngine; }

    // False if any oversampler the pinned JUCE built disagrees with the
    // Latency.h table at the last prepare(). Recorded in Release as well as
    // Debug because the table is load-bearing for both reported PDC and the
    // dry-ring alignment the bypass null rides on (ADR-0004).
    bool latencyTableMatchesJuce() const noexcept { return osTableMatchesJuce; }

    // The engaged lookahead window in BASE samples, as last handed to the
    // detector. Exposed because it is where invariant 8's "smooth,
    // band-limited" requirement for a lookahead move is observable — the
    // output is not, since the wedge and the attack/release asymmetry
    // absorb a tap step.
    int engagedWindowSamples() const noexcept
    { return engagedWindow.load (std::memory_order_relaxed); }

    // The block's deepest limiter gain (linear, ≤ 1) — the §2.9 GR meter tap.
    // Same publication class as engagedWindow: relaxed atomic, monotonic
    // display data, written once per process() call on the audio thread.
    float lastBlockMinGain() const noexcept
    { return grMinLinear.load (std::memory_order_relaxed); }

    // Per-stage GR for the P5 panel meters — the answer to the recorded
    // "which reduction is the meter showing" question: the COMP panel shows
    // this, the LIMITER panel shows lastBlockMinGain()'s dB, and neither
    // pretends to be chain reduction. Relaxed atomic, published per block.
    float lastCompGrDb() const noexcept
    { return compGrDb.load (std::memory_order_relaxed); }

    // -- §2.9 output metering: the RENDER tap ---------------------------------
    // Fed per sample from the bypass-mixed programme path BEFORE the two
    // monitor-only stages (§2.7 delta substitution and loudness-comp gain).
    // The buffer handed back to the host is the LISTENING path; metering that
    // buffer made the LUFS/dBTP readings follow whatever the user was
    // auditioning — Delta showed the difference signal's loudness, Comp the
    // attenuated level — and, because integrated LUFS and the dBTP hold are
    // session-cumulative, a few seconds of either permanently biased both.
    // With the monitor functions off the render and listening paths are
    // bit-identical (exact endpoints), so these read the same as buffer
    // metering did. Same-thread reads: the wrapper calls these right after
    // process() on the audio thread, so no atomics are needed.
    const LoudnessMeter& outputLoudness() const noexcept { return outMeter; }

    // §2.9 spectrum capture rings (THREAD_MODEL's planned edge, implemented
    // at P5 on the SPSC ring row): post-input-gain and post-chain (the render
    // tap), each published with one release-store per processed chunk. The
    // GUI-side FFT reads them as stateless peeks; nothing here consumes.
    const ScopeBuffer& spectrumInRing()  const noexcept { return specInRing; }
    const ScopeBuffer& spectrumOutRing() const noexcept { return specOutRing; }

    // §2.9 meter-hold reset, audio thread (the wrapper consumes the request at
    // the top of processBlock and calls this). Clears ONLY the render meter's
    // session-cumulative half — the integrated histogram. Deliberately not
    // touched: the §2.7 dry/wet meters (they feed the loudness COMPENSATION,
    // a monitor function — clearing them would bounce the monitor gain, which
    // is not what a meter-reset button means) and the GR ring (a rolling ~43 s
    // window that clears itself; the wrapper owns it in any case).
    void resetMeterHolds() noexcept { outMeter.resetIntegrated(); }
    float lastRenderTpMax() const noexcept { return renderTpMaxCall; }   // linear
    float lastRenderPeak() const noexcept  { return renderPeakCall; }    // plain |x| max

private:
    void latchOsConfig (int factorIdx, int phaseIdx) noexcept;
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int num,
                       const EngineParameters& p, bool eqPre, bool eqPost) noexcept;

    static constexpr int kMaxChannels = 2;
    static constexpr int kMaxOsFactorLog2 = 4;   // 16×

    double sr           = 48000.0;
    int    delaySamples = 480;        // maxLookaheadSamples(sr), set in prepare()
    int    maxBlock     = 512;
    int    numChans     = 2;

    // OS-rate lookahead line (wet) — allocated for 16× — and the base-rate
    // dry line for the bypass, with kMaxOsLatencySamples of extra depth.
    juce::AudioBuffer<float> wetRing, dryRing, staging;
    int ringSizeOs   = 0;             // logical size for the CURRENT factor
    int writePosOs   = 0;
    int delayOs      = 480;           // delaySamples · osN
    int dryRingSize  = 0;
    int dryWritePos  = 0;

    // Per-base-sample control values, filled in stage A and indexed by the
    // region at OS rate (i >> osShift): the same instantaneous ceiling the
    // gain computer uses reaches the clamp, exactly as before.
    std::vector<float> ceilArr;
    std::vector<int>   wArr;
    std::vector<float> pushArr;       // limiter push, applied inside the region

    // §2.9 spectrum taps: per-chunk staging (filled in stages A and E, pushed
    // once per chunk with a single release-store each) + the two SPSC rings
    // the GUI FFTs from. Scratch is sized in prepare(); no audio-thread
    // allocation.
    std::vector<float> specInL, specInR, specOutL, specOutR;
    ScopeBuffer specInRing, specOutRing;

    juce::SmoothedValue<float> inputGain      { 1.0f };
    juce::SmoothedValue<float> pushGain       { 1.0f };
    juce::SmoothedValue<float> ceilingLinear  { 0.8912509f };  // -1 dBTP default
    juce::SmoothedValue<float> windowSamples  { 96.0f };        // engaged lookahead, BASE samples
    bool smoothersPrimed = false;
    std::atomic<int> engagedWindow { 96 };
    std::atomic<float> grMinLinear { 1.0f };
    std::atomic<float> compGrDb { 0.0f };
    float grMinThisCall = 1.0f;

    LookaheadLimiter limiter;
    CeilingClamp     clamp;
    MasteringEQ      eq;
    MasteringComp    comp;
    ClipSat          clip;

    // §2.8 transition ducker: asymmetric raised cosine, ~6 ms out / ~28 ms
    // in. Gain advances per base sample in stage E and multiplies the
    // PROCESSED path only (bypass stays a bit-exact null). Engine-side
    // rewires are held in the applied* fields until the bottom; the POD the
    // stages see carries the APPLIED values, so nothing rewires at full gain.
    enum class DuckState { idle, out, bottom, in };
    std::atomic<bool> duckRequested { false };

    // ADR-0014 staged record + the audio-thread pending copy the duck bottom
    // applies (consumed at the block top per ADR-0012, applied at the bottom
    // per DESIGN §7 — two steps, both audio-thread).
    std::atomic<float> stagedFrozen[4] = { {0.0f}, {0.0f}, {0.0f}, {0.0f} };
    std::atomic<bool>  frozenPending { false };
    // Written by the message thread (stage) / audio thread (apply); read by the
    // message thread. Equal ⇒ the published vector is the last staged one.
    std::atomic<uint32_t> frozenStageSeq { 0 }, frozenAppliedSeq { 0 };
    AdaptiveEngine::Trims pendingFrozenTrims;
    uint32_t pendingFrozenSeq = 0;     // the generation `pendingFrozenTrims` holds
    bool havePendingFrozen = false;
    // kLearnNone is the "nothing pending" value, which is what lets the code
    // and the flag be the same word — see requestLearnStart above.
    static constexpr int kLearnNone = 0, kLearnStart = 1, kLearnCommit = 2,
                         kLearnCommitThenStart = 3;
    std::atomic<int> learnCmd { kLearnNone };
    std::atomic<bool> adaptivePending { false }, pendingLearned { false };
    std::atomic<float> pendingRefOnset { 0.0f }, pendingRefTilt { 0.0f };
    DuckState duckState = DuckState::idle;
    float duckGain = 1.0f, duckPhase = 0.0f;
    float duckOutInc = 0.0f, duckInInc = 0.0f;
    int   appliedEqPos = 0, appliedModel = 1;   // == the POD defaults

    // Two reasons the silent bottom is held past the block that reaches it:
    //  • refill — a latch empties the lookahead ring and resets the
    //    oversampler, so the processed path is EXACTLY silent for
    //    delaySamples + osLatBase base samples afterwards. Recovering before
    //    that expires puts the first real sample partway up the 28 ms ramp
    //    (~0.35 at 4×/48 kHz = a −9 dB step) — the click this layer exists to
    //    prevent, and the one case where the ramp itself is not the artefact.
    //  • a duck request that arrived while the out-leg was still running: the
    //    bulk swap it guards reaches the snapshot a block later, so it must
    //    find the engine still at zero gain.
    int  bottomHoldSamples = 0;
    bool duckAskedWhileOut = false;
    bool lastNonRealtime   = false;   // realtime↔offline flips adopt directly

    // Oversampling: [factorLog2 − 1][phase] — all eight built at prepare().
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplers[kMaxOsFactorLog2][2];
    juce::dsp::Oversampling<float>* osActive = nullptr;   // null = Off
    int latchedFactorIdx = -1;        // -1 Off, 0..3 = 2×..16×
    int latchedPhaseIdx  = 0;
    int osN = 1, osShift = 0;
    int osLatBase = 0;                // Latency.h table value for the latched config
    bool osTableMatchesJuce = true;   // set at prepare(), asserted by the suite

    // §2.7 loudness-compensated monitoring + delta. MONITOR-ONLY functions
    // (DSP_POLICY invariant 10): both are inert whenever nonRealtime is set,
    // so the render is untouched — that is the tested contract, not a hope.
    // Measure: K-weighted short-term loudness of the delay-aligned dry vs the
    // processed path (two always-fed LoudnessMeters; the measure FREEZES when
    // either side's momentary drops under the BS.1770 −70 LUFS absolute gate,
    // chosen over a dBFS gate because a mastering plugin meets quiet
    // classical passages). Predict: stateless floor from the deterministic
    // gain lift (inputGain + limGain + average measured GR), only ever
    // LOWERING monitor gain — cranking the macro pre-ducks instantly, no
    // ratchet. Applied = min(measure, predict), smoothed 200 ms, POST-mix so
    // the bypass leg carries the same compensation (the §2.7 loudness-matched
    // bypass). Delta = (delay-aligned dry − processed) behind its own
    // always-running ~10 ms crossfade.
    LoudnessMeter dryMeter, wetMeter;

    // §2.9 render-tap meters (see the public accessors for why these live in
    // the engine and not the wrapper: only the engine sees the sample before
    // the monitor-only stages touch it).
    LoudnessMeter     outMeter;
    TruePeakEstimator outTp;
    float renderTpMaxCall = 0.0f, renderPeakCall = 0.0f;
public:
    // §5.4 feature/trim readouts for the Advanced-view overlay and tests.
    const AdaptiveEngine& adaptive() const noexcept { return adaptiveEngine; }
private:
    AdaptiveEngine adaptiveEngine;
    float compMeasureDb = 0.0f;              // frozen on silence
    juce::SmoothedValue<float> monitorGain { 1.0f };
    float deltaMix = 0.0f, deltaStep = 0.0f;
    bool  deltaTarget = false;

    // Dither (§4.5): TPDF at the target LSB, optional first-order noise
    // shaping, deterministic xorshift so an offline render is repeatable.
    uint32_t rngState = 0x9E3779B9u;
    float    ditherErr[kMaxChannels] = {};

    // §2.8 minimal form: linear output crossfade between wet and dry over
    // ~10 ms, with exact-endpoint branches so both null tests are bit-exact.
    float bypassMix     = 0.0f;       // 0 = wet, 1 = dry
    float bypassStep    = 0.0f;
    bool  bypassTarget  = false;

    // CODE_STYLE §Structure: owning classes carry the guard. This one owns the
    // rings, the staging buffer and eight oversampler instances; an accidental
    // copy would duplicate them silently instead of failing to compile.
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnabasisEngine)
};

} // namespace anabasis
