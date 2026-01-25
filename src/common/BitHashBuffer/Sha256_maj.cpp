/**
 * @file Sha256_maj.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief SHA-256 majority function.
 */
#include "common/BitHashBuffer/detail/Sha256.h"

namespace crsce::common::detail::sha256 {
    /**
     * @name maj
     * @brief Majority bit per position among x, y, z.
     */
    u32 maj(const u32 x, const u32 y, const u32 z) {
        return (x & y) ^ (x & z) ^ (y & z);
    }
} // namespace crsce::common::detail::sha256
