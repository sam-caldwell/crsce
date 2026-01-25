/**
 * @file Sha256_ch.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief SHA-256 choose function.
 */
#include "common/BitHashBuffer/detail/Sha256.h"

namespace crsce::common::detail::sha256 {
    /**
     * @name ch
     * @brief SHA-256 choose function. It selects bits from two different inputs (\(y\) or \(z\)) based on the value
     *        of a third input (\(x\))
     * @param x First operand.
     * @param y Second operand.
     * @param z Third operand.
     * @return (x & y) ^ (~x & z)
     */
    u32 ch(const u32 x, const u32 y, const u32 z) {
        return (x & y) ^ (~x & z);
    }
} // namespace crsce::common::detail::sha256
