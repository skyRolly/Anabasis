#include "SpectrumView.h"
#include "../PluginProcessor.h"

using namespace abgui;

SpectrumView::SpectrumView (AnabasisAudioProcessor& p)
    // Sized HERE, not in the body: `std::atomic<float>` is neither copyable nor
    // movable, so a vector of them can be built with a count but never resized.
    : processor (p), pubIn ((size_t) kBins), pubOut ((size_t) kBins)
{
    scratchInL.resize (kSize);
    scratchInR.resize (kSize);
    scratchOutL.resize (kSize);
    scratchOutR.resize (kSize);
    fftData.resize ((size_t) kSize * 2);
    inDb.assign (kBins, -120.0f);
    outDb.assign (kBins, -120.0f);
    // …and the two copies the pair passes through on its way to the screen, at
    // the same floor: an analyser with no frame yet draws the empty trace, and
    // that is true of the painter's copy before the first publication exactly as
    // it is of the EMA before the first analysis.
    paintIn.assign (kBins, -120.0f);
    paintOut.assign (kBins, -120.0f);
    stageIn.assign (kBins, -120.0f);
    stageOut.assign (kBins, -120.0f);
    for (int b = 0; b < kBins; ++b)
    {
        pubIn [(size_t) b].store (-120.0f, std::memory_order_relaxed);
        pubOut[(size_t) b].store (-120.0f, std::memory_order_relaxed);
    }
    // Fires over the mode switch only (`hitTest` narrows the pointer claim),
    // so the hint names the switch's ACTION. Same string in both views — it is
    // one control drawn twice. Wording ⊕.
    setTooltip ("Switch the graph between the spectrum and the GR history");
}

void SpectrumView::visibilityChanged()
{
    if (isVisible())
    {
        // ANALYSE BEFORE ANYONE CAN PAINT, FOR THE TIME THAT ACTUALLY PASSED
        // (0.2.12, from the review that followed the GR history's version of
        // this). The two views of the well share a LIFECYCLE — each stops its
        // clock when the other takes over — and therefore share one hazard:
        // while this view is hidden nothing analyses, and the audio thread goes
        // on filling both scope rings regardless (`AnabasisEngine::processChunk`
        // has no visibility term). They do NOT share a state model, and this is
        // not the GR fix transplanted: there is no head here, no phase, no
        // position of any kind. What is retained is `inDb`/`outDb`, the two
        // per-bin EMAs, and `paint` draws them directly.
        //
        // So the first frame after the switch back drew the spectrum of audio
        // that had already gone by — and the recovery was slower than it looks,
        // because the EMA is a time constant rather than a step: the frame clock
        // deliberately restarts its pacing with a neutral 1/60 s dt, so the
        // first analysed frame keeps 87 % of every bin whose true level had
        // FALLEN (`decay = 1 − exp (−dt / 0.12)` = 0.13 at 60 Hz), and the trace
        // reads high for ~0.12 s of VISIBLE time however long the switch was.
        // Measured against a view that was never hidden, over the real analyser
        // on the real rings: mean 2.1–3.6 dB per bin and up to 48 dB on one, for
        // hidden intervals from one frame to two seconds.
        //
        // The missing quantity is the same one the GR view needed — the seconds
        // this view did not tick for — so it is measured in the same place
        // (`HiddenInterval`), and what it MEANS here is this view's own EMA
        // arithmetic: `analyse` folds the current window in with `decay` taken
        // from the gap, so a switch of half a second or more re-anchors the
        // trace outright (decay > 0.98) while a brief one keeps exactly the
        // peaks a view that was never hidden would still be holding. It also
        // brings the reset floors forward: they live in `tick`, so before this
        // a re-prepare during the switch could paint the PRE-reset EMA through
        // the new rate's bin mapping for one frame — the very artefact the
        // rewind exists to remove.
        //
        // NOTHING IS INVENTED when no audio arrived while the view was away:
        // the idle gate at the top of `tick` sees both counts and both
        // generations unmoved and returns before `analyse`, so the trace is
        // held exactly as it was — which is also what a VISIBLE analyser does
        // with an idle ring (`KNOWN_ISSUES` KI-007 item 6). A resume that
        // floored or re-analysed unconditionally would be a change to that
        // listening-pass behaviour, not a fix to this one.
        tick (hidden.resumedSeconds (juce::Time::getMillisecondCounterHiRes()));
        clock.start (*this, [this] (double dt) { tick (dt); });
    }
    else
    {
        clock.stop();
        // Stamped AFTER the stop, so the interval measured is exactly the
        // interval in which no tick could run.
        hidden.stopped (juce::Time::getMillisecondCounterHiRes());
    }
}

// The mode switch's hit-area — since 0.1.1 the shared two-segment GR|SPEC
// pill (`abgui::graph_switch`, one definition for both views; it was a
// single-name corner chip before, and the spectrum's dismiss × before that;
// bottom-left and toggle-anywhere since 0.1.2, items 4+5).
// Expanded 2 px beyond the drawn pill on every side: the surplus is the touch
// target, and `hitTest` and `mouseDown` both key on this ONE rectangle so a
// click the view accepts but then ignores cannot creep back in.
juce::Rectangle<int> SpectrumView::chipHitArea() const noexcept
{
    return graph_switch::bounds (getWidth(), getHeight()).expanded (2);
}

