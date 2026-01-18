/**
 * @file NibbleToHex.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "common/Util/NibbleToHex.h"

namespace crsce::common::util {
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
