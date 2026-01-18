/**
 * @file Sha256_load_be32.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Load a 32-bit word from big-endian order.
 */
#include "common/BitHashBuffer/detail/Sha256.h"
#include <array>
#include <cstring>

namespace crsce::common::detail::sha256 {

/** @name load_be32 */
u32 load_be32(const u8* src) {
  std::array<u8, 4> tmp{};
  std::memcpy(tmp.data(), src, tmp.size());
  return (static_cast<u32>(tmp[0]) << 24) |
         (static_cast<u32>(tmp[1]) << 16) |
         (static_cast<u32>(tmp[2]) << 8) |
         (static_cast<u32>(tmp[3]));
}

} // namespace crsce::common::detail::sha256