// ONLY the chip is interactive. Leaving JUCE's default (hit-test true
// everywhere) made this view consume every click in the metering strip and
// do nothing with it — the one region of the editor that took a click with no
// affordance and no effect. `CurveView` opts out wholesale with
// `setInterceptsMouseClicks (false, false)`; this view cannot, because it owns
// the mode chip, so it opts out per-pixel instead, which is what `hitTest` is
// for — and `GrHistoryView` now does exactly the same for its mirrored "SPEC"
// chip. `LoudnessMeterView` stays intercepting because its WHOLE surface is the
// affordance (click = meter reset).
//
// TWO CONSEQUENCES, both inseparable from the fix rather than additions to it —
// `hitTest` is what decides membership of JUCE's "under the mouse" set, so
// declining a region declines EVERYTHING about it, not just the click.
//
//   1. THE TOOLTIP NARROWS to the chip. That is now the RIGHT scope rather
//      than a cost: the tooltip names the chip's action ("Switch to the
//      gain-reduction history", set in the constructor), so it belongs over the
//      chip and nowhere else. It was written the other way round when the
//      string was the identifier `"Spectrum"` and the wording was still an
//      open C8 owner TODO — the R2 item-11 directive discharged that TODO and
//      supplied the action wording, so there is no longer a brand-pass question
//      about widening it back. Widening would mean intercepting everywhere
//      again, i.e. re-accepting consequence 2 below, for a hint that would then
//      be wrong over the trace.
//
//   2. CLICKS OVER THE TRACE REACH WHATEVER IS BENEATH — today the editor
//      itself, which is the correct outcome and the point of the change, but it
//      is a live routing decision rather than a void. The editor installs no
//      tooltip on its background and no click handler under the graph well, so
//      nothing happens there now. The thing to know before ADDING one: anything
//      placed under this view's footprint becomes reachable through the trace
//      while this mode is showing. If a future affordance lands there and must
//      NOT be clickable through the trace, the answer is to widen this
//      hit-area — not to revert to intercepting everywhere, which would restore
//      the swallow this removed.
bool SpectrumView::hitTest (int x, int y)
{
    return chipHitArea().contains (x, y);
}

void SpectrumView::mouseDown (const juce::MouseEvent& e)
{
    // The test is unreachable-false — `hitTest` already refused every click
    // outside the area — and is kept rather than trimmed so this function is
    // correct standing alone instead of correct because of what another
    // function happens to return.
    if (! chipHitArea().contains (e.getPosition()))
        return;
    // The whole pill is ONE toggle (0.1.2 item 5): from the spectrum any
    // press inside it switches the well to the GR history. The 0.1.1
    // side-of-divider semantics made a press on the active segment a silent
    // no-op — the owner-reported "clicking SPEC does not switch back".
    processor.internalState.state().setProperty (iid::spectrumOn, false, nullptr);
}

