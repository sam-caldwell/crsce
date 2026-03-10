/**
 * @file ColSum_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Constructor for the ColSum (VSM) cross-sum vector.
 */
#include "common/CrossSum/ColSum.h"

#include "common/CrossSum/CrossSumVector.h"

#include <cstdint>

namespace crsce::common {
    /**
     * @name ColSum
     * @brief Construct a column-sum vector for a matrix of dimension s. Allocates s sums initialized to zero.
     * @param s The matrix dimension.
     */
    ColSum::ColSum(const std::uint16_t s)
        : CrossSumVector(s) {
        sums_.resize(s, 0);
    }
} // namespace crsce::common
