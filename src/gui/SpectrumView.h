#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "LookAndFeel.h"
#include "FrameClock.h"
#include "HiddenInterval.h"
#include "../dsp/ScopeBuffer.h"

#include <atomic>
#include <functional>

class AnabasisAudioProcessor;

// ============================================================================
//  SpectrumView — the §2.9 dual-trace input/output analyser, and one of the
//  TWO MODES of the shared graph well (`int_spectrumOn` true selects it,
//  false selects `GrHistoryView`; ADR-0016). It was brief §6's dismissible
//  overlay — "visible until dismissed" — until 2026-08-05, when the owner's
//  round-2 directive made the well a two-view switch: the corner chip now
//  names the view it swaps TO rather than dismissing anything, because the
//  well always shows one of the pair.
//
//  The audio thread only fills the two ScopeBuffer rings (engine taps:
//  post-input-gain and post-chain); the FFT runs HERE, on the paint side, per
//  the ADR-0011 / THREAD_MODEL division. Reads are stateless `readLatest`
//  peeks; a frame with no new samples repaints nothing.
//
//  Display: 4096-point Hann FFT, mono-summed, log-f 20 Hz–20 kHz, −90..0 dB,
//  per-bin EMA smoothing so the trace holds still enough to read. The input
//  trace draws dim, the output in the accent — the same two-material rule
//  the meters use.
// ============================================================================

