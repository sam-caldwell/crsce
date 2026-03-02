/**
 * @file CrossSumVector_getByIndex.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Implementation of CrossSumVector::getByIndex -- direct index access for serialization.
 */
#include "common/CrossSum/CrossSumVector.h"

#include <cstdint>

namespace crsce::common {
    /**
     * @name getByIndex
     * @brief Direct index access for serialization: retrieve the sum at line index k.
     * @param k Line index.
     * @return The sum stored at index k.
     */
    std::uint16_t CrossSumVector::getByIndex(const std::uint16_t k) const {
        return sums_[k];
    }
} // namespace crsce::common
