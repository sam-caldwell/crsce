/**
 * @file kRowOnesDigest.h
 * @brief Precomputed LH digest for the all-ones row.
 * © Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <cstdint>

namespace crsce::decompress::known_lh {
    /**
     * @name kRowOnesDigest
     * @brief LH digest for a row of all ones.
     */
    inline constexpr std::array<std::uint8_t, 32> kRowOnesDigest{
        0x7A, 0x88, 0xC3, 0x87, 0xEB, 0xB4, 0xB8, 0x44, 0x1F, 0x92, 0x1A, 0x32, 0x33, 0x70, 0xBE, 0xFF,
        0x5D, 0x53, 0xE3, 0xDE, 0xE3, 0x74, 0xAD, 0xAA, 0x79, 0x68, 0xE8, 0x82, 0x3F, 0x2B, 0x41, 0x53
    };
} // namespace crsce::decompress::known_lh
