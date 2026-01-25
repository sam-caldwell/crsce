/**
 * @file NibbleToHex.h
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

namespace crsce::common::util {
    /**
     * @name nibble_to_hex
     * @brief Convert a 4-bit value (0..15) to its uppercase hexadecimal character.
     * @usage char c = nibble_to_hex(10); // 'A'
     * @throws None
     * @param v Unsigned integer; only the range 0..15 maps to a hex digit.
     * @return '0', ..., '9' or 'A', ..., 'F' for inputs 0, ..., 15; otherwise '?' for invalid inputs.
     */
    char nibble_to_hex(unsigned v);

    /**
     * @name NibbleToHexTag
     * @brief Tag type to satisfy one-definition-per-header for nibble-to-hex utility.
     */
    struct NibbleToHexTag {};
} // namespace crsce::common::util
