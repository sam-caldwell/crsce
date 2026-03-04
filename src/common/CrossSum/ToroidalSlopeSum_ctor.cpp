/**
 * @file ToroidalSlopeSum_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Constructor for the ToroidalSlopeSum cross-sum vector.
 */
#include "common/CrossSum/ToroidalSlopeSum.h"

#include "common/CrossSum/CrossSumVector.h"

#include <cstdint>

namespace crsce::common {
    /**
     * @name ToroidalSlopeSum
     * @brief Construct a toroidal-slope sum vector. Allocates s sums initialized to zero.
     * @param s The matrix dimension.
     * @param slope The slope parameter p.
     */
    ToroidalSlopeSum::ToroidalSlopeSum(const std::uint16_t s, const std::uint16_t slope)
        : CrossSumVector(s), slope_(slope) {
        sums_.resize(s, 0);
    }
} // namespace crsce::common
