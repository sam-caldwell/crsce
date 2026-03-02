/**
 * @file AntiDiagSum_cells.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Implementation of AntiDiagSum::cells -- returns all cells on the same anti-diagonal.
 */
#include "common/CrossSum/AntiDiagSum.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace crsce::common {
    /**
     * @name cells
     * @brief Return all cells on the same anti-diagonal as (r, c). The anti-diagonal is defined by
     *        x = r + c. All (r', c') where r' + c' = x and 0 <= r', c' < s.
     * @param r Row index of the cell.
     * @param c Column index of the cell.
     * @return Vector of (row, column) pairs for the anti-diagonal.
     */
    std::vector<std::pair<std::uint16_t, std::uint16_t>>
    AntiDiagSum::cells(const std::uint16_t r, const std::uint16_t c) const {
        const auto x = static_cast<std::int32_t>(lineIndex(r, c));
        const auto s = static_cast<std::int32_t>(s_);

        // On anti-diagonal x, c' = x - r'.
        // We need 0 <= r' < s and 0 <= c' < s.
        // c' >= 0  =>  r' <= x
        // c' < s   =>  r' > x - s
        const std::int32_t r_min = std::max(static_cast<std::int32_t>(0), x - s + 1);
        const std::int32_t r_max = std::min(s - 1, x) + 1;

        std::vector<std::pair<std::uint16_t, std::uint16_t>> result;
        result.reserve(static_cast<std::size_t>(r_max - r_min));
        for (std::int32_t rr = r_min; rr < r_max; ++rr) {
            const auto cc = static_cast<std::uint16_t>(x - rr);
            result.emplace_back(static_cast<std::uint16_t>(rr), cc);
        }
        return result;
    }
} // namespace crsce::common
