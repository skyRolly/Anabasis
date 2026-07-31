#pragma once

// ============================================================================
//  AbSlotIndex — dependency-free clamp for the A/B slot index at every
//  deserialisation boundary (schema v1 read rules: indices clamped).
//  Copy of the Anamorph idiom (Anamorph:src/AbSlotIndex.h:15-25 [Verified],
//  ADR-0009) — kept header-only and JUCE-free so both test targets can use it.
// ============================================================================

namespace anabasis
{

inline constexpr int kNumAbSlots = 2;

constexpr int clampAbSlotIndex (int index) noexcept
{
    return index < 0 ? 0 : (index >= kNumAbSlots ? kNumAbSlots - 1 : index);
}

} // namespace anabasis
