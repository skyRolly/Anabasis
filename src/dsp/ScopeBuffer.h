#pragma once

// Provenance (ADR-0009): copied from Anamorph src/dsp/ScopeBuffer.h:1-93 @ b6a3db8
// (namespace only). The §2.9 spectrum capture rings instantiate it — the
// THREAD_MODEL planned edge, now implemented on the SPSC ring row.

#include <atomic>
#include <vector>
#include <cstdint>
#include <cstring>

namespace anabasis
{

// ============================================================================
//  ScopeBuffer
//
//  Lock-free single-producer ring buffer of stereo samples. The audio thread
//  (producer) only writes; all reads happen on the GUI (message) thread as
//  stateless peeks (vectorscope + spectrum imager). No locks, no allocation
//  on either side.
//
//  Capacity is a power of two so wrap is a cheap mask.
// ============================================================================
class ScopeBuffer
{
public:
    static constexpr int capacity = 1 << 14; // 16384 stereo frames
    static constexpr int mask     = capacity - 1;

    // Adapted beyond the namespace (the provenance header's one functional
    // delta): storage lives on the HEAP, not inline in the object. Anamorph
    // holds one ScopeBuffer; Anabasis's engine holds two, and 2 × 128 KB of
    // inline arrays ride along with EVERY engine — including the processors
    // the state suite builds on the STACK, where Windows' 1 MB default
    // overflowed (the Linux 8 MB default hid it; the crash ate its own
    // buffered output, which is why CI showed exit 1 and nothing else).
    // Allocation happens HERE, at construction on a non-audio thread; the
    // audio-thread push path still never allocates.
    ScopeBuffer() : left ((size_t) capacity, 0.0f), right ((size_t) capacity, 0.0f)
    { write.store (0, std::memory_order_relaxed); }

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
        // At most two contiguous segments instead of a per-sample masked store
        // (Wave 4): the ring bytes and the single release-store publication are
        // identical -- readers only ever copy frames strictly below the index
        // they acquire, so intra-block store order was never observable.
        const int idx   = (int) (w & mask);
        const int first = n < capacity - idx ? n : capacity - idx;
        std::memcpy (left.data()  + idx, l, (size_t) first * sizeof (float));
        std::memcpy (right.data() + idx, r, (size_t) first * sizeof (float));
        if (n > first)
        {
            std::memcpy (left.data(),  l + first, (size_t) (n - first) * sizeof (float));
            std::memcpy (right.data(), r + first, (size_t) (n - first) * sizeof (float));
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
    int readLatest (float* dstL, float* dstR, int count) const noexcept
    {
        const auto w = write.load (std::memory_order_acquire);
        if (count > capacity) count = capacity;
        // Adapted (beyond the namespace): both ternary arms made unsigned —
        // the original's int arm trips -Wsign-conversion under this repo's
        // warning gate; the value range is unchanged (count ≤ capacity).
        const uint64_t available = (w < (uint64_t) count) ? w : (uint64_t) count;
        const uint64_t start = w - available;
        for (uint64_t i = 0; i < available; ++i)
        {
            const auto idx = (start + i) & mask;
            dstL[i] = left [idx];
            dstR[i] = right[idx];
        }
        return (int) available;
    }

private:
    std::vector<float> left, right;
    std::atomic<uint64_t>       write { 0 };
};

} // namespace anabasis
