/**
 * @file kRowAlt0101Digest.h
 * @brief Precomputed LH digest for alternating 0,1 pattern row.
 * © Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <cstdint>

namespace crsce::decompress::known_lh {
    /**
     * @name kRowAlt0101Digest
     * @brief LH digest for alternating 0,1,0,1,... row pattern.
     */
    inline constexpr std::array<std::uint8_t, 32> kRowAlt0101Digest{
        0xA9, 0x93, 0x97, 0xAE, 0x54, 0xEC, 0xA3, 0x37, 0x67, 0x88, 0x63, 0xE6, 0x39, 0x19, 0xB7, 0x62,
        0x8D, 0x18, 0xBC, 0xE4, 0x5F, 0x41, 0xAC, 0x12, 0xBB, 0xD5, 0x25, 0x05, 0xB7, 0xE3, 0x08, 0x71
    };
} // namespace crsce::decompress::known_lh
