/**
 * @file CrossSumVector_size.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Implementation of CrossSumVector::size -- returns the number of elements in the vector.
 */
#include "common/CrossSum/CrossSumVector.h"

#include <cstdint>

namespace crsce::common {
    /**
     * @name size
     * @brief Return the number of elements (lines) in this cross-sum vector.
     * @return The number of lines, equal to the length of the internal sums storage.
     */
    std::uint16_t CrossSumVector::size() const {
        return static_cast<std::uint16_t>(sums_.size());
    }
} // namespace crsce::common
