/**
 * @file DiagSum_lineIndex.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Implementation of DiagSum::lineIndex -- maps a cell to its diagonal index.
 */
#include "common/CrossSum/DiagSum.h"

#include <cstdint>

namespace crsce::common {
    /**
     * @name lineIndex
     * @brief Map cell (r, c) to its diagonal index: c - r + (s - 1).
     * @param r Row index.
     * @param c Column index.
     * @return c - r + (s - 1)
     */
    std::uint16_t DiagSum::lineIndex(const std::uint16_t r, const std::uint16_t c) const {
        return static_cast<std::uint16_t>(c - r + (s_ - 1));
    }
} // namespace crsce::common
