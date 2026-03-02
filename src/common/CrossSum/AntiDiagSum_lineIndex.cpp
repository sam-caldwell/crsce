/**
 * @file AntiDiagSum_lineIndex.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Implementation of AntiDiagSum::lineIndex -- maps a cell to its anti-diagonal index.
 */
#include "common/CrossSum/AntiDiagSum.h"

#include <cstdint>

namespace crsce::common {
    /**
     * @name lineIndex
     * @brief Map cell (r, c) to its anti-diagonal index: r + c.
     * @param r Row index.
     * @param c Column index.
     * @return r + c
     */
    std::uint16_t AntiDiagSum::lineIndex(const std::uint16_t r, const std::uint16_t c) const {
        return static_cast<std::uint16_t>(r + c);
    }
} // namespace crsce::common
