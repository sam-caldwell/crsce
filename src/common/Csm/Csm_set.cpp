/**
 * @file Csm_set.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Csm::set() implementation.
 */
#include "common/Csm/Csm.h"

#include <cstdint>
#include <stdexcept>

namespace crsce::common {
    /**
     * @name set
     * @brief Write bit (r, c) in the matrix to v using MSB-first indexing.
     * @param r Row index in [0, kS).
     * @param c Column index in [0, kS).
     * @param v Bit value: 0 or 1.
     * @throws std::out_of_range if r or c >= kS.
     */
    auto Csm::set(const std::uint16_t r, const std::uint16_t c, const std::uint8_t v) -> void {
        if (r >= kS || c >= kS) {
            throw std::out_of_range("Csm::set: index out of range");
        }
        const auto word = static_cast<std::uint16_t>(c / 64);
        const auto bit = static_cast<std::uint16_t>(63 - (c % 64));
        if (v != 0) {
            rows_[r][word] |= (1ULL << bit); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        } else {
            rows_[r][word] &= ~(1ULL << bit); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }
    }
} // namespace crsce::common
