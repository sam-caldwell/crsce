/**
 * @file Sha256_store_be32.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Store a 32-bit word in big-endian order.
 */
#include "common/BitHashBuffer/detail/Sha256.h"
#include <array>
#include <cstring>

namespace crsce::common::detail::sha256 {

/** @name store_be32 */
void store_be32(u8* dst, u32 x) {
  std::array<u8, 4> tmp{};
  tmp[0] = static_cast<u8>((x >> 24) & 0xffU);
  tmp[1] = static_cast<u8>((x >> 16) & 0xffU);
  tmp[2] = static_cast<u8>((x >> 8) & 0xffU);
  tmp[3] = static_cast<u8>(x & 0xffU);
  std::memcpy(dst, tmp.data(), tmp.size());
}

} // namespace crsce::common::detail::sha256
