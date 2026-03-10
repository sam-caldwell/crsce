/**
 * @file ToroidalSlopeSum_cells.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Implementation of ToroidalSlopeSum::cells -- enumerate all cells on a toroidal-slope line.
 */
#include "common/CrossSum/ToroidalSlopeSum.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace crsce::common {
    /**
     * @name cells
     * @brief Return all cells on the same toroidal-slope line as (r, c).
     *
     * For line k = lineIndex(r, c), cell enumeration is:
     *   (t, (k + slope_ * t) % s_) for t = 0..s_-1.
     *
     * @param r Row index of the cell.
     * @param c Column index of the cell.
     * @return Vector of (row, column) pairs for the line.
     */
    std::vector<std::pair<std::uint16_t, std::uint16_t>>
    ToroidalSlopeSum::cells(const std::uint16_t r, const std::uint16_t c) const {
        const auto k = static_cast<std::uint32_t>(lineIndex(r, c));
        const auto slope32 = static_cast<std::uint32_t>(slope_);
        const auto s32 = static_cast<std::uint32_t>(s_);

        std::vector<std::pair<std::uint16_t, std::uint16_t>> result;
        result.reserve(s_);
        for (std::uint16_t t = 0; t < s_; ++t) {
            const auto col = static_cast<std::uint16_t>((k + slope32 * t) % s32);
            result.emplace_back(t, col);
        }
        return result;
    }
} // namespace crsce::common
