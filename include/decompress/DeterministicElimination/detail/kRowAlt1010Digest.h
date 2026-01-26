/**
 * @file kRowAlt1010Digest.h
 * @brief Precomputed LH digest for alternating 1,0 pattern row.
 * © Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <cstdint>

namespace crsce::decompress::known_lh {
    /**
     * @name kRowAlt1010Digest
     * @brief LH digest for alternating 1,0,1,0,... row pattern.
     */
    inline constexpr std::array<std::uint8_t, 32> kRowAlt1010Digest{
        0xE5, 0x33, 0x12, 0xDC, 0x65, 0x55, 0x90, 0x85, 0xA9, 0xCF, 0xCE, 0x5C, 0x43, 0xEC, 0xA2, 0x57,
        0xD2, 0xB2, 0xD6, 0xC0, 0xD8, 0x79, 0x6F, 0x9B, 0xFD, 0xE7, 0x3B, 0x35, 0xF2, 0x25, 0x22, 0x3F
    };
} // namespace crsce::decompress::known_lh
