/**
 * @file ConstraintStore_getFirstUnassigned.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ConstraintStore::getFirstUnassigned -- O(1) per-word bitwise scan for next unassigned cell.
 */
#include "decompress/Solvers/ConstraintStore.h"

#include <cstdint>
#include <optional>
#include <utility>

namespace crsce::decompress::solvers {

    /**
     * @name kLastWordMask
     * @brief Mask for the last word of each row (only bits 0..kS%64-1 are valid).
     *
     * kS = 127, so 511 % 64 = 63, meaning bits 0..62 are valid in word 7.
     * The mask has 63 low bits set: (1ULL << 63) - 1.
     */
    static constexpr std::uint64_t kLastWordMask = (std::uint64_t{1} << (ConstraintStore::kS % 64)) - 1;

    /**
     * @name getFirstUnassigned
     * @brief Find the first unassigned cell at or after startRow using ctzll bitwise scanning.
     * @param startRow Row index to begin scanning from.
     * @return Pair (r, c) of the first unassigned cell, or nullopt if all assigned.
     */
    auto ConstraintStore::getFirstUnassigned(const std::uint16_t startRow) const
        -> std::optional<std::pair<std::uint16_t, std::uint16_t>> {

        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
        for (std::uint16_t r = startRow; r < kS; ++r) {
            // Skip rows with no unknowns (fast check via stats)
            if (stats_[r].unknown == 0) {
                continue;
            }

            for (std::uint16_t w = 0; w < 2; ++w) {
                // Unassigned bits are 0 in assigned_, so invert to find them
                std::uint64_t free = ~assigned_[r][w];

                // Mask invalid bits in the last word (bits >= kS%64)
                if (w == 1) {
                    free &= kLastWordMask;
                }

                if (free != 0) {
                    const auto bit = static_cast<std::uint16_t>(__builtin_ctzll(free));
                    return std::pair{r, static_cast<std::uint16_t>((w * 64) + bit)};
                }
            }
        }
        // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)

        return std::nullopt;
    }
} // namespace crsce::decompress::solvers
