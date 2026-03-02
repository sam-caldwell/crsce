/**
 * @file DiagSum_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Constructor for the DiagSum (DSM) cross-sum vector.
 */
#include "common/CrossSum/DiagSum.h"

#include "common/CrossSum/CrossSumVector.h"

#include <cstddef>
#include <cstdint>

namespace crsce::common {
    /**
     * @name DiagSum
     * @brief Construct a diagonal-sum vector for a matrix of dimension s. Allocates 2s - 1 sums initialized to zero.
     * @param s The matrix dimension.
     */
    DiagSum::DiagSum(const std::uint16_t s)
        : CrossSumVector(s) {
        sums_.resize(static_cast<std::size_t>((2U * s) - 1U), 0);
    }
} // namespace crsce::common
