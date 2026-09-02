#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

// ============================================================================
//  GrHistoryBuffer — the §2.9 GR/waveform history ring, the first Audio→GUI
//  SPSC ring in the tree (THREAD_MODEL's planned edge, now implemented, in
//  the ScopeBuffer idiom ADR-0011 cites):
//
//  - power-of-two storage, ONE producer (the audio thread, one entry per
//    processed block), ONE reader side (whatever paints);
//  - the monotonic write index is release-STORED once per entry, acquire-
//    loaded by readers, so a reader that sees index N sees entry N−1's data
//    complete;
//  - reads are stateless `const` peeks that consume nothing — any number of
//    message-thread/GL-paint read sites stay safe (THREADING_POLICY's ring
//    rule and its OpenGL nuance);
//  - THE PREPARED PAIR IS RING METADATA (0.2.8 final review). One entry spans
//    one host block, so the entries only mean anything mapped through the
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
//  (post-chain, linear). At 512-sample blocks a 4096-entry ring holds ~43 s
//  at 48 kHz — beyond the 10–30 s display window at every rate the product
//  supports; the GUI decimates for display.
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

    static constexpr int kSize = 4096;            // power of two
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

    // Reader side: the pair the entries were recorded under. Relaxed loads —
    // the caller brackets them with the epoch exactly as it brackets `peek`
    // (`resetEpoch()` even before, `batchIntact` after), and that bracket is
    // what makes the two loads coherent with each other AND with the entries.
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
        for (auto& e : entries)
        {
            e.grDb.store (0.0f, std::memory_order_relaxed);
            e.peak.store (0.0f, std::memory_order_relaxed);
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
    uint32_t resetEpoch() const noexcept
    { return resetGuard.load (std::memory_order_acquire); }

    // Audio thread, once per block. The entry is written FIRST, the index
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
    Entry peek (int64_t n) const noexcept
    {
        const auto& slot = entries[(size_t) (n & (int64_t) kMask)];
        return { slot.grDb.load (std::memory_order_relaxed),
                 slot.peak.load (std::memory_order_relaxed) };
    }

private:
    Slot entries[kSize];
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
