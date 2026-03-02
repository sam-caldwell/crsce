/**
 * @file ColSum_lineIndex.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Implementation of ColSum::lineIndex -- maps a cell to its column index.
 */
#include "common/CrossSum/ColSum.h"

#include <cstdint>

namespace crsce::common {
    /**
     * @name lineIndex
     * @brief Map cell (r, c) to its line index. For column sums, returns c.
     * @param r Row index (unused).
     * @param c Column index.
     * @return c
     */
    std::uint16_t ColSum::lineIndex([[maybe_unused]] const std::uint16_t r, const std::uint16_t c) const {
        return c;
    }
} // namespace crsce::common
