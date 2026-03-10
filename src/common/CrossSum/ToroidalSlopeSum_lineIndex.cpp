/**
 * @file ToroidalSlopeSum_lineIndex.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Implementation of ToroidalSlopeSum::lineIndex -- toroidal modular arithmetic.
 */
#include "common/CrossSum/ToroidalSlopeSum.h"

#include <cstdint>

namespace crsce::common {
    /**
     * @name lineIndex
     * @brief Map cell (r, c) to its toroidal-slope line index: ((c - slope_ * r) % s_ + s_) % s_.
     * @param r Row index.
     * @param c Column index.
     * @return The line index for cell (r, c).
     */
    std::uint16_t ToroidalSlopeSum::lineIndex(const std::uint16_t r, const std::uint16_t c) const {
        return slopeLineIndex(r, c, slope_, s_);
    }
} // namespace crsce::common
