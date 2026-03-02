/**
 * @file Csm_popcount.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Csm::popcount() implementation.
 */
#include "common/Csm/Csm.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace crsce::common {
    /**
     * @name popcount
     * @brief Count the number of set bits in row r.
     * @param r Row index in [0, kS).
     * @return Number of set bits (0..kS).
     * @throws std::out_of_range if r >= kS.
     */
    auto Csm::popcount(const std::uint16_t r) const -> std::uint16_t {
        if (r >= kS) {
            throw std::out_of_range("Csm::popcount: row index out of range");
        }
        std::uint16_t count = 0;
        for (std::uint16_t w = 0; w < kWordsPerRow; ++w) {
            count += static_cast<std::uint16_t>(std::popcount(rows_[r][w])); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }
        return count;
    }
} // namespace crsce::common