class SpectrumView : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    explicit SpectrumView (AnabasisAudioProcessor&);
    // The clock is DETACHED FIRST, not left to reverse-order destruction. Its
    // tick reads `scratchL`/`fftData`/`inDb`/`outDb` and the `shown*` counters,
    // all declared AFTER `clock`, so `= default` freed them while the vblank
    // attachment was still armed over them. Unreachable today — a message-thread
    // destructor cannot interleave with a message-thread vblank callback — but
    // that is the "safe by ordering" argument `~AnabasisAudioProcessorEditor`
    // refuses to rely on for its own `animVBlank`, and this is the same thing
    // one class down. Stating it also means a future member reorder cannot
    // quietly take the guarantee away. `FrameClock::stop()` is idempotent.
    ~SpectrumView() override { clock.stop(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;   // bottom-left pill → GR history
    // Interactive ONLY over that chip; everything else falls through to whatever
    // is beneath. See the definition — this is how a partly-interactive overlay
    // opts out. `GrHistoryView` mirrors it for its own chip; `CurveView` opts
    // out wholesale.
    bool hitTest (int x, int y) override;
    void visibilityChanged() override;

    static constexpr int kOrder = 12;                    // 4096-point FFT
    static constexpr int kSize  = 1 << kOrder;
    static constexpr int kBins  = kSize / 2;

    // PUBLIC only so the headless suite can drive it: the `FrameClock` needs a
    // vblank that never arrives there, so the analyser's one behavioural edge —
    // a ring rewound by a re-prepare must drop the trace it can no longer
    // justify — would otherwise have no way to be exercised. Same reasoning as
    // `AnabasisAudioProcessorEditor::refreshInternalSettingsBoxes` and
    // `MacroEngine::drainTick`: a direction nothing can call is a direction
    // nothing can guard.
    void tick (double dt);

    // EVERYTHING A RENDERER NEEDS TO INTERPRET THE PAIR, and it is deliberately
    // one struct rather than three facts a frame has to re-collect. `first` and
    // `span` are the range of ring frames both traces were analysed over;
    // `rate` is the sample rate those frames were captured at, which is what
    // turns a bin index into a frequency (`paint`'s `binHz = rate / kSize`).
    //
    // …AND `config` IS THE IDENTITY OF THE CONFIGURATION THE TRACES BELONG TO.
    // It advances when this view answers a reset, which is what makes a frame
    // self-describing: the pair, the span, the rate and the generation they were
    // all produced under, published together or not at all. It is deliberately
    // NOT the GR ring's reset epoch — that does not move on a re-prepare at an
    // unchanged (rate, block) pair, which still rewinds both spectrum rings —
    // and deliberately not either `ScopeBuffer`'s generation, because there are
    // two of those and a frame needs ONE identity.
    //
    // WHAT IT PROMISES, EXACTLY, because a serial invites the stronger reading:
    // two frames carrying the SAME id were produced with no reset observed
    // between them, so they describe one configuration. The converse is NOT
    // promised — a single reconfiguration may be answered on one ring and then
    // the other, and each answer advances the id — so a consumer may see one
    // configuration split across two ids. That direction is the safe one (it
    // over-reports change, never under-reports it), and the tick keeps it rare
    // by committing re-sampled generations on the blank branch rather than the
    // pre-count samples. The corner a serial cannot reach at all is a
    // configuration NEITHER ring has made visible: `KNOWN_ISSUES` KI-018.
    //
    // THE RATE IS PART OF THE FRAME BECAUSE THE BINS MEAN NOTHING WITHOUT IT.
    // This is ADR-0038 clause 7 — "a published display estimate carries the
    // identity of the state it describes" — applied to a payload rather than to
    // a scroll phase, and it was the round-11 review finding: `paint` used to
    // take the pair from the published frame and the rate from
    // `AnabasisAudioProcessor::preparedSampleRate()` on its own, so a rendered
    // frame could hold one configuration's trace against another's bin mapping.
    // MEASURED at 6 kHz, which sits at bin 512 at 48 kHz and at bin 256 at
    // 96 kHz: the tone read −0.00 dB paired correctly, −116.80 dB as an old
    // trace under the new rate, and −120.00 dB as a new trace under the old one
    // — the tone gone from the display outright, either way.
    //
    // Public for the reason the two trace accessors are: what a frame has to
    // get right is a property of the WHOLE frame, and a test that can only see
    // the pixels cannot say which state the pair was drawn from.
    struct Frame
    {
        uint64_t first  = 0;
        int      span   = 0;
        double   rate   = 0.0;
        uint32_t config = 0;
    };
    Frame lastFrame() const noexcept { return { drawnFirst, drawnSpan, drawnRate, drawnConfig }; }

    // …AND THE FRAME THE LAST PAINT ACTUALLY DREW, which is a different fact on
    // a different thread. `lastFrame` is what the message thread committed;
    // this is what the renderer picked up through `readPublishedFrame`, and the
    // two are only ever equal because the publication makes them so. A test that
    // could see only the first could not tell a renderer reading the published
    // frame from one reading the tick's working vectors — nor one taking the
    // rate from the processor behind the frame's back.
    Frame paintedFrame() const noexcept { return paintFrame; }

    // …and the TRACE that paint drew, for the same reason one step further: the
    // frame description and the bins have to agree, and only a reader that can
    // see both can say so. Painter-owned, so it is safe to read from the thread
    // that painted (which is what the concurrent-paint test does) or from a
    // quiesced view.
    const std::vector<float>& paintedInDb() const noexcept { return paintIn; }

    // THE PUBLISHED PAIR, READ AS A PAIR. `tick` runs on the message thread and
    // `paint` does NOT, on two of the three shipped hosts: JUCE renders an
    // OpenGL-backed editor on the context's own render thread (macOS, Windows —
    // `THREAD_MODEL`, "Which context paints"), so the painter is a SECOND THREAD
    // reading state the message thread writes. JUCE happens to serialise the two
    // with the message-manager lock on that path — measured at the pinned 9.0.1,
    // and recorded in `THREAD_MODEL` — which is a property of a vendored
    // renderer, not of this class: it is absent from the other callers of
    // `paint`, it can move with the pin, and "safe because something else holds
    // a lock" is the reasoning this tree refuses two lines further down in this
    // very file. Two
    // separate `std::vector<float>` traces gave it neither guarantee it needs:
    // the accesses were an unsynchronised float race outright, and even where
    // the hardware made them benign a frame could take `inDb` from tick N and
    // `outDb` from tick N + 1 — the exact split the shared committed head was
    // introduced to prevent, re-entering one layer later, in a display whose
    // entire purpose is comparing the two traces.
    //
    // The pair is therefore PUBLISHED, in one bracketed store, and read back
    // whole or not at all. `false` means the reader was overtaken twice and has
    // no new frame — the caller keeps the pair it already had, which is a
    // coherent frame from an earlier tick rather than a mixed one from two. The
    // `frame` is filled only on `true`. `inTrace` and `outTrace` must already
    // hold `kBins` entries, and on `false` their contents are INDETERMINATE —
    // the 4096-bin copy has already happened by the time the bracket can be
    // checked, so a lost read leaves them holding bins from more than one
    // publication. The caller therefore reads into storage it is willing to
    // throw away and commits only on `true`; `paint` stages into
    // `stageIn`/`stageOut` and swaps. Round 11 found this documented the other
    // way round, with `paint` discarding the result — which drew a mix of up to
    // three publications' bins through the PREVIOUS frame's rate, exactly the
    // defect this publication exists to prevent, and one no sanitizer can see
    // because there is no race in it, only a broken invariant.
    //
    // Public because the coherence it provides is exactly what a test has to be
    // able to observe from ANOTHER THREAD — the same reasoning as the two
    // `analysed*Db` accessors, one step further out: those report the message
    // thread's own state, and the property at stake here is what a second
    // thread can see.
    bool readPublishedFrame (std::vector<float>& inTrace,
                             std::vector<float>& outTrace,
                             Frame& frame) const noexcept;

    // ── THE THREE RENDEZVOUS POINTS, AND WHY THEY ARE HERE ──────────────────
    //
    // A test cannot establish an interleaving it can only observe. Everything
    // this publication is FOR happens inside two functions and one tick: a
    // reader that lands while the counter is odd, a reader whose copy is
    // overtaken, a rewind that becomes visible between a tick's two generation
    // samples. None of those states is DETERMINISTICALLY reachable from
    // outside the class — an incidental race reaches them, which is exactly
    // what the old tests lived on and why they passed at all, but nothing
    // outside can put a thread in one ON DEMAND — the
    // windows are a few hundred instructions long, they sit in the middle of
    // `publishFrame`, `readPublishedFrame` and `tick`, and no public call ends
    // inside one. Until 0.2.12 round 18 the two concurrency tests therefore
    // SWEPT for them: they ran the real threads and hoped the scheduler would
    // put one inside the other, counted the times it did, and asserted that the
    // count was not zero. That is a test of the scheduler as much as of the
    // code, and CI proved it twice — `specFrame` on the macOS x86_64 slice
    // under Rosetta, `specStraddle` under valgrind, where a cooperative
    // scheduler produced ZERO straddles in six thousand rounds (KI-019).
    //
    // These three `std::function`s are how the tests stop hoping. Each is
    // called at ONE point, in a state the caller could not otherwise be in, and
    // each is EMPTY in every shipped build — nothing in `src/` assigns one, and
    // the only writers are the two tests. What a test does with the callback is
    // block: the thread inside the bracket waits on a condition variable until
    // the other thread has done its half, so the interleaving is FORCED rather
    // than raced, and it is forced identically under a preemptive scheduler,
    // under valgrind's cooperative one and under binary translation.
    //
    // WHAT THEY DO NOT DO, stated because a seam that quietly changed the thing
    // it observes would be worse than no seam. They add no ordering: each sits
    // BETWEEN two existing operations and cannot move either, and an empty
    // `std::function` is a null test and a not-taken branch, on the message
    // thread, never on the audio path (`REALTIME_AUDIO_POLICY` — the audio
    // thread's only contact with this view is filling two `ScopeBuffer`s). They
    // change no store, no fence, no memory order and no field of the frame.
    // They allocate nothing per tick: the one allocation is the test's own
    // assignment, once, before the threads start. And they are not a way to
    // reach private state — a callback sees exactly what any other thread sees,
    // which is the point.
    //
    // `whileHalfPublished` runs with the counter ODD and the input bins stored
    // against the output bins of the PREVIOUS frame: the published payload is
    // genuinely torn at that instant, which is the state ADR-0039 exists to
    // make unreadable rather than merely unlikely.
    // `whileReadUncommitted` runs with a reader's copy in hand and its closing
    // check not yet run, so a publication completed from another thread must be
    // caught by that check.
    // `whileBatchAnalysed` runs with both windows read and folded and the
    // generations not yet re-read, which is the only instant at which a rewind
    // can become visible INSIDE a tick.
    //
    // WHICH THREAD READS WHICH, because only one of the three is on a
    // boundary at all. `whileHalfPublished` and `whileBatchAnalysed` are read
    // on the MESSAGE THREAD and nowhere else: `publishFrame` is private and
    // called only from `tick`, and `tick` is a `FrameClock` /
    // `juce::VBlankAttachment` callback, which arrives on the message thread.
    // A message-thread read of a message-thread-written member crosses
    // nothing. `whileReadUncommitted` is the one that does: it is read inside
    // `readPublishedFrame`, whose only production caller is `paint`, and paint
    // runs on the GL render thread wherever a context is attached.
    //
    // THAT SHAPE IS ALREADY ADMITTED, and by name. `THREADING_POLICY.md`'s
    // Message -> Painting row (ADR-0027) says of exactly this kind of member:
    // "A hook the paint path invokes is torn down only AFTER that thread is
    // joined (`glContext.detach()` first), because assigning to a live
    // `std::function` races on the callable regardless of what it reads." The
    // rule below is that rule, not a new one — see ADR-0039 clause 12.
    //
    // THE ONE RULE FOR ASSIGNING THEM, and it is the ordinary one for a
    // non-atomic member the painting thread can reach: assign a rendezvous
    // only while NO thread can be inside the function that reads it. `readPublishedFrame`
    // is `const` and runs on whichever thread paints, so arming
    // `whileReadUncommitted` after a reading thread has started — or clearing it
    // before that thread is JOINED — is a data race on the `std::function`
    // itself, not on anything this class publishes. Both tests arm before the
    // thread starts and clear after the join, and the rule is written here
    // rather than left to be rediscovered.
    std::function<void()> whileHalfPublished;      // publishFrame, counter odd, payload torn
    std::function<void()> whileReadUncommitted;    // readPublishedFrame, copy taken, unchecked
    std::function<void()> whileBatchAnalysed;      // tick, windows folded, generations unre-read

    // Read-only views of the smoothed analysis, for the same reason. BOTH, since
    // 0.2.12: what a frame has to get right is that its two traces describe the
    // same span of audio, and a test that can see only one of them cannot pin
    // it — the defect is precisely a disagreement between the pair.
    const std::vector<float>& analysedInDb()  const noexcept { return inDb; }
    const std::vector<float>& analysedOutDb() const noexcept { return outDb; }

    // HAS A RESET HAPPENED THAT THIS VIEW HAS NOT YET ACCOUNTED FOR? Two
    // SUFFICIENT conditions, neither of them necessary, and the OR is the whole
    // point — a `ScopeBuffer` reset is TWO stores (the index rewind, then the
    // generation bump) and a reader can observe either one first.
    //
    // The generation stays the RELIABLE detector and nothing about 0.2.7's
    // finding is reversed: it retired the COUNT as the SOLE detector because a
    // fast refill can pass the old value and the reset is then missed OUTRIGHT,
    // permanently. That is an insufficiency argument, never a soundness one, so
    // the count is re-admitted here as additional, earlier-firing evidence and
    // never as a replacement.
    //
    // Why the count term is SOUND: `write`'s modification order is 0 at
    // construction, then a non-decreasing run of `w + n` from `pushBlock`
    // (single producer), punctuated by 0 from `reset()` — the only writer of a
    // smaller value. `shownCount` is a value THIS thread obtained from an
    // earlier acquire load of that same object, so the two loads are
    // sequenced-before and read-read coherence ([intro.races]) forbids the
    // later one returning a value EARLIER in the modification order. A strictly
    // lower count is therefore proof that a 0-store intervened. It survives
    // arbitrary staleness, because coherence is stated over the modification
    // order rather than over real time.
    //
    // It cannot false-positive: both operands are unsigned and `shownCount`
    // starts at 0, so the first tick and a view attached to an already-running
    // processor both compare against 0. And it needs no change to `tick`'s idle
    // test, which keys on the same counts: `count < shownCount` implies
    // `count != shownCount`, so the gate can never swallow it.
    static bool resetObserved (uint32_t gen, uint32_t shownGen,
                               uint64_t count, uint64_t shownCount) noexcept
    { return gen != shownGen || count < shownCount; }

    // DID THIS FRAME GET ONE SPAN, TWICE? The frame asks both rings for the same
    // `[first, first + span)` and then has to establish that it got it — because
    // the two reads are two separate copies from a producer that never stops,
    // and choosing the span from a snapshot of the floors only settles what was
    // ASKED, not what came back.
    //
    // Two things can go wrong between the snapshot and the end of the second
    // copy, and this is the one place both are decided:
    //   * A READ SHORTENED ITSELF. `readEndingAt` clamps its start to its own
    //     ring's floor at its own load, so a producer that lapped this window
    //     after the snapshot makes THAT read return fewer frames than the other.
    //     `got == span` on both sides is what rejects the half-shortened pair.
    //   * A COPY WAS LAPPED WHILE IT RAN. Neither `got` sees that: the frames
    //     were in range when the read began. The floor re-read AFTER both copies
    //     does see it, and it is monotone, so `oldest ≤ first` on both rings
    //     means no slot in the window was overwritten at any point during
    //     either copy. That is the same before-and-after discipline the reset
    //     generations use two functions up, applied to the lapping bound.
    //
    // A span of 0 is coherent by definition — nothing was asked for and nothing
    // came back, which is how an empty or rewound pair of rings is expressed —
    // so the floor terms are not consulted there.
    static bool onePairOneSpan (int span, int gotIn, int gotOut, uint64_t first,
                                uint64_t oldestIn, uint64_t oldestOut) noexcept
    {
        return gotIn == span && gotOut == span
               && (span == 0 || (oldestIn <= first && oldestOut <= first));
    }

    // THE OLDEST FRAME A DRAWN PAIR MAY REST ON, which is NOT the oldest frame
    // the ring still holds. `ScopeBuffer::oldestReadable()` answers the question
    // the RING can answer — "given the index I have published, what have I not
    // yet overwritten?" — and the reader needs a different one, because a push
    // writes its payload BEFORE it releases the index. While `pushBlock` runs,
    // slots `[w, w + n)` have already been taken and `w` still says they have
    // not; a window ending at `w` that starts at `w − capacity` therefore has
    // its oldest `n` frames rewritten UNDER the copy, with no store the reader
    // can observe and nothing in `got` to show for it. Measured before this
    // bound existed: 17 of 2269 drawn frames mismatched with both rings proved
    // by the published floor alone.
    //
    // The reserve is the largest push the producer can be inside, and it is not
    // a new fact — it is `samplesPerBlock`, which `GrHistoryBuffer::prepared()`
    // already publishes for the whole plugin and `AnabasisAudioProcessor::
    // preparedBlockSize()` forwards (the same discipline, and the same single
    // home, as `preparedSampleRate()`). The draft that carried it a second time
    // as a `maxPush` atomic INSIDE the ring extended the producer/consumer
    // protocol for a quantity the protocol already had; it was withdrawn rather
    // than sent to architecture review, and this is where the bound lives now:
    // on the reader, which is the only side that needs it.
    //
    // At `reserve == capacity` (a chunk of a whole ring) the floor meets the
    // head and the span is 0 — the "nothing coherent left" case, which this
    // view HOLDS rather than draws, and which is the honest answer there.
    static uint64_t reservedFloor (uint64_t writeCount, int reserve) noexcept
    {
        const auto cap   = (uint64_t) anabasis::ScopeBuffer::capacity;
        const auto reach = writeCount + (uint64_t) reserve;
        return reach > cap ? reach - cap : 0;
    }

private:
    // The chip hit-area, in ONE place because `hitTest` and `mouseDown` must
    // agree about it — see the definition.
    juce::Rectangle<int> chipHitArea() const noexcept;
    void analyse (const float* srcL, const float* srcR, int got,
                  std::vector<float>& smoothedDb, double dt);
    // Copies the message thread's two traces and the frame description they
    // belong to into the published storage, inside the odd/even bracket
    // `readPublishedFrame` checks. The ONLY writer, and the only place a frame
    // becomes visible.
    void publishFrame (uint64_t first, int span, double rate, uint32_t config) noexcept;

    // IS THE CONFIGURATION THIS TICK READ STILL THE ONE IT STARTED UNDER? The
    // GR history ring publishes the prepared (rate, block) pair inside its own
    // reset epoch, and `AnabasisAudioProcessor::prepareToPlay` rewinds the two
    // spectrum rings BEFORE it republishes that pair — so the epoch bracket is
    // what makes "the rate this frame publishes" and "the frames this rate is
    // published with" one configuration. Same shape, same reader contract, as
    // `GrHistoryView`'s: an ODD sample is a clear in progress, and a moved value
    // is a clear that overlapped the tick. Either way the frame is HELD.
    bool configurationHeld (uint32_t epoch0) const noexcept;

    AnabasisAudioProcessor& processor;
    abgui::FrameClock clock;
    // The seconds this view was hidden for, which is the dt the reveal's tick
    // analyses with: the per-bin EMA decays in wall-clock time and the stopped
    // clock cannot report the time it did not tick through. Shared with
    // `GrHistoryView`, whose smoothed head needs the same quantity for its own
    // reason — see `HiddenInterval.h` and `visibilityChanged`.
    abgui::HiddenInterval hidden;
    juce::dsp::FFT fft { kOrder };
    juce::dsp::WindowingFunction<float> window { kSize,
        juce::dsp::WindowingFunction<float>::hann };

    // ONE PAIR PER RING, because the frame reads both windows BEFORE it analyses
    // either: the pair has to be shown coherent (`onePairOneSpan`) while both
    // copies are still in hand, and the old single pair was overwritten by the
    // second read before the first had been transformed. 32 KB more, allocated
    // once at construction — the tick allocates nothing, as before.
    std::vector<float> scratchInL, scratchInR, scratchOutL, scratchOutR, fftData, inDb, outDb;
    // `shownInCount`/`shownOutCount` are the last index observed in each ring,
    // and since 0.2.12 they answer ONE question: has this ring rewound?
    // (`resetObserved`'s count term, whose coherence argument is about a single
    // ring's modification order.) "Are there new frames?" is `shownCommitted`'s
    // question, because a frame is drawn from the span BOTH taps have
    // published — see tick().
    uint64_t shownInCount = 0, shownOutCount = 0;
    // The common committed head this view last drew from: `min` of the two
    // published indices at that tick. Zero before anything is drawn, which is
    // also what an empty pair of rings reports, so the first tick over empty
    // rings idles exactly as it always did.
    uint64_t shownCommitted = 0;
    // …and the window the last DRAWN frame was analysed over (`lastWindow`).
    uint64_t drawnFirst = 0;
    int      drawnSpan  = 0;
    double   drawnRate  = 0.0;
    // The configuration identity of the last committed frame, and the view's
    // running count of the resets it has answered. Message-thread only; the
    // published copy is `pubConfig`, inside the frame's own bracket.
    uint32_t drawnConfig = 0;
    uint32_t configSeq   = 0;
    uint32_t shownInGen = 0, shownOutGen = 0;

    static_assert (std::atomic<float>::is_always_lock_free
                     && std::atomic<double>::is_always_lock_free
                     && std::atomic<uint64_t>::is_always_lock_free
                     && std::atomic<int>::is_always_lock_free
                     && std::atomic<uint32_t>::is_always_lock_free,
                   "the published trace is stored one bin at a time in atomics that the renderer "
                   "reads; a locking atomic here would put a lock inside paint() on the OpenGL "
                   "render thread, so a target without lock-free float atomics must fail the "
                   "build rather than ship one (same rule, same reason, as ScopeBuffer::Sample)");

    // THE PUBLISHED FRAME. `std::atomic<float>` per bin for the reason
    // `ScopeBuffer::Sample` is one: the renderer reads these while a tick writes
    // them, and a plain `float` touched by two threads is a data race whatever
    // the hardware does with it. Relaxed on both sides — the ORDERING is carried
    // by `frameSeq`, not by the payload, which is the standard sequence-bracket
    // discipline and is why the payload needs no stronger order.
    //
    // 16 KB, allocated once at construction. A tick allocates nothing: it stores
    // 2 × 2048 floats it has already computed, and the painter loads them into
    // buffers it also allocated once. Publishing an immutable snapshot per tick
    // would have been simpler to state and would have allocated 16 KB every
    // frame at 60 Hz; this does not.
    std::vector<std::atomic<float>> pubIn, pubOut;
    std::atomic<uint64_t> pubFirst { 0 };
    std::atomic<int>      pubSpan  { 0 };
    // …and the rate the published bins are to be read through, inside the same
    // bracket as the bins themselves. `std::atomic<double>` for the reason the
    // bins are atomic, and lock-free for the reason they must be.
    std::atomic<double>   pubRate  { 0.0 };
    std::atomic<uint32_t> pubConfig { 0 };
    // EVEN means the published frame is whole; ODD means a tick is inside the
    // bracket. A reader that sees an odd value, or a different value after its
    // copy, saw a publication in progress and has no frame — see
    // `readPublishedFrame`, which gives up after two attempts rather than
    // spinning: the frame it would win by retrying further is one it is about
    // to redraw a 60th of a second later, and an unbounded loop on the render
    // thread is a worse failure than a repeated frame.
    std::atomic<uint32_t> frameSeq { 0 };

    // The painter's OWN copy of the pair, so the traces it walks cannot move
    // under it while it walks them (each is read four times over, by the
    // interpolators). Touched only by `paint`, which JUCE never runs twice over
    // one component at once. Seeded to the display floor, which is what an
    // analyser with no frame yet has always drawn.
    std::vector<float> paintIn, paintOut;
    Frame              paintFrame {};
    // …and the buffers it reads INTO, which are a different thing: a lost read
    // leaves indeterminate content behind, so the read lands here and is
    // committed to the pair above only when it succeeded. The commit is a SWAP,
    // so a successful frame costs two pointer exchanges rather than a second
    // copy and a lost one costs nothing at all. 16 KB more, allocated once.
    std::vector<float> stageIn, stageOut;
    Frame              stageFrame {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumView)
};
