#pragma once

// Provenance (ADR-0009): copied from Anamorph src/dsp/ScopeBuffer.h:1-93 @ b6a3db8.
// The §2.9 spectrum capture rings instantiate it — the THREAD_MODEL planned
// edge, now implemented on the SPSC ring row. FOUR functional deltas beyond
// the namespace, each stated where it lives: heap storage instead of inline
// arrays (the ctor, below); an ATOMIC payload; following from it, the
// reversion of the sibling's Wave-4 two-segment `memcpy` to two store loops
// (`pushBlock`); and the reader side needed to serve TWO rings as one — an
// entry point that takes the END of the window rather than always taking this
// ring's own head, a start clamped to the frames the producer has not taken
// back, and the floor query a caller needs to choose a length both rings can
// honour (`readEndingAt`, `oldestReadable`, 0.2.12). The sibling has ONE ring
// and no pairing question; this product has two, and its analyser must read
// both over one committed span. ADR-0009 item 8 makes divergence accepted and one-way — there
// is no upstream-sync obligation and no backport path, and Anamorph is
// read-only from here (CLAUDE.md §3) — so the sibling keeps the unrepaired
// shape. That instance is recorded in `docs/KNOWN_ISSUES.md` (KI-016) rather
// than left as an undocumented difference.

#include <atomic>
#include <vector>
#include <cstdint>

namespace anabasis
{

// ============================================================================
//  ScopeBuffer
//
//  Lock-free single-producer ring buffer of stereo samples. The audio thread
//  (producer) only writes; all reads happen on the GUI (message) thread as
//  stateless peeks. No locks, no allocation on either side.
//
//  The reader inventory, since the inherited banner named Anamorph's and this
//  product has no vectorscope: `SpectrumView::analyse` (one `readLatest` per
//  ring per tick) and the two suites. There is no second GUI reader.
//
//  Capacity is a power of two so wrap is a cheap mask.
//
//  THE PAYLOAD IS ATOMIC, AND THAT IS A CORRECTNESS PROPERTY, NOT A STYLE.
//  The release/acquire pair on `write` settles what a reader SEES — every
//  frame strictly below the acquired index is complete — and that is a
//  BACKWARD edge only: nothing in `pushBlock` reads anything the reader
//  writes, so the producer's SUBSEQUENT writes are unordered against every
//  iteration of the reader's copy loop. When the producer laps the reader
//  mid-copy the two touch the same `float` objects, and until this was
//  repaired both accesses were plain — a data race, undefined the moment it
//  happens, whatever the machine code looked like. Storing each sample in a
//  relaxed atomic makes that read DEFINED: each load returns some value from
//  that object's modification order. Deliberately NOT the sibling ring's
//  "one of the two values" wording — `GrHistoryBuffer` has one slot of margin
//  by construction, whereas a lapped reader here can be overtaken repeatedly
//  during one 4096-frame copy, so the guarantee is coherence, not a two-value
//  choice. The DISPLAY consequence is unchanged and was always accepted: one
//  FFT frame may mix old and new audio, and it decays on the analyser's
//  ~120 ms EMA.
//
//  TWO SPELLINGS ARE FORBIDDEN HERE, and both are compile errors or gated
//  warnings rather than review promises, because neither is caught by any
//  runtime instrument this repository owns:
//    * `std::memcpy` over the payload. It is what this repair removed, and
//      re-introducing it would reinstate exactly the undefined behaviour.
//      `Sample` has no trivial copy-assignment, which is the criterion GCC's
//      `-Wclass-memaccess` uses, so GCC diagnoses it and the
//      zero-first-party-warning gate turns that into a red job — MEASURED,
//      both compilers: clang-22 is SILENT even with `-Wnontrivial-memaccess`,
//      so the GCC lanes are the whole of the automated catch and the clang
//      lanes are not a second opinion. Note the criterion is NOT
//      `std::is_trivially_copyable`, which reports TRUE for this type on both
//      libstdc++ and libc++ despite every copy and move operation being
//      deleted; the test asserts copy-constructibility and copy-assignability
//      instead, for that measured reason.
//      `<cstring>` is deliberately not included, so the spelling also needs a
//      new include: a visible diff rather than a silent one.
//    * `slot = value`. On a bare `std::atomic<float>` that selects
//      `operator=`, which is SEQUENTIALLY CONSISTENT — a fenced store per
//      sample on the audio thread. `Sample` wraps the atomic and exposes only
//      relaxed `store`/`load`, so the spelling does not compile.
// ============================================================================
class ScopeBuffer
{
public:
    // One stored sample. The wrapper is the enforcement half of the banner's
    // second prohibition — see there for why a bare `std::atomic<float>` is
    // not enough — and it is public for the same reason `GrHistoryBuffer::Slot`
    // is: a test asserts the property instead of trusting the comment.
    class Sample
    {
    public:
        void  store (float v) noexcept { value.store (v, std::memory_order_relaxed); }
        float load() const noexcept    { return value.load (std::memory_order_relaxed); }

