/**
 * @file Csm_getRow.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Csm::getRow() implementation.
 */
#include "common/Csm/Csm.h"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace crsce::common {
    /**
     * @name getRow
     * @brief Return a copy of the 8-word bitset for row r.
     * @param r Row index in [0, kS).
     * @return Copy of the row's 8 x uint64_t words.
     * @throws std::out_of_range if r >= kS.
     */
    auto Csm::getRow(const std::uint16_t r) const -> std::array<std::uint64_t, kWordsPerRow> {
        if (r >= kS) {
            throw std::out_of_range("Csm::getRow: row index out of range");
        }
        return rows_[r]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    }
} // namespace crsce::common
