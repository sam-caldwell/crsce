/**
 * @file CrossSumVector_setByIndex.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Implementation of CrossSumVector::setByIndex -- direct index access for serialization.
 */
#include "common/CrossSum/CrossSumVector.h"

#include <cstdint>

namespace crsce::common {
    /**
     * @name setByIndex
     * @brief Direct index access for serialization: set the sum at line index k.
     * @param k Line index.
     * @param value The value to store at index k.
     */
    void CrossSumVector::setByIndex(const std::uint16_t k, const std::uint16_t value) {
        sums_[k] = value;
    }
} // namespace crsce::common
