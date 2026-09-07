#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <memory>

// ============================================================================
//  GrHistoryBuffer — the §2.9 GR/waveform history ring, the first Audio→GUI
//  SPSC ring in the tree (THREAD_MODEL's planned edge, now implemented, in
//  the ScopeBuffer idiom ADR-0011 cites):
//
//  - power-of-two storage, ONE producer (the audio thread, one entry per
//    PREPARED block of processed audio — see `push`), ONE reader side
//    (whatever paints);
//  - the monotonic write index is release-STORED once per entry, acquire-
//    loaded by readers, so a reader that sees index N sees entry N−1's data
//    complete;
//  - reads are stateless `const` peeks that consume nothing — any number of
//    message-thread/GL-paint read sites stay safe (THREADING_POLICY's ring
//    rule and its OpenGL nuance);
//  - THE PREPARED PAIR IS RING METADATA (0.2.8 final review). One entry spans
//    one PREPARED block (0.2.12 — see the first bullet and `push`; it used to
//    say "one host block", which was the same thing only for a host that
//    delivered exactly its declared maximum), so the entries only mean
//    anything mapped through the
//    (rate, block) they were recorded under — and that pair lives HERE, stored
//    inside the same clear that starts a new timeline, rather than being read
//    back from `AudioProcessor`'s plain `getSampleRate()`/`getBlockSize()`,
//    which the host writes from its own callback thread while the message
//    thread ticks and the render thread paints. Readers take it with
//    `prepared()` inside the same epoch bracket as the entries, so the pair a
//    frame maps through is the pair its entries were recorded under, and the
//    two cannot be seen torn against each other.
//  - THE READER'S CLOSING CHECK IS A FENCE, NOT JUST A LOAD (`batchIntact`).
//    An acquire LOAD of the epoch keeps later accesses after it; it does not
//    keep the batch's earlier relaxed loads BEFORE it, so on a weakly ordered
//    target a data load could be satisfied after the epoch was re-read and a
//    torn batch pass as intact. `atomic_thread_fence (acquire)` before the
//    re-read is the missing half: a data load that saw a value from inside a
//    clear then synchronises with the writer's release fence, the odd epoch
//    happens-before the re-read, and the batch is discarded. This is the
//    seqlock reader as the memory model requires it, not as x86 forgives it.
//  - THE PRODUCER RELEASE-FENCES TOO, AND THAT IS WHAT MAKES THE LAP CHECK
//    MEAN ANYTHING (round 6). The fence above closes the reader's half for the
//    CLEAR, because `clear` release-fences before it writes the payload. The
//    LAP had only half a bargain: `push` stored its payload relaxed and then
//    released the INDEX, and a release store orders what precedes it, never
//    what follows — so a peek could return an overwriting value while the
//    closing `available()` re-read still returned a pre-lap index, and the
//    frame was ACCEPTED with overwritten entries in it. Defined data, wrong
//    data. `push` now carries the same release fence `clear` does, before its
//    payload stores, which pairs with the reader's acquire fence by
//    [atomics.fences] and forces the re-read to observe the lapping push.
//    THE INVARIANT IS THEREFORE: either the batch read clean data, or the
//    discard is guaranteed — there is no third case. It costs the audio thread
//    zero instructions on x86-64 and one `dmb ish` per PUBLISHED ENTRY on
//    AArch64 (`push`'s own note has the cadence that unit implies).
//  - THE PAYLOAD ITSELF IS ATOMIC (0.2.8 review). The index ordering above
//    settles what a reader SEES; it does not make a read that lands on the
//    slot the producer is writing legal. That read is exactly what this
//    ring's two guards — the reset epoch and the reader's window clamp —
//    are designed to DETECT and discard, and detection is the wrong tool for
//    the job: under the C++ memory model a plain read concurrent with a plain
//    write is a data race and therefore undefined behaviour the moment it
//    happens, and discarding the frame afterwards cannot unhappen it. The
//    stored fields are `std::atomic<float>`, written and read RELAXED, so the
//    racing case is defined — each field yields one of the two values, the
//    pair may be mismatched — and the guards keep their job, which is to
//    throw such a frame away. Relaxed adds no fence and no instruction on the
//    supported targets: the audio-thread store is the same store it was, and
//    the `static_assert` below is what keeps that true (a non-lock-free
//    `std::atomic<float>` would put a LOCK in `push`, which
//    `REALTIME_AUDIO_POLICY` forbids outright — it must fail the build).
//
//  Entry = per-block gain reduction (dB, ≤ 0) + the block's waveform peak
//  (post-chain, linear), ONE PER PREPARED BLOCK — so the seconds a full ring
//  holds are `kSize · block / rate`, and the window a frame may read is one
//  slot less than that (`GrHistoryView::windowEntries`). THE SIZE OF THIS RING
//  IS THEREFORE A TIME CONTRACT, and it is one at EVERY prepared pair rather
//  than at a nominal one. Sizing it against a single block size is what this
//  constant did until 0.2.12 round 17, and the sentence that stood here —
//  "~43 s at 48 kHz, beyond the 10–30 s display window at every rate the
//  product supports" — was true only of the 512-sample block it quietly
//  assumed. MEASURED at 4096 entries on the real engine, the real ring and
//  the real view: 20 s held at 48 kHz / 256 and above, then 10.92 s at
//  48 kHz / 128, 5.46 s at 48 kHz / 64, 2.73 s at 48 kHz / 32 and 0.6825 s at
//  192 kHz / 32 — against the twenty seconds USER_MANUAL.md promises and the
//  ten DESIGN §2.9 floors at. The capacity below is chosen from the worst
//  prepared pair this product costs itself against instead; the GUI decimates
//  for display.
// ============================================================================

