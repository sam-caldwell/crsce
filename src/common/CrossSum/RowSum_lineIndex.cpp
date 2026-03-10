/**
 * @file RowSum_lineIndex.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Implementation of RowSum::lineIndex -- maps a cell to its row index.
 */
#include "common/CrossSum/RowSum.h"

#include <cstdint>

namespace crsce::common {
    /**
     * @name lineIndex
     * @brief Map cell (r, c) to its line index. For row sums, returns r.
     * @param r Row index.
     * @param c Column index (unused).
     * @return r
     */
    std::uint16_t RowSum::lineIndex(const std::uint16_t r, [[maybe_unused]] const std::uint16_t c) const {
        return r;
    }
} // namespace crsce::common
