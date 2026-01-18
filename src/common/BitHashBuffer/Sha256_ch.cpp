/**
 * @file Sha256_ch.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief SHA-256 choose function.
 */
#include "common/BitHashBuffer/detail/Sha256.h"

namespace crsce::common::detail::sha256 {

/**
 * @name ch
 * @brief SHA-256 choose function.
 * @return (x & y) ^ (~x & z)
 */
u32 ch(const u32 x, const u32 y, const u32 z) { return (x & y) ^ (~x & z); }

} // namespace crsce::common::detail::sha256