namespace anabasis
{

class GrHistoryBuffer
{
public:
    // What a reader gets: a plain value pair, assembled from the atomic slot
    // below. Kept plain deliberately — callers copy it, compare it and store
    // it in local aggregates, and none of that wants atomics.
    struct Entry
    {
        float grDb  = 0.0f;
        float peak  = 0.0f;
    };

    // What the ring STORES. Public so the property the banner argues can be
    // asserted by a test rather than trusted from a comment: the payload is
    // atomic, and it is lock-free.
    struct Slot
    {
        std::atomic<float> grDb { 0.0f };
        std::atomic<float> peak { 0.0f };
    };

    static_assert (std::atomic<float>::is_always_lock_free,
                   "the audio thread stores these; a locking atomic here would be a lock on the "
                   "audio path (REALTIME_AUDIO_POLICY), so a target without lock-free float "
                   "atomics must fail the build rather than ship one");

    // The (rate, block) pair the entries are recorded under — see `prepare`.
    struct Prepared
    {
        double rate  = 0.0;
        int    block = 0;
    };

    // CHOSEN FROM THE WORST PREPARED PAIR, NOT FROM A NOMINAL BLOCK (0.2.12
    // round 17). An entry is one prepared block, so `N` slots are
    // `N · block / rate` seconds and the entries a 20 s window needs are
    // `ceil (20 · rate / block)`: 120000 at 192 kHz / 32, the cell ADR-0020 §1
    // and ADR-0011's 2026-09-07 amendment already cost this product against
    // and the state suite already exercises. The next power of two is this,
    // which holds the whole 20 s at every pair up to 6553 entries a second
    // and stays inside DESIGN §2.9's 10 s floor up to 13107 — 192 kHz / 16,
    // below anything a host offers. Below THAT the window is
    // `(kSize - 1) · block / rate` and shortens in proportion, which is what
    // `GrHistoryView::windowSeconds` states and a test pins.
    //
    // FIXED, not sized at `prepare`. A reader can be inside `peek` when the
    // host re-prepares: the epoch bracket makes a torn READ safe, and no
    // amount of it makes a freed pointer safe. Re-allocating on the prepared
    // pair would need a reclamation protocol this ring deliberately does not
    // have, so the capacity is a constant and the shortfall below it is
    // documented rather than allocated away.
    static constexpr int kSize = 1 << 17;         // 131072 entries, power of two
    static constexpr int kMask = kSize - 1;

