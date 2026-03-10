/**
 * @file DiagSum_cells.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Implementation of DiagSum::cells -- returns all cells on the same diagonal.
 */
#include "common/CrossSum/DiagSum.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace crsce::common {
    /**
     * @name cells
     * @brief Return all cells on the same diagonal as (r, c). The diagonal is defined by
     *        d = c - r + (s - 1). All (r', c') where c' - r' + (s - 1) = d and 0 <= r', c' < s.
     * @param r Row index of the cell.
     * @param c Column index of the cell.
     * @return Vector of (row, column) pairs for the diagonal.
     */
    std::vector<std::pair<std::uint16_t, std::uint16_t>>
    DiagSum::cells(const std::uint16_t r, const std::uint16_t c) const {
        const auto d = static_cast<std::int32_t>(lineIndex(r, c));
        const auto s = static_cast<std::int32_t>(s_);

        // On diagonal d, c' = r' + d - (s - 1).
        // We need 0 <= r' < s and 0 <= c' < s.
        // c' >= 0  =>  r' >= (s - 1) - d
        // c' < s   =>  r' < s + (s - 1) - d  =  2s - 1 - d
        const std::int32_t r_min = std::max(static_cast<std::int32_t>(0), (s - 1) - d);
        const std::int32_t r_max = std::min(s, (2 * s) - 1 - d);

        std::vector<std::pair<std::uint16_t, std::uint16_t>> result;
        result.reserve(static_cast<std::size_t>(r_max - r_min));
        for (std::int32_t rr = r_min; rr < r_max; ++rr) {
            const auto cc = static_cast<std::uint16_t>(rr + d - (s - 1));
            result.emplace_back(static_cast<std::uint16_t>(rr), cc);
        }
        return result;
    }
} // namespace crsce::common