    private:
        std::atomic<float> value { 0.0f };
    };

    static_assert (std::atomic<float>::is_always_lock_free,
                   "the audio thread stores these; a locking atomic here would be a lock on the "
                   "audio path (REALTIME_AUDIO_POLICY), so a target without lock-free float "
                   "atomics must fail the build rather than ship one");
    static_assert (sizeof (Sample) == sizeof (float) && alignof (Sample) == alignof (float),
                   "the payload's footprint per RING is unchanged at 2 x 64 KB (left + right), "
                   "which is what the heap-storage note below depends on — the ENGINE holds two "
                   "rings, so the 2 x 128 KB figure there is the same fact in the engine's frame");

    static constexpr int capacity = 1 << 14; // 16384 stereo frames
    static constexpr int mask     = capacity - 1;

    // Adapted beyond the namespace (the FIRST of the provenance header's three
    // functional deltas): storage lives on the HEAP, not inline in the object. Anamorph
    // holds one ScopeBuffer; Anabasis's engine holds two, and 2 × 128 KB of
    // inline arrays ride along with EVERY engine — including the processors
    // the state suite builds on the STACK, where Windows' 1 MB default
    // overflowed (the Linux 8 MB default hid it; the crash ate its own
    // buffered output, which is why CI showed exit 1 and nothing else).
    // Allocation happens HERE, at construction on a non-audio thread; the
    // audio-thread push path still never allocates.
    // The size ctor VALUE-initialises, and `Sample`'s member initialiser makes
    // that 0.0f — the fill argument the plain form carried is ill-formed now
    // that the element is non-copyable, which is the point rather than a cost.
    ScopeBuffer() : left ((size_t) capacity), right ((size_t) capacity)
    { write.store (0, std::memory_order_relaxed); }