    GrHistoryBuffer() = default;

    // Host thread (prepareToPlay, audio stopped). The P5 READER CONTRACT this
    // settles (THREAD_MODEL's planned-edge question, designed here as
    // promised): the write index is monotonic BETWEEN resets and MAY REWIND
    // across one, and the bulk clear below is a host-thread write a concurrent
    // `const` peek could observe half-done. The reset epoch is what makes both
    // safe to read against: it is bumped to ODD before the clear and back to
    // EVEN after (a seqlock in miniature), so a reader samples `resetEpoch()`
    // before a batch of peeks and again after — an odd value or a changed
    // value means the batch raced a reset and is discarded, and the reader
    // re-anchors its cursor to the fresh `available()`. One display frame is
    // dropped at worst, on an event (re-prepare) that already blanks the
    // programme. Readers must therefore never cache `available()` across an
    // epoch change; within one epoch the existing SPSC contract is unchanged.
    //
    // CALL `AnabasisEngine::resetHistoryTimeline()` INSTEAD when this ring has
    // a producer (0.2.12 round 16). This starts a NEW TIMELINE, and the
    // producer carries an unpublished partial entry belonging to the old one:
    // clear without telling the engine to re-read the epoch and the new
    // timeline's first entry is completed with the previous one's statistics,
    // standing for as little as one sample. That wrapper is this call plus the
    // sync, so it cannot be got wrong; the same is true of
    // `prepareHistoryTimeline` for the gate below. This entry point stays
    // public for rings with no producer attached, which is what the tests use.
    void reset() noexcept
    {
        clear (preparedRate.load (std::memory_order_relaxed),
               preparedBlock.load (std::memory_order_relaxed));
    }

    // Host thread (`prepareToPlay`). The clear-on-change gate that lived in the
    // wrapper until the 0.2.8 final review, moved here so the pair it compares
    // is the pair the ring PUBLISHES: a re-prepare at the same (rate, block)
    // keeps the timeline (0.1.2 item 6 — hosts re-prepare on transport start,
    // and the display must continue rather than restart), and a changed pair
    // clears the ring AND stores the new pair inside that clear's epoch
    // window, so no reader can pair new entries with the old time base or the
    // reverse. Returns whether it cleared. Single writer, its own previous
    // stores, so the comparison reads are relaxed.
    bool prepare (double rate, int block) noexcept
    {
        if (juce::exactlyEqual (preparedRate.load (std::memory_order_relaxed), rate)
            && preparedBlock.load (std::memory_order_relaxed) == block)
            return false;
        clear (rate, block);
        return true;
    }

    // Reader side: the pair the entries were recorded under. Relaxed loads.
    //
    // TWO DISCIPLINES USE THIS, and they want different things — stated here
    // because the single-sentence version was true of one caller and became
    // false when the second arrived (KI-017, round 6).
    //  * A reader that maps ENTRIES through the pair must bracket it with the
    //    epoch exactly as it brackets `peek` (`resetEpoch()` even before,
    //    `batchIntact` after). The bracket is what makes the two loads coherent
    //    with each other AND with the entries, so a frame cannot map one
    //    timeline's entries through another's time base. `GrHistoryView` does
    //    this and must keep doing it.
    //  * A reader that wants only "what rate is the plugin prepared at, right
    //    now" needs no bracket, and taking one would mean nothing: there are no
    //    entries in its question. One relaxed load of one lock-free atomic is
    //    coherent by itself, and a caller that reads a rate one reconfiguration
    //    stale draws one frame at the previous rate — the same class of
    //    residual ADR-0027 clause 4 (as amended by ADR-0038) already licenses
    //    for a single unpaired scalar on this boundary. `CurveView` does this,
    //    through `AnabasisAudioProcessor::preparedSampleRate`: its curve comes
    //    from the parameter set, not from a ring, so there are no entries whose
    //    timeline the rate has to match. What it must NOT do is read it twice in
    //    one expression.
    //
    //    **`SpectrumView` MOVED TO THE FIRST DISCIPLINE ON 2026-09-06, and this
    //    sentence used to name it here.** That was the split misapplied: a
    //    spectrum trace is a row of BIN indices, and the rate is what turns a
    //    bin into a frequency, so the view has entries in its question exactly
    //    as `GrHistoryView` does. Reading the rate unbracketed let a rendered
    //    frame pair one configuration's trace with another's mapping — measured
    //    at 6 kHz, which is bin 512 at 48 kHz and bin 256 at 96 kHz: the tone
    //    read −0.00 dB paired, −116.80 dB and −120.00 dB crossed. It now samples
    //    `resetEpoch()` and the pair together at the top of its tick and closes
    //    with `batchIntact` before it publishes anything
    //    (`SpectrumView::configurationHeld`; ADR-0039).
    //
    // Zeros before the first `prepare`, which the view's `windowEntries` /
    // `entryPeriod` already read as 48 kHz.
    Prepared prepared() const noexcept
    {
        return { preparedRate.load (std::memory_order_relaxed),
                 preparedBlock.load (std::memory_order_relaxed) };
    }

