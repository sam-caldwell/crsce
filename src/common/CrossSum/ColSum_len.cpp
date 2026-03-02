/**
 * @file ColSum_len.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Implementation of ColSum::len -- returns the number of cells on a given column.
 */
#include "common/CrossSum/ColSum.h"

#include <cstdint>

namespace crsce::common {
    /**
     * @name len
     * @brief Return the number of cells on line k. For column sums, every column has s cells.
     * @param k Line index (unused; all columns have the same length).
     * @return s
     */
    std::uint16_t ColSum::len([[maybe_unused]] const std::uint16_t k) const {
        return s_;
    }
} // namespace crsce::common