    // Host thread (prepare, audio stopped). Rewinds the published index so no
    // reader can reach a frame captured under the PREVIOUS configuration.
    // `readLatest` returns the newest N frames and the analyser maps their
    // bins through the CURRENT sample rate, so frames captured at the old rate
    // are drawn at the wrong frequencies until they age out. The GR history
    // ring has been cleared at `prepareToPlay` since P3 for the same reason;
    // these two were the analyser state that survived a re-prepare.
    //
    // Deliberately only the INDEX, not the samples: `readLatest` copies
    // strictly below the acquired index, so rewinding it makes every stale
    // frame unreachable, and the reader's own `count > w` clamp then returns
    // fewer frames until the ring refills. Clearing 2 × 16384 floats on a
    // re-prepare would buy nothing a reader can observe.
    //
    // The generation is bumped with it, and that is the ANNOUNCEMENT half.
    // Rewinding the index is how a reset works; it is not a reliable way to
    // TELL a reader one happened. `SpectrumView` used to infer it from
    // `writeCount()` going backwards, which is true only while the observed
    // count is still below the reader's last one — let the producer republish
    // past that value between two reader ticks and the reset is missed
    // outright, silently, with the reader's own EMA state left describing a
    // configuration the ring no longer holds. A monotonic counter the reader
    // compares against its own copy has no such window: any reset between two
    // observations changes it, however far the index has since travelled.
    //
    // WHAT THAT RETIREMENT DID AND DID NOT SAY (clarified round 7, because the
    // wording above reads as "the count is useless" and it is not). The
    // argument is an INSUFFICIENCY one: the count alone can MISS a reset. It is
    // not a soundness argument, and the converse still holds — a count strictly
    // BELOW one the same reader previously observed is PROOF of a reset, since
    // this index only ever decreases here. `SpectrumView::resetObserved`
    // therefore takes the count as a SECOND SUFFICIENT condition beside the
    // generation, which fires earlier when a reader observes this rewind before
    // the bump below. The generation remains the complete detector; the count is
    // never a replacement for it.
    //
    // Published AFTER the rewind, with release, so a reader that has ACQUIRED
    // the new generation cannot then read the pre-reset index. The opposite
    // skew (new index, old generation) is possible and is why the reader
    // samples the generation on BOTH sides of its batch — the same contract
    // `GrHistoryBuffer::resetEpoch()` states for the same question. It is a
    // plain generation, NOT that class's odd/even seqlock. **CORRECTED IN
    // ROUND 6, because the reason given here was wrong in a way that misled a
    // review.** It said "there is nothing here for a reader to observe
    // half-done". There is: a reader can observe the REWOUND INDEX before the
    // generation bump that announces it, because they are two atomics and the
    // rewind is stored first. What that costs is bounded and is NOT a broken
    // invariant — a reader that acquires the NEW generation is forced by the
    // release ordering below to see the rewind or later, so it can never
    // attribute pre-reset frames to a post-reset timeline; the reverse skew
    // only leaves the display one or more ticks stale, until the bump becomes
    // visible. That residual is KI-018. The TRUE half of the sentence is the
    // one the rest of this file leans on: this function writes one
    // atomic and touches no sample. THAT half IS LOAD-BEARING TWICE OVER —
    // it is also why this ring needs no `batchIntact`-style acquire FENCE on
    // the reader's closing check, where `GrHistoryBuffer` does: that ring's
    // clear WRITES THE PAYLOAD inside its epoch window, so a reader's payload
    // loads are the only thing that can witness it, and an acquire load would
    // leave those earlier relaxed loads unordered against the epoch re-read.
    // Here there is no clear-window store for a payload load to synchronise
    // through, and the producer's payload stores are relaxed with no release
    // fence before them, so an acquire fence would have nothing to attach to
    // ([atomics.fences]). See ADR-0011's third dated 2026-09-02 amendment.
    // NAMED PREMISE, because it is a plugin-API contract rather than a C++
    // guarantee: this runs from `prepare`, with audio stopped. The engine's
    // reallocation of every other ring it holds already depends on the same
    // thing, so the code is unsound far beyond this ring if it fails.
    void reset() noexcept
    {
        write.store (0, std::memory_order_release);
        resetGen.fetch_add (1, std::memory_order_release);
    }

    // --- gui thread ------------------------------------------------------
    // Reader side of the contract above. Monotonic across the instance's life
    // (one bump per `prepare`, so it cannot realistically wrap or alias);
    // sample it before and after a batch of reads and treat ANY change as
    // "the frames I just read may straddle a reset".
    uint32_t resetGeneration() const noexcept
    { return resetGen.load (std::memory_order_acquire); }

    // --- audio thread ----------------------------------------------------
    // Writes a whole block and publishes it with ONE release-store on the
    // write index (S9). Readers acquire the index and only copy frames
    // strictly below it, so a block becomes visible atomically -- partially
    // committed frames can never be observed. The synchronisation contract is
    // unchanged: the same single writer, the same single release/acquire pair
    // on the same atomic, just at block cadence instead of per sample.
    inline void pushBlock (const float* l, const float* r, int n) noexcept
    {
        auto w = write.load (std::memory_order_relaxed);
        const auto end = w + (uint64_t) n;
        if (n > capacity) // pathological block: only the newest frames can fit
        {
            l += n - capacity;
            r += n - capacity;
            w = end - (uint64_t) capacity;
            n = capacity;
        }
        // STILL at most two contiguous segments, and the segment arithmetic is
        // untouched; what changed is the store. The sibling's Wave-4 note said
        // the two `memcpy`s were equivalent to a per-sample masked store
        // because "intra-block store order was never observable" — true, and
        // beside the point that retired it: a `memcpy` is a PLAIN write, and a
        // plain write racing the reader's plain read is undefined however
        // unobservable its order is (the banner). Each sample is published
        // with a relaxed atomic store, which costs the loop its vectorisation
        // and nothing else: the same bytes, the same slots, the same single
        // release-store publication below, and the same values a reader gets
        // back. `left.data()` is hoisted so the element type's accessor cannot
        // reload the container per sample.
        const int idx   = (int) (w & mask);
        const int first = n < capacity - idx ? n : capacity - idx;
        auto* const dl = left.data();
        auto* const dr = right.data();
        for (int i = 0; i < first; ++i)
        {
            dl[idx + i].store (l[i]);
            dr[idx + i].store (r[i]);
        }
        for (int i = first; i < n; ++i)
        {
            dl[i - first].store (l[i]);
            dr[i - first].store (r[i]);
        }
        write.store (end, std::memory_order_release);
    }