    // Reader side, the CLOSE of a batch: did the epoch hold across it? The
    // acquire FENCE is the point (banner): it orders every relaxed load the
    // batch made before the epoch re-read, so a load that saw a value from
    // inside a clear synchronises with the writer's release fence and the odd
    // epoch is what this returns. Boehm, "Can Seqlocks Get Along with
    // Programming Language Memory Models?" (MSPC 2012) — this is the reader
    // that paper shows to be correct; an acquire load alone is not.
    bool batchIntact (uint32_t epoch0) const noexcept
    {
        std::atomic_thread_fence (std::memory_order_acquire);
        return resetGuard.load (std::memory_order_relaxed) == epoch0;
    }

private:
    void clear (double rate, int block) noexcept
    {
        // ORDERING, stated as what the barrier actually gives rather than as
        // what a release STORE would give. The opening needs the odd value
        // visible BEFORE the clear; a release store orders earlier accesses
        // before ITSELF, which is the wrong direction here. `atomic_thread_
        // fence(release)` is a StoreStore+LoadStore barrier: accesses
        // sequenced before it cannot be reordered after any store sequenced
        // after it — so the relaxed increment above cannot sink past the clear
        // below. Relaxed increment + release fence is the canonical seqlock
        // write-begin (the same shape as the kernel's `seq++; smp_wmb();`).
        // The closing increment is a release STORE, and there the direction is
        // right: it orders the clear before the even value.
        //
        // What this does NOT claim: a racing batch can still observe a torn
        // VALUE (the stores below are atomic, so the read is defined, but the
        // pair may be half old, half cleared). Nothing here prevents that —
        // the reader's epoch bracket DISCARDS such a batch instead, which is
        // the seqlock bargain and the reason `resetEpoch()` is sampled before
        // a batch and `batchIntact` after it. The barrier's job is only to
        // keep "odd" from arriving after the writes it is meant to announce.
        resetGuard.fetch_add (1, std::memory_order_relaxed);   // odd: clearing
        std::atomic_thread_fence (std::memory_order_release);
        // RELAXED stores, for the banner's reason: a reader may be peeking
        // these very slots while this loop runs — that is the race the epoch
        // exists to announce — and the accesses on both sides have to be
        // atomic for the announcement to be about defined behaviour. The
        // fence above still orders the odd value before every one of them.
        for (int n = 0; n < kSize; ++n)
        {
            entries[(size_t) n].grDb.store (0.0f, std::memory_order_relaxed);
            entries[(size_t) n].peak.store (0.0f, std::memory_order_relaxed);
        }
        // The pair the NEW timeline is recorded under, inside the same window
        // as the entries it governs — that is what lets a reader treat the two
        // as one coherent unit under one epoch.
        preparedRate.store (rate, std::memory_order_relaxed);
        preparedBlock.store (block, std::memory_order_relaxed);
        writeIndex.store (0, std::memory_order_release);
        resetGuard.fetch_add (1, std::memory_order_release);   // even: stable
    }

public:
    // Reader side of the contract above. Even = stable; sample before a batch
    // of peeks, and close the batch with `batchIntact` — which is the fence
    // plus the re-read, and not this load again.
    //
    // THE PRODUCER'S OWNER READS IT TOO, since 0.2.12 round 16, and for a
    // different question: not "did my batch survive a clear?" but "is the
    // partial entry the engine is carrying still on this ring's timeline?". A
    // clear is the only thing that moves this value, so a change in it IS the
    // event "a new timeline started", whichever call caused it, and
    // `AnabasisEngine::syncHistoryTimeline` throws the unpublished partial
    // away when it sees one — welded to the two calls that can clear, as
    // `prepareHistoryTimeline`/`resetHistoryTimeline`. That read takes no
    // batch and needs no bracket —
    // it compares one epoch against the one it recorded — and it happens on
    // the HOST thread, right after the call that may have cleared, so it is
    // not a read of this ring off the message thread and adds no path to
    // `THREADING_POLICY.md`'s table.
    uint32_t resetEpoch() const noexcept
    { return resetGuard.load (std::memory_order_acquire); }

