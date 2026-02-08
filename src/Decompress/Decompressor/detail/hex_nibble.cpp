/**
* @file hex_nibble.cpp
 * @brief Convert a 4-bit nibble to a lowercase hex character.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include "decompress/Decompressor/detail/hex_nibble.cpp"

namespace crsce::decompress::detail {
    /**
     * @name hex_nibble
     * @brief Convert a 0..15 value to a lowercase hex char.
     * @param v Value to convert.
     * @return char Lowercase hex character.
     */
    inline char hex_nibble(unsigned v) {
        return static_cast<char>((v < 10U) ? ('0' + v) : ('a' + (v - 10U)));
    }
}

