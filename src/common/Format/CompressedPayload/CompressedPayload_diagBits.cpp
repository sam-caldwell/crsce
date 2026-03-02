/**
 * @file CompressedPayload_diagBits.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::diagBits() implementation.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <bit>
#include <cstdint>

namespace crsce::common::format {

    /**
     * @name diagBits
     * @brief Compute the number of bits needed to encode a value on diagonal k.
     * @details Returns ceil(log2(diagLen(k) + 1)), which is the minimum number
     *          of bits needed to represent any value in [0, diagLen(k)].
     *          Uses std::bit_width for an efficient integer-only computation:
     *          bit_width(n) = floor(log2(n)) + 1 for n > 0, which equals ceil(log2(n+1)).
     * @param k Diagonal index in [0, kDiagCount).
     * @return Number of bits (1..9).
     */
    std::uint8_t CompressedPayload::diagBits(const std::uint16_t k) {
        const auto len = static_cast<std::uint32_t>(diagLen(k));
        // ceil(log2(len + 1)) == bit_width(len) for len >= 1
        return static_cast<std::uint8_t>(std::bit_width(len));
    }

} // namespace crsce::common::format