    // Audio thread, once per PUBLISHED ENTRY — see the cadence note inside.
    // The entry is written FIRST, the index
    // release-stored AFTER — that ordering is the whole synchronisation, and
    // it is unchanged by the fields being atomic: the release store still
    // orders both relaxed payload stores before the index a reader acquires,
    // so "a reader that sees index N sees entry N−1 complete" holds exactly
    // as it did. What the atomics add is the OTHER case — a reader that lands
    // on this slot while this function is inside it — which the banner argues
    // and the reader's guards discard.
    void push (float grDb, float peak) noexcept
    {
        const auto i = writeIndex.load (std::memory_order_relaxed);
        auto& slot = entries[(size_t) (i & (int64_t) kMask)];
        // THE LAP CHECK'S HALF OF THE BARGAIN, AND IT IS THE PRODUCER'S.
        // `clear` has carried this exact fence since 0.2.8, so that a payload
        // load which witnessed the clear makes the odd epoch happen-before
        // `batchIntact`'s re-read. The LAP is the same shape with `writeIndex`
        // in place of `resetGuard` — and it had no fence, which is what the
        // second 2026-09-02 amendment admitted when it said the model gave the
        // lap check "no formal guarantee".
        //
        // Without the fence: a reader whose relaxed peek returned THIS push's
        // value has no edge forcing its `available()` re-read to observe `i`.
        // A release STORE orders accesses BEFORE itself, never after, so the
        // payload stores below may become visible ahead of the previous push's
        // release store of the index; the re-read may then legally return a
        // pre-lap value, `first < readFloor (available())` is false, and the
        // frame is ACCEPTED carrying entries the producer overwrote. Defined
        // (the payload is atomic) but WRONG DATA — not a dropped frame.
        //
        // With it, by [atomics.fences]: this fence (A) is sequenced before the
        // payload stores (X); the reader's peek (Y) reads the value X wrote
        // and is sequenced before the acquire fence (B) in `batchIntact`; so A
        // synchronises with B, and everything sequenced before A — including
        // the PREVIOUS push's release store of `i` — happens-before everything
        // sequenced after B, which the reader orders to include the
        // `available()` re-read. Write-read coherence then forbids that re-read
        // returning less than `i`, and `first <= i - kSize < readFloor (i)`
        // discards the frame. EITHER THE BATCH READ CLEAN DATA OR THE DISCARD
        // IS GUARANTEED; there is no third case.
        //
        // Cost, measured rather than asserted: **zero instructions on x86-64**
        // (clang-22 `-O3` emits `#MEMBARRIER`, a compiler barrier — the
        // previous amendment's two-`movss`-one-`movq` sequence is unchanged),
        // and one `dmb ish` on AArch64, PER CALL TO THIS FUNCTION.
        //
        // WHAT THAT UNIT IS, since it stopped being "one per `processBlock`"
        // in 0.2.12 (OQ-017 fix 1; ADR-0011 amended 2026-09-07). The producer
        // is `AnabasisEngine`, and one entry is one PREPARED block of
        // processed audio, so a `processBlock` call publishes
        // `floor((carried + delivered) / prepared)` entries — none when the
        // host delivers less than the prepared size, one when it delivers
        // exactly it, several when it delivers a multiple — with the remainder
        // carried into the next call. What is guaranteed is the LONG-RUN rate,
        // `sampleRate / preparedBlock`, and the bound: never more than
        // `ceil(delivered / prepared)` calls to this function per host block,
        // and never per sample.
        std::atomic_thread_fence (std::memory_order_release);
        slot.grDb.store (grDb, std::memory_order_relaxed);
        slot.peak.store (peak, std::memory_order_relaxed);
        writeIndex.store (i + 1, std::memory_order_release);
    }

