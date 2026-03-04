/**
 * @file ToroidalSlopeSum_len.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Implementation of ToroidalSlopeSum::len -- always returns s (toroidal lines wrap).
 */
#include "common/CrossSum/ToroidalSlopeSum.h"

#include <cstdint>

namespace crsce::common {
    /**
     * @name len
     * @brief Return the number of cells on line k. For toroidal slopes, every line has s cells.
     * @param k Line index (unused; all toroidal-slope lines have the same length).
     * @return s
     */
    std::uint16_t ToroidalSlopeSum::len([[maybe_unused]] const std::uint16_t k) const {
        return s_;
    }
} // namespace crsce::common
