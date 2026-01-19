/**
 * @file Sha256_small_sigma0.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief SHA-256 σ0 function.
 */
#include "common/BitHashBuffer/detail/Sha256.h"

namespace crsce::common::detail::sha256 {

/** @name small_sigma0 */
u32 small_sigma0(const u32 x) { return rotr(x, 7U) ^ rotr(x, 18U) ^ (x >> 3U); }

} // namespace crsce::common::detail::sha256
