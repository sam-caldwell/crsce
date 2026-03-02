/**
 * @file CrossSumVector_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Constructor for the CrossSumVector abstract base class.
 */
#include "common/CrossSum/CrossSumVector.h"

#include <cstdint>

namespace crsce::common {
    /**
     * @name CrossSumVector
     * @brief Construct a cross-sum vector for a matrix of dimension s, initializing all sums to zero.
     * @param s The matrix dimension (number of rows and columns).
     */
    CrossSumVector::CrossSumVector(const std::uint16_t s)
        : s_(s) {
    }
} // namespace crsce::common
