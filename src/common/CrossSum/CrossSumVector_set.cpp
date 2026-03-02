/**
 * @file CrossSumVector_set.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Implementation of CrossSumVector::set -- accumulates a cell value into the appropriate sum.
 */
#include "common/CrossSum/CrossSumVector.h"

#include <cstdint>

namespace crsce::common {
    /**
     * @name set
     * @brief Accumulate a cell's value into the appropriate sum for the line containing cell (r, c).
     * @param r Row index of the cell.
     * @param c Column index of the cell.
     * @param v Value to accumulate (typically 0 or 1).
     */
    void CrossSumVector::set(const std::uint16_t r, const std::uint16_t c, const std::uint8_t v) {
        sums_[lineIndex(r, c)] += v;
    }
} // namespace crsce::common
