/**
 * @file DiagSum_len.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Implementation of DiagSum::len -- returns the number of cells on a given diagonal.
 */
#include "common/CrossSum/DiagSum.h"

#include <algorithm>
#include <cstdint>

namespace crsce::common {
    /**
     * @name len
     * @brief Return the number of cells on diagonal k: min(k + 1, s, 2s - 1 - k).
     * @param k Diagonal index.
     * @return Number of cells on diagonal k.
     */
    std::uint16_t DiagSum::len(const std::uint16_t k) const {
        const auto k32 = static_cast<std::int32_t>(k);
        const auto s32 = static_cast<std::int32_t>(s_);
        return static_cast<std::uint16_t>(std::min({k32 + 1, s32, (2 * s32) - 1 - k32}));
    }
} // namespace crsce::common
