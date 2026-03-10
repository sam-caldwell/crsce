/**
 * @file CrossSumVector_get.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Implementation of CrossSumVector::get -- retrieves the sum for a given cell's line.
 */
#include "common/CrossSum/CrossSumVector.h"

#include <cstdint>

namespace crsce::common {
    /**
     * @name get
     * @brief Retrieve the current sum for the line containing cell (r, c).
     * @param r Row index of the cell.
     * @param c Column index of the cell.
     * @return The sum for the line containing (r, c).
     */
    std::uint16_t CrossSumVector::get(const std::uint16_t r, const std::uint16_t c) const {
        return sums_[lineIndex(r, c)];
    }
} // namespace crsce::common
