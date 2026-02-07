/**
 * @file nibble_to_hex.h
 * @brief Convert 4-bit value to uppercase hexadecimal character.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

namespace crsce::common::util {
    /**
     * @name nibble_to_hex
     * @brief Convert a 4-bit value (0..15) to its uppercase hexadecimal character.
     * @param v Unsigned integer; only the range 0..15 maps to a hex digit.
     * @return '0'..'9' or 'A'..'F' for inputs 0..15; otherwise '?' for invalid inputs.
     */
    char nibble_to_hex(unsigned v);
}
