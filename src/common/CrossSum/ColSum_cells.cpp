/**
 * @file ColSum_cells.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Implementation of ColSum::cells -- returns all cells in the same column.
 */
#include "common/CrossSum/ColSum.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace crsce::common {
    /**
     * @name cells
     * @brief Return all cells in the same column as (r, c): all (r', c) for r' in [0, s).
     * @param r Row index of the cell (unused for enumeration, column is determined by c).
     * @param c Column index of the cell.
     * @return Vector of (row, column) pairs for the entire column.
     */
    std::vector<std::pair<std::uint16_t, std::uint16_t>>
    ColSum::cells([[maybe_unused]] const std::uint16_t r, const std::uint16_t c) const {
        std::vector<std::pair<std::uint16_t, std::uint16_t>> result;
        result.reserve(s_);
        for (std::uint16_t row = 0; row < s_; ++row) {
            result.emplace_back(row, c);
        }
        return result;
    }
} // namespace crsce::common
