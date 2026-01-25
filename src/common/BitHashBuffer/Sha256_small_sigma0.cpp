/**
 * @file Sha256_small_sigma0.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief SHA-256 σ0 function.
 */
#include "common/BitHashBuffer/detail/Sha256.h"

namespace crsce::common::detail::sha256 {
    /**
     * @brief SHA-256 σ0 function.
     * @param x Input 32-bit value.
     * @return rotr(x,7) ^ rotr(x,18) ^ (x >> 3)
     */
    u32 small_sigma0(const u32 x) { return rotr(x, 7U) ^ rotr(x, 18U) ^ (x >> 3U); }
} // namespace crsce::common::detail::sha256