    // Reader side: how many entries have ever been pushed.
    int64_t available() const noexcept
    { return writeIndex.load (std::memory_order_acquire); }

    // Stateless peek at entry n (absolute index). Entries older than kSize
    // behind the head have been overwritten; the caller clamps its window.
    // The clamp is **kSize - 1**, not kSize: the index is masked, so `head -
    // kSize` aliases the slot `push` is filling at this instant (it writes the
    // slot, THEN publishes head + 1). A reader that asks for the full capacity
    // therefore reads a half-written entry as its oldest one.
    //
    // The clamp is the reader's side of the bargain and it is not the whole
    // of it: a batch long enough for the producer to lap it reaches these
    // slots anyway, whatever window it started from. So the loads are
    // RELAXED ATOMIC rather than plain — the racing read is defined, each
    // field yielding one of the two values — and the caller re-checks its
    // window afterwards and throws such a frame away
    // (`GrHistoryView::readFloor`, the epoch guard). Defined-then-discarded,
    // not detected-after-the-fact.
    //
    // THAT RE-CHECK ONLY BECAME BINDING IN ROUND 6. Until then the producer
    // published its payload with no fence, so a peek could return an
    // overwriting value while the re-read still saw a pre-lap index and the
    // frame was kept. `push`'s release fence is what pairs with the reader's
    // acquire fence and makes "throws such a frame away" a guarantee rather
    // than an intention; see the banner and `push`.
    Entry peek (int64_t n) const noexcept
    {
        const auto& slot = entries[(size_t) (n & (int64_t) kMask)];
        return { slot.grDb.load (std::memory_order_relaxed),
                 slot.peak.load (std::memory_order_relaxed) };
    }

private:
    // ON THE HEAP, ONE ALLOCATION AT CONSTRUCTION, AND THAT IS NOT AN
    // AESTHETIC CHOICE. At `kSize` entries this array is a megabyte, and the
    // suites build both this ring and whole `AnabasisAudioProcessor`s as
    // LOCALS — two of the latter live at once in places, eight in one
    // function — against a Windows main thread whose default stack is one
    // megabyte in total. A member array would put
    // the capacity decision and a stack overflow on the same line, with no
    // diagnostic between them. Nothing on the audio path allocates: the block
    // is taken once here, on the message thread, and lives as long as the
    // ring, so `push` and `peek` pay one extra load of a pointer that is hot
    // in L1 (measured: no change in the per-entry push cost).
    std::unique_ptr<Slot[]> entries { new Slot[(size_t) kSize] };
    std::atomic<int64_t>  writeIndex { 0 };
    std::atomic<uint32_t> resetGuard { 0 };
    // Host-thread written inside `clear`'s epoch window, read by the painting
    // and message threads under the epoch bracket. Lock-free or the build
    // fails, for the same reason as the payload's assertion above.
    static_assert (std::atomic<double>::is_always_lock_free && std::atomic<int>::is_always_lock_free,
                   "the prepared pair is read on the painting thread without blocking, or not at all");
    std::atomic<double>   preparedRate  { 0.0 };
    std::atomic<int>      preparedBlock { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrHistoryBuffer)
};

} // namespace anabasis
