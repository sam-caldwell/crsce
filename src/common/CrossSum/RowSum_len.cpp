/**
 * @file RowSum_len.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Implementation of RowSum::len -- returns the number of cells on a given row.
 */
#include "common/CrossSum/RowSum.h"

#include <cstdint>

namespace crsce::common {
    /**
     * @name len
     * @brief Return the number of cells on line k. For row sums, every row has s cells.
     * @param k Line index (unused; all rows have the same length).
     * @return s
     */
    std::uint16_t RowSum::len([[maybe_unused]] const std::uint16_t k) const {
        return s_;
    }
} // namespace crsce::common
