/**
 * @file RowSum_cells.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Implementation of RowSum::cells -- returns all cells in the same row.
 */
#include "common/CrossSum/RowSum.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace crsce::common {
    /**
     * @name cells
     * @brief Return all cells in the same row as (r, c): all (r, c') for c' in [0, s).
     * @param r Row index of the cell.
     * @param c Column index of the cell (unused for enumeration, row is determined by r).
     * @return Vector of (row, column) pairs for the entire row.
     */
    std::vector<std::pair<std::uint16_t, std::uint16_t>>
    RowSum::cells(const std::uint16_t r, [[maybe_unused]] const std::uint16_t c) const {
        std::vector<std::pair<std::uint16_t, std::uint16_t>> result;
        result.reserve(s_);
        for (std::uint16_t col = 0; col < s_; ++col) {
            result.emplace_back(r, col);
        }
        return result;
    }
} // namespace crsce::common
