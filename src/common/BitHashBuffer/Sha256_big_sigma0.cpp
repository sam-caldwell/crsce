/**
 * @file Sha256_big_sigma0.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief SHA-256 Σ0 function.
 */
#include "common/BitHashBuffer/detail/Sha256.h"

namespace crsce::common::detail::sha256 {

/** @name big_sigma0 */
u32 big_sigma0(const u32 x) {
  return rotr(x, 2U) ^ rotr(x, 13U) ^ rotr(x, 22U);
}

} // namespace crsce::common::detail::sha256
