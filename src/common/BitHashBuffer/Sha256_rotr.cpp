/**
 * @file Sha256_rotr.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief SHA-256 rotate-right helper.
 */
#include "common/BitHashBuffer/detail/Sha256Types32.h" // u32
#include "common/BitHashBuffer/detail/sha256/rotr.h"

namespace crsce::common::detail::sha256 {
    /**
     * @name rotr
     * @brief Rotate-right utility for 32-bit values.
     * @param x Input word.
     * @param n Rotation amount [0..31].
     * @return x rotated right by n bits.
     */
    u32 rotr(const u32 x, const u32 n) { return (x >> n) | (x << (32U - n)); }
} // namespace crsce::common::detail::sha256
