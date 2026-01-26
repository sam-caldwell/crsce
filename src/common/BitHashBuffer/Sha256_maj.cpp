/**
 * @file Sha256_maj.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief SHA-256 majority function.
 */
#include "common/BitHashBuffer/detail/Sha256Types32.h" // u32
#include "common/BitHashBuffer/detail/sha256/maj.h"

namespace crsce::common::detail::sha256 {
    /**
     * @name maj
     * @brief Compute the bitwise majority of three 32-bit words.
     * @param x First 32-bit word operand.
     * @param y Second 32-bit word operand.
     * @param z Third 32-bit word operand.
     * @return u32 Bitwise majority of the corresponding bits of x, y, and z.
     */
    u32 maj(const u32 x, const u32 y, const u32 z) {
        return (x & y) ^ (x & z) ^ (y & z);
    }
} // namespace crsce::common::detail::sha256