    // --- gui thread ------------------------------------------------------
    // Monotonic total of frames ever written (uint64 -- never wraps in
    // practice). The same acquire load readLatest performs; lets a reader
    // detect "no new frames since last check" without copying any data.
    // Read-only: never mutates the ring and consumes nothing.
    uint64_t writeCount() const noexcept { return write.load (std::memory_order_acquire); }

    // Copies up to `count` most-recent frames (oldest first) into the caller's
    // buffers. Returns the number of frames actually copied.
    //
    // Same shape as `GrHistoryBuffer::peek` — a masked index into a ring the
    // producer is still writing. It needs no `capacity - 1` clamp, because
    // unlike that ring it never aliases the slot being filled: the reader takes
    // [w - count, w) and the producer writes from w, which are disjoint until
    // the producer laps. THE MARGIN IS A DISPLAY BOUND, NOT A CORRECTNESS ONE,
    // and the distinction is this repair's subject — a lapped read is DEFINED
    // (the banner), it is merely not the audio the caller asked for. Stated
    // exactly, because the figure this replaces was quoted as if it were an
    // invariant and was not:
    //   * The oldest frame copied sits `count` behind the head, so a push of
    //     `n` frames reaches that ONE slot at n >= capacity - count + 1
    //     (12289 for the only caller's 4096 of 16384) and covers the reader's
    //     WHOLE window only at n >= capacity. Two different thresholds.
    //   * As TIME, 12288 frames is 0.256 s at 48 kHz and 0.064 s at 192 kHz.
    //     It is a frame count; it is not a quarter of a second.
    //   * `reset()` rewinds the head. A reader holding a pre-reset `w` has a
    //     margin anywhere in [0, capacity), not `capacity - count`.
    //   * `n` is bounded by the engine's `maxBlock`, which is the HOST's
    //     `samplesPerBlock` with no upper clamp. Every other ring the engine
    //     holds is sized from `maxBlock`; this one is a fixed 16384 whatever
    //     the host prepares, and that sizing asymmetry is unenforced rather
    //     than deliberate. Possible by construction; no in-tree stimulus
    //     prepares more than 512, so it is unexercised here rather than known
    //     to be reachable in a shipping host.
    // A future capacity reduction or window widening narrows the DISPLAY
    // margin, not the legality; `count > capacity` alone would not catch it.
    int readLatest (float* dstL, float* dstR, int count) const noexcept
    { return readEndingAt (dstL, dstR, count, kNewest); }

