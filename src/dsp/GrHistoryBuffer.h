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
//    rule and its OpenGL nuance).
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
    struct Entry
    {
        float grDb  = 0.0f;
        float peak  = 0.0f;
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
        // A seqlock's OPENING increment needs a fence, not a release store: a
        // release orders earlier writes before itself and says nothing about
        // later ones, so the clear below could be observed above the odd epoch
        // — a reader would then see half-cleared entries while the epoch still
        // read "stable", which is the one case this guard exists to exclude.
        // Relaxed increment + release fence is the canonical writer opening;
        // the closing increment stays a release store, where the ordering it
        // does give (clear-before-even) is exactly the one required.
        resetGuard.fetch_add (1, std::memory_order_relaxed);   // odd: clearing
        std::atomic_thread_fence (std::memory_order_release);
        for (auto& e : entries)
            e = {};
        writeIndex.store (0, std::memory_order_release);
        resetGuard.fetch_add (1, std::memory_order_release);   // even: stable
    }

    // Reader side of the contract above. Even = stable; sample before and
    // after a batch of peeks, discard the batch if it moved or was odd.
    uint32_t resetEpoch() const noexcept
    { return resetGuard.load (std::memory_order_acquire); }

    // Audio thread, once per block. The entry is written FIRST, the index
    // release-stored AFTER — that ordering is the whole synchronisation.
    void push (float grDb, float peak) noexcept
    {
        const auto i = writeIndex.load (std::memory_order_relaxed);
        entries[(size_t) (i & (int64_t) kMask)] = { grDb, peak };
        writeIndex.store (i + 1, std::memory_order_release);
    }

    // Reader side: how many entries have ever been pushed.
    int64_t available() const noexcept
    { return writeIndex.load (std::memory_order_acquire); }

    // Stateless peek at entry n (absolute index). Entries older than kSize
    // behind the head have been overwritten; the caller clamps its window.
    Entry peek (int64_t n) const noexcept
    { return entries[(size_t) (n & (int64_t) kMask)]; }

private:
    Entry entries[kSize] = {};
    std::atomic<int64_t>  writeIndex { 0 };
    std::atomic<uint32_t> resetGuard { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrHistoryBuffer)
};

} // namespace anabasis
