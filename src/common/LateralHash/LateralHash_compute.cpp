/**
 * @file LateralHash_compute.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief LateralHash::compute() implementation (CRC-32 for B.57).
 */
#include "common/LateralHash/LateralHash.h"

#include <array>
#include <cstdint>

#include "common/Util/crc32_ieee.h"

namespace crsce::common {
    /**
     * @name compute
     * @brief Compute the CRC-32 digest of a 127-bit row (2 uint64 words = 16 bytes).
     * @param row The 2 x uint64_t words representing the row, MSB-first bit ordering.
     * @return 4-byte CRC-32 digest.
     * @throws None
     */
    std::array<std::uint8_t, LateralHash::kDigestBytes>
    LateralHash::compute(const std::array<std::uint64_t, 2> &row) {
        // Convert 2 uint64 words to 16 big-endian bytes
        std::array<std::uint8_t, 16> msg{};
        for (int w = 0; w < 2; ++w) {
            for (int b = 7; b >= 0; --b) {
                msg[(w * 8) + (7 - b)] = static_cast<std::uint8_t>(row[w] >> (b * 8)); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            }
        }
        const std::uint32_t crc = util::crc32_ieee(msg.data(), msg.size());
        return {
            static_cast<std::uint8_t>((crc >> 24U) & 0xFFU),
            static_cast<std::uint8_t>((crc >> 16U) & 0xFFU),
            static_cast<std::uint8_t>((crc >> 8U) & 0xFFU),
            static_cast<std::uint8_t>(crc & 0xFFU)
        };
    }
} // namespace crsce::common