    // THE SAME READ, ENDING WHERE THE CALLER SAYS (the fourth provenance delta,
    // 0.2.12). `readLatest` answers "the newest `count` frames THIS ring has",
    // which is the right question for a display with one ring and the wrong one
    // for a display with two: the engine publishes the input tap and the output
    // tap with one release-store each, back to back, so a reader can observe one
    // ring a chunk past the other and pair the input spectrum of chunk k with
    // the output spectrum of chunk k−1. `SpectrumView` therefore takes the
    // COMMON COMMITTED HEAD — `min` of the two indices, the newest frame both
    // taps have published — and asks each ring for the window ending there.
    // Measured before it did: the two analysed windows ended at different
    // indices on 1.28 % of ticks at 48 kHz / 512 and 4.70 % at 128 — the
    // producer publishing during the 132 µs FFT that sits between the two
    // reads — and a chunk of audio one tap had published alone reached one
    // trace and not the other.
    //
    // EVERY SAFETY ARGUMENT OF `readLatest` SURVIVES, because this is that
    // function with an upper bound on where the window ends:
    //   * The acquire load is the same load, taken here rather than by the
    //     caller, and `end` is CLAMPED to it. A caller's index can only be
    //     stale-LARGE — the other ring's head, or an index this ring has since
    //     rewound (`reset`) — and the clamp makes both cases read what this
    //     ring has actually published, never past it. That is what keeps the
    //     "copies strictly below the acquired index" contract true for an index
    //     the caller supplied.
    //   * The window is [e − count, e) with e ≤ w, so it stays disjoint from the
    //     slot the producer is filling at w. The DISPLAY margin against a lap
    //     narrows by exactly `w − e`: 12288 frames become 12288 − skew for the
    //     only caller's 4096 of 16384 — AND THAT SUBTRACTION REACHES ZERO,
    //     which the first draft of this comment named without following. A
    //     chunk longer than the margin laps the window the caller asked for:
    //     at `w − e ≥ 12289` the oldest requested frame is already overwritten,
    //     at `w − e ≥ 16384` all of it is. Reproduced exactly there — 0
    //     overwritten frames at a 12288-frame chunk, 1 at 12289, 712 at 13000,
    //     4096 at 16384 — so the read now CLAMPS ITS OWN START as well as its
    //     end (`oldestReadable`), and returns the shorter count rather than
    //     frames the producer has taken back. The caller that reads two rings
    //     as one span asks both for a length neither has to shorten, so the
    //     clamp is a backstop rather than the mechanism; see `SpectrumView::tick`.
    //   * Nothing else is touched: same mask, same relaxed payload loads, same
    //     oldest-first order, same short-read return.
    static constexpr uint64_t kNewest = ~(uint64_t) 0;   // "wherever this ring is"

    // THE OLDEST ABSOLUTE INDEX THIS RING CAN STILL SERVE. Slot `i & mask` holds
    // index `i` until the producer writes `i + capacity`, so everything below
    // `w − capacity` has been taken back. One acquire load, no payload touched,
    // and it is what a reader pairing TWO rings needs before it can choose a
    // window length both of them can honour: the pair's floor is the HIGHER of
    // the two, because a span is only common if both rings still hold it.
    //
    // It is a bound at the instant of the load, like every other answer this
    // ring gives a reader. A producer that advances afterwards raises the true
    // floor, which is the pre-existing display margin `readLatest` has always
    // had (a push of `capacity − count + 1` frames during the copy reaches the
    // oldest slot) — unchanged by any of this, and the reason the clamp inside
    // `readEndingAt` re-derives the floor from ITS OWN load rather than trusting
    // a caller's earlier one.
    uint64_t oldestReadable() const noexcept
    {
        const auto w = write.load (std::memory_order_acquire);
        return w > (uint64_t) capacity ? w - (uint64_t) capacity : 0;
    }

    int readEndingAt (float* dstL, float* dstR, int count, uint64_t end) const noexcept
    {
        const auto w = write.load (std::memory_order_acquire);
        const uint64_t e = end < w ? end : w;
        if (count > capacity) count = capacity;
        // Adapted (beyond the namespace): both ternary arms made unsigned —
        // the original's int arm trips -Wsign-conversion under this repo's
        // warning gate; the value range is unchanged (count ≤ capacity).
        uint64_t available = (e < (uint64_t) count) ? e : (uint64_t) count;
        uint64_t start = e - available;
        // …and the START is clamped to what the producer has not taken back, from
        // the SAME acquired index: a window ending inside the ring can still begin
        // outside it (a chunk longer than `capacity − count`), and returning those
        // frames would hand the caller audio it did not ask for while telling it
        // the count it wanted. Shortening is the honest answer and the one the
        // short-read contract already covers — `analyse` zero-pads a short read.
        const uint64_t oldest = w > (uint64_t) capacity ? w - (uint64_t) capacity : 0;
        if (start < oldest)
        {
            start     = oldest;
            available = e > start ? e - start : 0;
        }
        for (uint64_t i = 0; i < available; ++i)
        {
            const auto idx = (start + i) & mask;
            dstL[i] = left [idx].load();
            dstR[i] = right[idx].load();
        }
        return (int) available;
    }

private:
    std::vector<Sample> left, right;
    std::atomic<uint64_t>       write { 0 };
    std::atomic<uint32_t>       resetGen { 0 };   // see reset() / resetGeneration()
};

} // namespace anabasis
