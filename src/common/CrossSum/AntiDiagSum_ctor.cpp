/**
 * @file AntiDiagSum_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Constructor for the AntiDiagSum (XSM) cross-sum vector.
 */
#include "common/CrossSum/AntiDiagSum.h"

#include "common/CrossSum/CrossSumVector.h"

#include <cstddef>
#include <cstdint>

namespace crsce::common {
    /**
     * @name AntiDiagSum
     * @brief Construct an anti-diagonal-sum vector for a matrix of dimension s.
     *        Allocates 2s - 1 sums initialized to zero.
     * @param s The matrix dimension.
     */
    AntiDiagSum::AntiDiagSum(const std::uint16_t s)
        : CrossSumVector(s) {
        sums_.resize(static_cast<std::size_t>((2U * s) - 1U), 0);
    }
} // namespace crsce::common
