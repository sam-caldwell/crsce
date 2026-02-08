/**
 * @file hex_nibble.h
 * @brief Convert a 4-bit nibble to a lowercase hex character.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

namespace crsce::decompress::detail {
    /**
     * @name hex_nibble
     * @brief Convert a 0..15 value to a lowercase hex char.
     * @param v Value to convert.
     * @return char Lowercase hex character.
     */
    inline char hex_nibble(unsigned v) {
        static constexpr std::array<char, 16> kDigits{
            '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'
        };
        return kDigits.at(v & 0x0F);
    }
}
