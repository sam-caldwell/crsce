/**
 * @file Sha256_big_sigma1.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief SHA-256 Σ1 function.
 */
#include "common/BitHashBuffer/detail/Sha256Types32.h" // u32
#include "common/BitHashBuffer/detail/sha256/rotr.h"
#include "common/BitHashBuffer/detail/sha256/big_sigma1.h"

namespace crsce::common::detail::sha256 {
    /**
     * @brief SHA-256 Σ1 function.
     * @param x Input 32-bit value.
     * @return rotr(x,6) ^ rotr(x,11) ^ rotr(x,25)
     */
    u32 big_sigma1(const u32 x) {
        return rotr(x, 6U) ^ rotr(x, 11U) ^ rotr(x, 25U);
    }
} // namespace crsce::common::detail::sha256
