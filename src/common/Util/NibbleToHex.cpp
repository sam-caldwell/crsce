/**
 * @file NibbleToHex.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "common/Util/NibbleToHex.h"

namespace crsce::common::util {
/**
 * @name nibble_to_hex
 * @brief Convert a 4-bit value (0..15) to its uppercase hexadecimal character.
 * @param v Unsigned value; only the low nibble is meaningful.
 * @return '0'..'9' or 'A'..'F' for inputs 0..15; '?' for invalid inputs.
 * @details Used by lightweight hex formatting routines where speed and locality matter.
 */
char nibble_to_hex(const unsigned v) {
  if (v < 10U) {
    return static_cast<char>('0' + v);
  }
  if (v < 16U) {
    return static_cast<char>('A' + (v - 10U));
  }
  return '?';
}
} // namespace crsce::common::util
