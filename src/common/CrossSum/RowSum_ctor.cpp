/**
 * @file RowSum_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Constructor for the RowSum (LSM) cross-sum vector.
 */
#include "common/CrossSum/RowSum.h"

#include "common/CrossSum/CrossSumVector.h"

#include <cstdint>

namespace crsce::common {
    /**
     * @name RowSum
     * @brief Construct a row-sum vector for a matrix of dimension s. Allocates s sums initialized to zero.
     * @param s The matrix dimension.
     */
    RowSum::RowSum(const std::uint16_t s)
        : CrossSumVector(s) {
        sums_.resize(s, 0);
    }
} // namespace crsce::common
