/**
 * @file Sha256_small_sigma1.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief SHA-256 σ1 function.
 */
#include "common/BitHashBuffer/detail/Sha256.h"

namespace crsce::common::detail::sha256 {
    /**
     * @brief SHA-256 σ1 function.
     * @param x Input 32-bit value.
     * @return rotr(x,17) ^ rotr(x,19) ^ (x >> 10)
     */
    u32 small_sigma1(const u32 x) {
        return rotr(x, 17U) ^ rotr(x, 19U) ^ (x >> 10U);
    }
} // namespace crsce::common::detail::sha256
