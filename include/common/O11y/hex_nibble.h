/**
 * @file hex_nibble.h
 * @brief Convert a 4-bit value into a lowercase hexadecimal character.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

namespace crsce::o11y::detail {
    /**
     * @name hex_nibble
     * @brief Convert a 4-bit value to a lowercase hex digit.
     * @param v Unsigned 4-bit value (low nibble used).
     * @return char ASCII hex digit ('0'..'9','a'..'f').
     */
    inline char hex_nibble(unsigned char v) {
        return (v < 10U) ? static_cast<char>('0' + v) : static_cast<char>('a' + (v - 10U));
    }
}

