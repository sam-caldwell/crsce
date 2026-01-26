/**
 * @file kRowZerosDigest.h
 * @brief Precomputed LH digest for the all-zeros row.
 * © Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <cstdint>

namespace crsce::decompress::known_lh {
    /**
     * @name kRowZerosDigest
     * @brief LH digest for a row of all zeros.
     */
    inline constexpr std::array<std::uint8_t, 32> kRowZerosDigest{
        0x84, 0x8A, 0x48, 0xBA, 0x81, 0xDA, 0x7E, 0x14, 0x69, 0x93, 0x97, 0xC9, 0x1C, 0xB1, 0x10, 0x1D,
        0x7C, 0xFE, 0xDA, 0xE4, 0x31, 0x85, 0x03, 0x62, 0xC0, 0xBF, 0x60, 0xBB, 0x3C, 0xE2, 0xC9, 0x2D
    };
} // namespace crsce::decompress::known_lh