void SpectrumView::analyse (const float* srcL, const float* srcR, int got,
                            std::vector<float>& smoothedDb, double dt)
{
    // THE WINDOW IS ALREADY IN HAND. `tick` reads both rings — same endpoint,
    // same length — and establishes that the pair survived the copies before
    // either is transformed; this function is what turns one of those windows
    // into a trace, and it decides nothing about which frames they are.
    if (got <= 0)
    {
        // A ZERO-LENGTH READ IS ITSELF A RESET ANNOUNCEMENT (round 7), and this
        // is the one place the rewind is observable through the load that
        // actually drives the display. `readEndingAt` clamps to
        // `min (committed head, acquired index, kSize)`, so `got == 0` is
        // EQUIVALENT to "the window I was asked for ends at 0" — reachable from
        // construction, from `reset()`, and (since 0.2.12) from the OTHER ring
        // having been rewound, which drags the common committed head to 0 and
        // is what stops one trace being drawn from a span the other cannot
        // honour. The caller's `resetObserved` cannot see this case when the
        // reset lands between its own `writeCount()` load and this one: the
        // count it compared was the pre-reset value, so it stayed silent while
        // this read came back empty. Flooring here is what stops the reader
        // holding two mutually inconsistent facts and acting on neither.
        //
        // No guard is needed and none is wanted. The construction arm reaches
        // this branch with `smoothedDb` already at the floor, so the fill is a
        // no-op there; and this is NOT the "should an idle analyser decay?"
        // question (`KNOWN_ISSUES` KI-007 item 6), which stays exactly as it
        // was: an idle ring returns a FULL window of stale frames and never
        // reaches this branch at all, and the idle case early-returns in `tick`
        // before `analyse` is called.
        std::fill (smoothedDb.begin(), smoothedDb.end(), -120.0f);
        return;
    }

    std::fill (fftData.begin(), fftData.end(), 0.0f);
    const int off = kSize - got;                          // zero-pad a short read
    for (int i = 0; i < got; ++i)
        fftData[(size_t) (off + i)] = 0.5f * (srcL[i] + srcR[i]);
    window.multiplyWithWindowingTable (fftData.data(), kSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    // Hann coherent gain 0.5; normalise so a full-scale sine reads ~0 dB.
    const float norm = 2.0f / ((float) kSize * 0.5f);
    // EMA per bin, dt-corrected (~120 ms), so the trace is readable without
    // hiding programme changes. Attack instant, decay smoothed — peaks show.
    const float decay = 1.0f - std::exp ((float) (-dt / 0.12));
    for (int b = 0; b < kBins; ++b)
    {
        const float mag = fftData[(size_t) b] * norm;
        const float db  = juce::Decibels::gainToDecibels (mag, -120.0f);
        float& s = smoothedDb[(size_t) b];
        s = db > s ? db : s + (db - s) * decay;
    }
}

void SpectrumView::tick (double dt)
{
    const anabasis::ScopeBuffer& in  = processor.spectrumInRing();
    const anabasis::ScopeBuffer& out = processor.spectrumOutRing();

    // THE CONFIGURATION THIS FRAME IS ABOUT, TAKEN ONCE AND UNDER THE EPOCH THAT
    // OWNS IT (0.2.12, the review's mismatched-axis finding). A trace is a row
    // of BIN indices; the sample rate is what turns a bin into a frequency, and
    // until this round `paint` read it for itself while the pair came from the
    // published frame — two independent reads of two different configurations,
    // free to disagree in one rendered frame.
    //
    // The pair is now published WITH its rate, and this is where the rate that
    // goes with it is chosen. Taken here rather than at publication time
    // deliberately: `AnabasisAudioProcessor::prepareToPlay` rewinds both
    // spectrum rings (`engine.prepare`) BEFORE it republishes the prepared pair
    // (`grHistoryRing.prepare`), so the epoch bracket around this read is what
    // makes "the rate this frame publishes" and "the frames that rate is
    // published with" one configuration rather than two — see
    // `configurationHeld`, which is the close of this bracket and the reader
    // contract `GrHistoryBuffer`'s banner states for any reader that maps
    // entries through the pair. `GrHistoryView` has always done this; this view
    // was on the other side of that banner's split and should not have been.
    const auto   cfgEpoch = processor.grHistory().resetEpoch();
    const double cfgRate  = processor.preparedSampleRate();
    // THE RESET IS ANNOUNCED, NOT INFERRED. `AnabasisEngine::prepare` rewinds
    // both rings so frames captured at the previous sample rate become
    // unreachable; making them unreachable is only half of it, because
    // `analyse` returns immediately when `readLatest` yields nothing — exactly
    // the post-rewind state — so `inDb`/`outDb` would keep the PREVIOUS
    // lifecycle's EMA and go on being drawn, the old analysis rendered against
    // the new rate's bin mapping, which is the artefact the rewind exists to
    // remove. The reader owns its smoothed copy, so the reader must drop it;
    // the ring cannot do it from the other side.
    //
    // This used to key on `writeCount()` going BACKWARDS *alone*, and that
    // predicate is weaker than the guarantee the comment claimed. (Round 7
    // re-admitted it as a SECOND sufficient condition beside the generation —
    // see `resetObserved`. What follows is why it cannot be the only one.) It holds only while the
    // observed count is still below the one the last tick stored: let the
    // producer republish past that value between two ticks — one tick delayed
    // past ~1 s of audio, a suspended message thread, a debugger stop — and the
    // rewind is missed OUTRIGHT, permanently, with no later tick able to notice,
    // because every subsequent count is larger again. The failure is silent and
    // its symptom is the exact artefact this code exists to prevent. Ordering
    // two counters cannot express "a reset happened"; a generation can, and
    // `GrHistoryBuffer` already answered the same question with an epoch rather
    // than a counter comparison. The rings now carry one too.
    //
    // Sampled on BOTH sides of the analysis batch, which is `resetEpoch()`'s
    // documented reader contract: the pre-batch sample catches a reset that
    // landed since the last tick, the post-batch one catches a reset that
    // landed DURING this tick's reads, whose frames may straddle two
    // configurations. Either way the answer is the same — drop to the floor and
    // re-anchor.
    //
    // WHAT THIS DOES AND DOES NOT GUARANTEE. Round 6 corrected this from an
    // outright claim to "BEST-EFFORT"; round 8 corrects it again, because
    // best-effort UNDERSTATES it in the one case that matters and a reader of
    // this comment could reasonably delete the post-batch re-read as decorative.
    //
    // FORCED, not best-effort, whenever `analyse` read a NON-ZERO post-reset
    // index: that value came from the audio thread's release store in
    // `pushBlock`, so it synchronises-with the acquire load in `readLatest`;
    // the host's generation bump happens-before that push by the named premise
    // in `ScopeBuffer`'s `reset()` (it runs from `prepare`, audio stopped);
    // happens-before is transitive and the `gi1` load below is sequenced after
    // the read, so write-read coherence FORCES `gi1` past the bump and the fill
    // runs. That is the leg that catches the partial-refill MIXTURE — a tick
    // where `resetObserved` is silent (the count has climbed back) and the
    // zero-length floor is silent (frames were returned) — and it destroys the
    // mixture inside the same tick, before `repaint`.
    //
    // BEST-EFFORT in the complementary case: if `analyse` read exactly 0, that
    // value came from `reset()` itself and the bump is SEQUENCED AFTER it, so
    // nothing forces `gi1`. That row is closed by the zero-length floor in
    // `analyse`, which is therefore not redundant with this re-read: each
    // closes what the other cannot.
    //
    // DETECTED PER RING, ANSWERED JOINTLY (0.2.12, the review's
    // cross-configuration finding). One `prepare` resets both rings in order, so
    // the in-ring's rewind orders nothing about the out-ring's and this re-read
    // really can catch one of the two alone. What USED to follow from that was a
    // floor on the ring it caught, which left `analyse (out, …)`'s pre-reset
    // frames folded into a trace published beside the other ring's post-reset
    // one. It no longer does: every floor below is joint, because a reset
    // observed on EITHER ring is proof that `AnabasisEngine::prepare` ran and
    // therefore that both rings were rewound. Detection stays per ring — a
    // rewind is a property of one ring's index — and the CONSEQUENCE is the
    // whole view's. What KI-018 still carries is the other direction: a
    // configuration whose bump neither ring has made visible yet is one this
    // view cannot see at all, and no flooring rule can answer that.
    //
    // What is guaranteed unconditionally: `shownInGen` only ever advances in a
    // tick that floored the EMA first (`resetIn`) or after (`gi1 != gi0`), and
    // a tick that saw the new generation is forced by `reset()`'s release
    // ordering to read a post-rewind index — so **a pre-reset spectrum can
    // never be committed as a post-reset one**.
    const auto gi0 = in.resetGeneration();
    const auto go0 = out.resetGeneration();
    const auto ci  = in.writeCount();
    const auto co  = out.writeCount();

    // ONE FRAME, ONE SPAN (0.2.12, from the review's split-publication finding —
    // which points at this region and has the mechanism backwards; the worklog
    // carries both). The two taps are published by ONE producer with one
    // release-store each, back to back inside `processChunk`, so THESE two
    // loads can straddle that window and read `ci > co`. That is not what
    // decided the frame, and it is not where the skew lives: until 0.2.12 each
    // `analyse` took its OWN acquire load of its OWN ring inside `readLatest`,
    // and the two are separated by a whole 4096-point FFT — 132 µs, measured —
    // during which the producer has every chance to publish a chunk to BOTH
    // rings. The skew that reaches the screen is therefore the one between the
    // two READS, it runs the OTHER way (the output trace leads, because its
    // read is the later one), and it is two orders of magnitude more common
    // than the store window: measured on the real processor with a real audio
    // thread, the two analysed windows described different spans on 1.28 % of
    // ticks at 48 kHz / 512 and 4.70 % at 128 — roughly once a second at 60 Hz —
    // against 0.015 % / 0.029 % for the count loads above. What that draws is
    // the input spectrum of chunk k beside the output spectrum of chunk k ± 1,
    // in a display whose entire purpose is comparing the two: a marker chunk
    // published to one ring alone reaches one trace at 87 dB in a bin the other
    // still reads as empty.
    //
    // The committed head is the newest frame index BOTH taps have published, so
    // a chunk one of them has published alone is simply not yet a state the
    // PAIR can represent; it is drawn on the first frame where both have. That
    // also answers the reveal question the finding is about: the hidden
    // interval's decay is applied to two traces that describe the same span and
    // the same seconds, which is what makes applying it to both correct.
    const uint64_t committed = juce::jmin (ci, co);

    // …AND A LENGTH BOTH RINGS CAN STILL SERVE (0.2.12, the review's large-block
    // finding). A shared endpoint is only half of a shared span: each ring holds
    // `capacity` frames, so it can serve `[w − capacity, w)` and no more, and a
    // ring whose head has run `capacity − kSize` = 12288 frames past the
    // committed endpoint has already taken back the oldest frame of the window
    // ending there. One chunk is enough to do it — `num` is the host's prepared
    // block with no upper clamp, and an offline render prepares whatever it
    // likes. Reproduced exactly at the boundary, one ring published and the
    // other not: 0 overwritten frames of the 4096 requested at a 12288-frame
    // chunk, 1 at 12289, 712 at 13000, 4095 at 16383, all 4096 at 16384 and
    // above.
    //
    // The pair's floor is the HIGHER of the two rings' — a span is common only
    // if BOTH still hold it — and the span is what is left between it and the
    // committed endpoint. Choosing it here rather than letting each read
    // shorten itself is what keeps the two windows the same length: `kSize` in
    // every configuration a real-time host presents (the shrink begins only
    // above a 12288-frame chunk, 256 ms at 48 kHz), shorter only where the
    // alternative was reading frames the producer had taken back, and zero when
    // nothing coherent is left, which is the one case this view refuses to draw.
    //
    // …AND THE FLOOR IS THE READER'S, NOT THE RING'S (`reservedFloor`). A push
    // in flight has already overwritten the slots it will publish, so the pair
    // gives up one prepared block of the oldest history it could otherwise have
    // claimed. `preparedBlockSize()` is the block half of the pair the plugin
    // already publishes at `prepare` — no second home for the same fact, and no
    // new store on the producer's path. Clamped from BELOW at 1 to mirror the
    // engine's own `jmax (1, maxBlockSize)`, and from above at `capacity`,
    // where the span reaches 0 and the frame holds.
    const int reserve = juce::jlimit (1, (int) anabasis::ScopeBuffer::capacity,
                                      processor.preparedBlockSize());
    const uint64_t floor = juce::jmax (reservedFloor (in .writeCount(), reserve),
                                       reservedFloor (out.writeCount(), reserve));
    const int span = committed > floor
                         ? (int) juce::jmin ((uint64_t) kSize, committed - floor)
                         : 0;

    // The generations join the idle test, or a reset landing on a tick with no
    // new frames would early-return past the clear below. The COUNTS left it
    // when the committed head arrived: "new frames" now means frames the pair
    // can be drawn from, so a tick that observes one ring alone move waits for
    // the other rather than redrawing an inconsistent pair — at most one chunk
    // later, and only on the observations that saw the split at all. A rewind
    // still cannot hide here: it drags the committed head DOWN (to 0 for a
    // `reset`), which differs from the shown one, and the generations catch a
    // refill that has returned the head to where it was.
    if (committed == shownCommitted
        && gi0 == shownInGen && go0 == shownOutGen)
        return;                                           // idle: nothing new

    // Both facets, both rings — see `resetObserved`. Before round 7 this keyed
    // on the generation ALONE, so a reader that had already observed the index
    // rewind but not yet the generation bump held the previous spectrum on
    // screen and, having committed `shownInCount = 0`, then satisfied the idle
    // test below on every subsequent tick and stopped looking.
    const bool resetIn  = resetObserved (gi0, shownInGen,  ci, shownInCount);
    const bool resetOut = resetObserved (go0, shownOutGen, co, shownOutCount);

    // EITHER RING'S RESET FLOORS BOTH TRACES (0.2.12, the review's
    // cross-configuration finding). This used to floor per ring — `resetIn` the
    // input trace, `resetOut` the output one — and that is what let a published
    // frame hold one trace from each configuration. A RESET IS NOT A PROPERTY OF
    // ONE RING: `AnabasisEngine::prepare` rewinds both, back to back and
    // unconditionally, so observing it on either ring is proof the configuration
    // changed and therefore that the OTHER trace's EMA is stale too — the EMA is
    // the only state in this view that survives a tick, and it is what carries
    // the previous configuration forward.
    //
    // MEASURED on the real processor with the reader holding one ring's reset
    // and not the other (`resetIn` true, `resetOut` false), across a genuine
    // `prepareToPlay` from 48 kHz to 96 kHz with a 5 kHz marker tone — bin 427
    // at the old rate, a frequency the new rate's mapping calls 10 kHz and the
    // audio has nothing at. At bin 427 the published pair read −120.0 dB (input,
    // floored and refilled from the new configuration) against −15.7 dB (output,
    // still the old configuration's EMA): 104.3 dB apart in ONE frame at a
    // 512-frame block, and 69.0 dB at 4096. After this change the same forced
    // ordering leaves 3.1 dB and 0.1 dB, both of them the ordinary difference
    // between a pre-chain and a post-chain tap near the floor — the control run
    // with no reconfiguration at all shows the same residual.
    //
    // HOW OFTEN IT ARRIVES ON ITS OWN, stated honestly: not once. A host thread
    // alternating 48/96 kHz against a ticking analyser reached 2006 re-prepares
    // over four eight-second runs and produced no cross-configuration frame
    // either before or after this change. Getting there needs the reader to
    // hold one of `AnabasisEngine::prepare`'s two back-to-back rewinds and not
    // the other, which is KI-018's propagation corner rather than an ordinary
    // interleaving. The defect is constructible, not frequent; it is fixed here
    // because a frame that mixes two configurations is wrong whenever it lands.
    //
    // Still only on the reset EDGE. This is not the "should an idle analyser
    // decay to the floor?" question — that is the early return above, it is a
    // listening-pass call, and it stays exactly as it was (`KNOWN_ISSUES`
    // KI-007 item 6). What changed is WHICH traces a reset floors, not when.
    bool configChanged = resetIn || resetOut;
    if (configChanged)
    {
        std::fill (inDb.begin(),  inDb.end(),  -120.0f);
        std::fill (outDb.begin(), outDb.end(), -120.0f);
    }

    // NOTHING COHERENT LEFT: the producer has taken back every frame of the
    // window both taps share, which needs a chunk of a whole ring (16384 frames,
    // 341 ms at 48 kHz) landing between the two publications. The last coherent
    // pair stays on screen for this tick — the split closes on the producer's
    // next store and the following tick draws the new span in full. Flooring
    // instead would put silence on screen where there is audio, and reading
    // anyway would put one trace's newest chunk where the other's history is.
    // `committed == 0` is NOT this case: the rings are empty or rewound, and the
    // zero-length read is how that is already expressed (`analyse` floors).
    if (span == 0 && committed > 0)
    {
        // …UNLESS THE PAIR THIS FRAME WOULD HOLD IS THE PREVIOUS CONFIGURATION'S
        // (0.2.12, the review's large-block-reset finding). "Hold the last
        // coherent pair" is only an answer while the held pair still describes
        // the audio the view is looking at. A re-prepare invalidates it
        // outright — the bins mean different frequencies now — and where the
        // host's block is at least a whole ring the span is 0 not for one tick
        // but for EVERY tick that follows, because `reservedFloor` puts the
        // floor at or past the head at that size. The EMA above is floored on
        // the reset edge and then this return skipped the publication, so the
        // floored trace never reached the screen: the previous rate's spectrum
        // stayed drawn, under the new rate's mapping, indefinitely.
        //
        // So the reset publishes. Not a fabricated frame — an EMPTY one: the
        // floor in both traces and a zero-length window, which is exactly the
        // state this view has always shown before its first frame, and the
        // honest answer to "what does the new configuration look like so far?".
        // The accounting is committed with it (generations, counts, head), for
        // the same reason the drawn path commits: a reset that has been answered
        // must not be answered again on every tick from here on.
        //
        // Scoped to the reset EDGE deliberately. Without one, a span of 0 is the
        // lapping case a paragraph up — the traces are still the current
        // configuration's, they are still coherent, and blanking them there
        // would put silence on screen where there is audio.
        if (resetIn || resetOut)
        {
            // …and the configuration bracket closes BEFORE any of it. A tick
            // that straddled a clear must commit NOTHING — committing the
            // generations here while discarding the frame would mark this reset
            // answered without ever answering it, and at a block of a whole ring
            // this is the last publication that would ever happen, so the blank
            // would never arrive. Held whole, retried next tick.
            if (! configurationHeld (cfgEpoch))
                return;
            // THE GENERATIONS ARE RE-SAMPLED, not carried down from the top of
            // the tick. `gi0`/`go0` are loaded before the two counts and so
            // inherit none of the ordering a post-reset count establishes: one
            // of them can be the generation the ring has already left. Marking a
            // reset "answered" against a generation the ring never had is a
            // bookkeeping error that pays for itself twice — the next tick fires
            // `resetObserved` again for a reset already answered, floors again,
            // and advances the identity a second time for ONE reconfiguration.
            //
            // Re-sampling is sound precisely because this branch publishes the
            // FLOOR: an empty frame belongs to every configuration equally, so
            // committing the newest generations either ring can show says no
            // more than "every reset visible at this instant is answered by the
            // blank about to be published", which is exactly true. The drawn
            // path below needs no equivalent — it commits `gi1`/`go1`, which are
            // sequenced after the reads and are forced past the bump whenever
            // the read returned anything at all.
            shownInGen     = in .resetGeneration();
            shownOutGen    = out.resetGeneration();
            shownInCount   = ci;
            shownOutCount  = co;
            shownCommitted = committed;
            drawnFirst     = committed;
            drawnSpan      = 0;
            drawnRate      = cfgRate;
            drawnConfig    = ++configSeq;
            publishFrame (committed, 0, cfgRate, drawnConfig);
            repaint();
        }
        return;
    }

    // BOTH WINDOWS FIRST, THEN THE PROOF, THEN THE TRANSFORMS (0.2.12, the
    // review's concurrent-publication finding). The span above is chosen from a
    // SNAPSHOT of the two floors, and a snapshot is not a lock: the producer can
    // publish between it and the first read, between the two reads, or during
    // either copy. When it does, `readEndingAt` protects each ring on its own —
    // it clamps its start to that ring's floor at its own load — and protecting
    // each ring on its own is exactly what breaks the pair: ONE read comes back
    // short and the frame draws two different spans again, which is the defect
    // the shared endpoint exists to prevent. So the reads happen while nothing
    // is committed, the pair is proved with both windows in hand
    // (`onePairOneSpan`: both served the whole span, and neither ring's floor
    // has passed its start), and only then are the two transforms run.
    //
    // A frame that cannot prove it is HELD, not repaired: nothing is folded into
    // either EMA, nothing is committed, and the next tick re-derives from a
    // settled producer. There is no retry loop — the invalidation needs the
    // producer to publish `capacity − span` frames inside two 4096-frame copies,
    // so a retry would be a second draw of the same lottery on a thread that has
    // a frame to paint, and the frame it would save is one 60th of a second old.
    const uint64_t first  = committed - (uint64_t) span;
    const int      gotIn  = in .readEndingAt (scratchInL .data(), scratchInR .data(), span, committed);
    const int      gotOut = out.readEndingAt (scratchOutL.data(), scratchOutR.data(), span, committed);
    if (! onePairOneSpan (span, gotIn, gotOut, first,
                          reservedFloor (in .writeCount(), reserve),
                          reservedFloor (out.writeCount(), reserve)))
        return;

    analyse (scratchInL .data(), scratchInR .data(), gotIn,  inDb,  dt);
    analyse (scratchOutL.data(), scratchOutR.data(), gotOut, outDb, dt);

    // The second sample. A generation that moved while the batch ran means the
    // frames just folded into the EMA may span the rewind, so the EMA is not a
    // description of either configuration — the post-reset state is the floor,
    // which is what a reset leaves in any case.
    const auto gi1 = in.resetGeneration();
    const auto go1 = out.resetGeneration();
    if (gi1 != gi0 || go1 != go0)
    {
        // BOTH, for the reason above: a generation that moved on either ring is
        // a reconfiguration, and the frames just folded into the OTHER trace
        // belong to the configuration it ended. Flooring one of the two here was
        // the same defect one step later in the tick.
        configChanged = true;
        std::fill (inDb.begin(),  inDb.end(),  -120.0f);
        std::fill (outDb.begin(), outDb.end(), -120.0f);
    }

    // …AND THE CONFIGURATION HAS TO HAVE HELD ACROSS ALL OF IT. Everything above
    // proves the two traces describe one span of audio; this proves that span
    // belongs to the rate about to be published beside it. Held, not repaired,
    // and held BEFORE anything is committed: a tick that spanned a re-prepare
    // leaves every `shown*` where it was, so the next tick re-derives from a
    // settled configuration instead of inheriting this one's half-answer.
    if (! configurationHeld (cfgEpoch))
        return;

    shownInGen   = gi1;
    shownOutGen  = go1;
    shownInCount = ci;
    shownOutCount = co;
    // What this frame actually drew from, which is what the next tick's idle
    // test compares against. The per-ring counts above stay for `resetObserved`
    // — a rewind is a property of ONE ring's index, and the coherence argument
    // behind the count term is about that ring's modification order.
    shownCommitted = committed;
    drawnFirst     = first;
    drawnSpan      = span;
    drawnRate      = cfgRate;
    // THE IDENTITY OF THE CONFIGURATION THESE TRACES BELONG TO, and it advances
    // exactly when this view answers a reset. Not the GR ring's epoch — that
    // does not move on a re-prepare at an unchanged (rate, block) pair, which
    // still rewinds both spectrum rings — and not either ring's generation,
    // because there are two of them and a frame needs ONE identity. This counter
    // is what the view itself has accounted for, which is precisely the
    // generation of the content it is about to publish.
    if (configChanged) ++configSeq;
    drawnConfig = configSeq;
    // …and only now does the frame become visible. Publication is the LAST thing
    // the tick does, after both EMAs are settled and both post-batch generation
    // checks have had their say, so the pair the renderer can pick up is one
    // this tick was willing to commit to — never a half-updated one it was
    // about to floor.
    publishFrame (first, span, cfgRate, drawnConfig);
    repaint();
}

// THE CLOSE OF THE CONFIGURATION BRACKET, and it is `GrHistoryView`'s reader
// contract verbatim (`GrHistoryView.cpp` `paintHistory`, and the two-discipline
// rule at `GrHistoryBuffer::prepared`): an ODD sample was taken inside a clear
// and describes a half-written pair, and a value that has MOVED means a clear
// overlapped the tick. Either way the frame is HELD.
//
// WHAT IT IS FOR. The rate and the frames live in DIFFERENT objects — the rate
// in `GrHistoryBuffer`, the frames in two `ScopeBuffer`s — and a relaxed load of
// one is unordered against an acquire load of the other. `prepareToPlay` writes
// them in one thread in one sequence (`engine.prepare` rewinds both rings at
// `PluginProcessor.cpp:769`, `grHistoryRing.prepare` republishes the pair at
// `:785`), and this bracket is what lets a reader use that sequence.
//
// THE CASES ARE A SPLIT OVER WHERE `epoch0` FELL IN `resetGuard`'s MODIFICATION
// ORDER, which is what makes them exhaustive — not over the direction of the
// mismatch, which is not a partition:
//   (i)  `epoch0` ODD. Rejected outright, before `batchIntact` is consulted at
//        all. The odd increment is a RELAXED RMW, so an acquire load that takes
//        it synchronises with the PREVIOUS clear, not this one — it carries no
//        ordering against the rewinds, and `batchIntact` would compare it equal
//        to itself and pass. This test is the whole of that case.
//   (ii) `epoch0` EVEN and BEFORE the clear. Two sub-cases, both forced:
//        * the tick read the NEW rate — that load read a store made INSIDE the
//          clear window, and it is sequenced before the acquire fence
//          `batchIntact` carries, so the seqlock reader rule (Boehm, MSPC 2012 —
//          the paper `GrHistoryBuffer` cites for exactly this) forces the
//          re-read to observe at least that clear's odd increment;
//        * the tick's ACQUIRED RING INDICES were post-rewind — those frames are
//          pushed only after `prepareToPlay` returned, so the clear's even
//          release RMW happens-before the pushes, which happen-before this
//          tick's acquire of the index that revealed them, which is sequenced
//          before the re-read; coherence forbids the re-read returning anything
//          earlier.
//        Either way the value has moved and the frame is held.
//   (iii) `epoch0` EVEN and AFTER the clear. Nothing to catch, and nothing that
//        needs catching: `epoch0` is then a value written by a RELEASE RMW and
//        read by an ACQUIRE load, so everything sequenced before it — the rate
//        store AND both rings' `write.store (0, release)` — happens-before every
//        later load in this tick.
//
// A NAMED PREMISE, in the words `ScopeBuffer::reset` already uses for the same
// thing: the host does not deliver audio across `prepareToPlay`. It is a
// plugin-API contract, not a C++ guarantee, and case (ii)'s second sub-case
// rests on it. It also covers the gap this bracket cannot see into — the
// milliseconds between the ring rewinds and the clear, spent constructing the
// oversamplers — where `epoch0` and the rate are both the OLD configuration's
// and the rings are already rewound: there is no new-rate audio yet to mis-map,
// and the rings report `committed == 0`.
//
// WHAT THIS DOES *NOT* CLAIM, stated because the shorter version is wrong in two
// ways a reader would not otherwise find:
//   * IT IS AN ANNOUNCEMENT FOR THE RATE, NOT FOR THE RING RESET.
//     `GrHistoryBuffer::prepare` clears only when the (rate, block) pair
//     CHANGED, while `AnabasisEngine::prepare` rewinds both spectrum rings
//     UNCONDITIONALLY — so the ordinary transport-start re-prepare at an
//     unchanged pair rewinds the rings with the epoch standing still. That case
//     cannot move the rate, which is all this bracket is about, and the rings'
//     own `resetGeneration` remains the SOLE detector for it (`resetObserved`,
//     and the second sample after the batch). Nothing here subsumes that.
//   * IT PAIRS THE RATE WITH THE FRAMES THIS TICK'S ACQUIRED INDICES DESCRIBE,
//     not with every sample the EMA remembers. The payload itself is read
//     through relaxed loads, and `KNOWN_ISSUES` KI-018's cross-ring residual —
//     one ring's rewind observed and the other's not, for one tick — is
//     unchanged: that tick floors the ring it saw and can fold the other's
//     pre-reset frames into an EMA published beside the new rate. One tick,
//     ~16.7 ms, decaying on the 120 ms EMA, and the next tick floors it. The
//     residual is bounded, not removed, and saying so is the point.
//
// CONSERVATIVE IN ONE DIRECTION, deliberately: a block-size-only re-prepare
// moves the epoch and holds one frame whose rate did not change. One frame at
// 60 Hz, on an event that already blanks the display.
bool SpectrumView::configurationHeld (uint32_t epoch0) const noexcept
{
    return (epoch0 & 1u) == 0u && processor.grHistory().batchIntact (epoch0);
}

// ONE PUBLICATION, BOTH TRACES. The bracket is the ordinary sequence-counter
// form and every step of it is load-bearing:
//   * the counter goes ODD first, so a reader that starts mid-write sees an odd
//     value and knows it before it copies anything;
//   * a RELEASE FENCE follows that store rather than the store carrying release
//     itself — release orders what came BEFORE it, and what has to be ordered
//     here is what comes after, namely the payload stores, which must not sink
//     above the odd marker;
//   * the payload is relaxed, because the counter carries the ordering and the
//     atomics are there to make the concurrent access defined rather than to
//     synchronise anything on their own;
//   * the counter goes EVEN with RELEASE, which is what publishes every payload
//     store to the reader that acquires the same value.
// The counter is only ever written here, on the message thread, so the plain
// load/store pair needs no read-modify-write.
void SpectrumView::publishFrame (uint64_t first, int span, double rate, uint32_t config) noexcept
{
    const auto s = frameSeq.load (std::memory_order_relaxed);
    frameSeq.store (s + 1, std::memory_order_relaxed);
    std::atomic_thread_fence (std::memory_order_release);

    for (int b = 0; b < kBins; ++b)
        pubIn [(size_t) b].store (inDb [(size_t) b], std::memory_order_relaxed);
    for (int b = 0; b < kBins; ++b)
        pubOut[(size_t) b].store (outDb[(size_t) b], std::memory_order_relaxed);
    pubFirst.store (first, std::memory_order_relaxed);
    pubSpan .store (span,  std::memory_order_relaxed);
    pubRate .store (rate,  std::memory_order_relaxed);
    pubConfig.store (config, std::memory_order_relaxed);

    frameSeq.store (s + 2, std::memory_order_release);
}

// …AND THE MIRROR OF IT. Read the counter, copy, read it again: an odd first
// value or a changed second one means a tick was inside the bracket and the
// copy may hold bins from two frames, so it is DISCARDED rather than drawn. The
// acquire fence before the second load is the counterpart of the writer's
// release fence — it stops the payload loads floating past the check that is
// meant to validate them.
//
// TWO ATTEMPTS, then give up. A tick publishes 4098 relaxed stores at 60 Hz;
// losing twice in a row means being overtaken twice inside that window, and the
// caller's answer — keep the frame it already has — costs one repeated frame at
// 16.7 ms. An unbounded spin here would be a spin on the render thread, which is
// the one place a stall is visible as a dropped frame rather than a stale one.
bool SpectrumView::readPublishedFrame (std::vector<float>& inTrace,
                                       std::vector<float>& outTrace,
                                       Frame& frame) const noexcept
{
    // The three SCALARS are staged in locals below and committed only on
    // success. The 4096-bin payload cannot be — that is the caller's job, and
    // the header states it as the contract rather than leaving it to be found.
    if (inTrace.size() != (size_t) kBins || outTrace.size() != (size_t) kBins)
    {
        jassertfalse;   // the caller owns the storage and must size it once
        return false;
    }

    for (int attempt = 0; attempt < 2; ++attempt)
    {
        const auto s0 = frameSeq.load (std::memory_order_acquire);
        if ((s0 & 1u) != 0u)
            continue;                                   // a tick is mid-publication

        for (int b = 0; b < kBins; ++b)
            inTrace [(size_t) b] = pubIn [(size_t) b].load (std::memory_order_relaxed);
        for (int b = 0; b < kBins; ++b)
            outTrace[(size_t) b] = pubOut[(size_t) b].load (std::memory_order_relaxed);
        const auto first = pubFirst.load (std::memory_order_relaxed);
        const auto span  = pubSpan .load (std::memory_order_relaxed);
        const auto rate  = pubRate .load (std::memory_order_relaxed);
        const auto cfg   = pubConfig.load (std::memory_order_relaxed);

        std::atomic_thread_fence (std::memory_order_acquire);
        if (frameSeq.load (std::memory_order_relaxed) == s0)
        {
            frame = { first, span, rate, cfg };
            return true;
        }
    }
    return false;
}

void SpectrumView::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (10.0f, 8.0f);

    // THE PAIR THE RENDERER DRAWS IS ONE TICK'S, WHOLE — and so is the rate it
    // reads the bins through. On macOS and Windows this function runs on the
    // OpenGL context's render thread, not the message thread that ticks
    // (`THREAD_MODEL`, "Which context paints"), so the traces are taken through
    // the published bracket into buffers only this function touches. Nothing
    // here allocates: every vector is `kBins` long from construction.
    //
    // THE READ LANDS IN THE STAGING PAIR, AND ONLY A SUCCESSFUL ONE IS COMMITTED
    // (0.2.12, round 11). `readPublishedFrame` cannot preserve its output on
    // failure — the 4096-bin copy has already happened by the time the bracket
    // can be checked — so reading straight into `paintIn`/`paintOut` and
    // discarding the result, which is what this line used to do, drew a mix of
    // up to three publications' bins through the PREVIOUS frame's rate. That is
    // the defect this whole publication exists to prevent, arriving through the
    // reader rather than the writer, and no sanitizer can see it: there is no
    // race in it, only a broken invariant. Staged and swapped, a lost read now
    // leaves the previous frame exactly where it was — which is what the header
    // and ADR-0039 always claimed — and a successful one costs two pointer
    // exchanges rather than a second copy.
    if (readPublishedFrame (stageIn, stageOut, stageFrame))
    {
        paintIn .swap (stageIn);
        paintOut.swap (stageOut);
        paintFrame = stageFrame;
    }

    // KI-017's rule, and since 0.2.12 the FRAME's rate rather than a second read
    // of the processor's. Reading `preparedSampleRate()` here was correct about
    // the data race — it is the published pair, not `getSampleRate()`'s plain
    // member — and wrong about the pairing: it is a different configuration's
    // answer from the one the bins came from whenever a re-prepare falls between
    // the tick and the paint. The bins and the rate now arrive together or not
    // at all, and `paint` reads no processor state at all (see `Frame`).
    // The fallback stays exactly where it was and for the same reason: a view
    // that has never published carries rate 0 — as JUCE's member does before the
    // first `prepareToPlay` — and `binHz = 0` would make `fa / binHz` infinite
    // and its conversion to `int` undefined, two lines further down. The
    // resolved value is written BACK into the frame, so `paintedFrame().rate` is
    // by construction the rate this paint actually mapped bins through, which is
    // what lets a test tell a renderer reading the published rate from one
    // reading the processor's behind the frame's back.
    const double sr = paintFrame.rate > 0.0 ? paintFrame.rate : 48000.0;
    paintFrame.rate = sr;
    const float fLo = 20.0f, fHi = 20000.0f;
    const float dbLo = -90.0f, dbHi = 0.0f;

    // Column reads: the two-regime rule adapted from Anamorph's SpectrumImager
    // (ADR-0009 provenance: Anamorph src/gui/SpectrumImager.cpp, `magCubic` /
    // `magForColumn`). On a log-f axis the low end packs many pixel columns
    // into few FFT bins, so the nearest-bin read this replaces quantised the
    // LF trace into a staircase. Instead, each column spans [cx−½, cx+½]:
    // where that covers fewer than 1.5 bins, the value is a Catmull-Rom
    // interpolation across the four surrounding bins; where it covers 1.5 bins
    // or more (the HF end, many bins per column), it averages every covered
    // bin.
    //
    // BOTH REGIMES WORK IN THE dB DOMAIN, AND SO DOES THE SIBLING — this is a
    // faithful port, not an adaptation. Worth stating outright because the
    // sibling's names say otherwise and have now misled a review: its array is
    // called `mags` and its readers `magCubic`/`magForColumn`, but the array
    // holds **decibels**, not linear magnitudes. The evidence, all in
    // `Anamorph:src/gui/SpectrumImager.cpp`: it is seeded to `kMinDb` (−90) at
    // `:105`; it is an attack-instant / release-decayed EMA of `magsDb`, which
    // is itself `Decibels::gainToDecibels (fftData[k] * norm, kMinDb)` at
    // `:796`; the clip-glow reader adds 6 dB to it as "window-gain compensated
    // dBFS" at `:833`; and `magForColumn`'s result goes straight into
    // `dbToY (…)` at `:1132` with no conversion on the way. A dB-domain mean
    // IS a geometric mean of magnitudes, and it is the one the sibling has
    // always drawn — so matching it is what preserves the family's display
    // behaviour, and converting to linear here to "fix" the domain would be
    // the divergence.
    //
    // An earlier revision of this comment claimed the sibling averaged linear
    // magnitudes and that reading dB was an adaptation. That was wrong on both
    // halves and is corrected rather than deleted, because the wrong version is
    // what a reviewer read and repeated.
    //
    // The inclusive `[ka, kb]` span below is likewise the sibling's, verbatim
    // (`floor` / `ceil` / `k <= kb` at `:685-689`): it reaches up to one bin
    // past each edge of the column, which is deliberate overlap between
    // neighbouring columns, not an off-by-one. A half-open form would drop the
    // overlap and diverge.
    //
    // The clamp floor stays at bin 1 — the nearest-bin read this replaces never
    // showed DC and this port keeps that exclusion.
    const float binHz = (float) (sr / (double) kSize);
    auto binAt = [&] (const std::vector<float>& bins, int j)
    {
        return bins[(size_t) juce::jlimit (1, kBins - 1, j)];
    };
    auto dbCubic = [&] (const std::vector<float>& bins, float binPos)
    {
        const int   i = (int) std::floor (binPos);
        const float u = binPos - (float) i;
        const float m0 = binAt (bins, i - 1), m1 = binAt (bins, i);
        const float m2 = binAt (bins, i + 1), m3 = binAt (bins, i + 2);
        return 0.5f * ((2.0f * m1) + (-m0 + m2) * u
                       + (2.0f * m0 - 5.0f * m1 + 4.0f * m2 - m3) * u * u
                       + (-m0 + 3.0f * m1 - 3.0f * m2 + m3) * u * u * u);
    };
    auto dbForColumn = [&] (const std::vector<float>& bins, float fa, float fb)
    {
        const float span = (fb - fa) / binHz;
        if (span < 1.5f)
            return dbCubic (bins, 0.5f * (fa + fb) / binHz);
        const int ka = juce::jlimit (1, kBins - 1, (int) std::floor (fa / binHz));
        const int kb = juce::jlimit (1, kBins - 1, (int) std::ceil  (fb / binHz));
        float sum = 0.0f;
        for (int k = ka; k <= kb; ++k)
            sum += bins[(size_t) k];
        return sum / (float) (kb - ka + 1);
    };

    auto traceOf = [&] (const std::vector<float>& bins, juce::Path& path)
    {
        bool started = false;
        const int cols = juce::jmax (1, (int) area.getWidth());
        auto freqAt = [&] (float cx)
        {
            return fLo * std::pow (fHi / fLo, cx / (float) cols);
        };
        for (int cx = 0; cx <= cols; ++cx)
        {
            const float t  = (float) cx / (float) cols;
            const float fa = freqAt ((float) cx - 0.5f);
            const float fb = freqAt ((float) cx + 0.5f);
            const float db = juce::jlimit (dbLo, dbHi, dbForColumn (bins, fa, fb));
            const float x = area.getX() + t * area.getWidth();
            const float y = area.getBottom()
                          - (db - dbLo) / (dbHi - dbLo) * area.getHeight();
            if (! started) { path.startNewSubPath (x, y); started = true; }
            else           path.lineTo (x, y);
        }
    };

    juce::Path pin, pout;
    traceOf (paintIn, pin);
    traceOf (paintOut, pout);
    g.setColour (colours::textDim.withAlpha (0.55f));
    g.strokePath (pin, juce::PathStrokeType (1.0f));
    g.setColour (colours::accent);
    g.strokePath (pout, juce::PathStrokeType (1.3f));

    // The shared SPEC|GR mode switch, drawn LAST so it floats over the traces
    // (it is translucent, so what it overlaps stays readable).
    graph_switch::paint (g, getWidth(), getHeight(), true);
}
