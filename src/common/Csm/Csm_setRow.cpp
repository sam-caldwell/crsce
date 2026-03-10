/**
 * @file Csm_setRow.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Csm::setRow() implementation.
 */
#include "common/Csm/Csm.h"

#include <array>
#include <cstdint>

namespace crsce::common {
    /**
     * @name setRow
     * @brief Copy an 8-word bitset directly into row r.
     * @param r Row index in [0, kS).
     * @param data The 8 x uint64_t words to set as the row.
     * @throws None
     */
    void Csm::setRow(const std::uint16_t r, const std::array<std::uint64_t, kWordsPerRow> &data) {
        rows_[r] = data; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    }
} // namespace crsce::common
