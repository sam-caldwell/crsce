/**
 * @file CompressedPayload_diagLen.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::diagLen() implementation.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <algorithm>
#include <cstdint>

namespace crsce::common::format {

    /**
     * @name diagLen
     * @brief Compute the number of cells on diagonal (or anti-diagonal) k.
     * @details For a kS x kS matrix with kDiagCount = 2*kS - 1 diagonals,
     *          the number of cells on diagonal k is min(k+1, kS, kDiagCount - k).
     * @param k Diagonal index in [0, kDiagCount).
     * @return Number of cells on diagonal k.
     */
    std::uint16_t CompressedPayload::diagLen(const std::uint16_t k) {
        const auto k32 = static_cast<std::int32_t>(k);
        const auto s32 = static_cast<std::int32_t>(kS);
        const auto dc32 = static_cast<std::int32_t>(kDiagCount);
        return static_cast<std::uint16_t>(std::min({k32 + 1, s32, dc32 - k32}));
    }

} // namespace crsce::common::format
