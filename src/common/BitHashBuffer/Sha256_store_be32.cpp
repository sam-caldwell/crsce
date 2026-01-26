/**
 * @file Sha256_store_be32.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Store a 32-bit word in big-endian order.
 */
#include "common/BitHashBuffer/detail/Sha256Types.h"    // u8
#include "common/BitHashBuffer/detail/Sha256Types32.h"  // u32
#include "common/BitHashBuffer/detail/sha256/store_be32.h"
#include <array>
#include <cstring>

namespace crsce::common::detail::sha256 {
    /**
     * @name store_be32
     * @brief Store a 32-bit value in big-endian byte order.
     * @param dst Destination buffer (must have space for 4 bytes).
     * @param x   32-bit value to store.
     */
    void store_be32(u8 *dst, u32 x) {
        std::array<u8, 4> tmp{};
        tmp[0] = static_cast<u8>((x >> 24) & 0xffU);
        tmp[1] = static_cast<u8>((x >> 16) & 0xffU);
        tmp[2] = static_cast<u8>((x >> 8) & 0xffU);
        tmp[3] = static_cast<u8>(x & 0xffU);
        std::memcpy(dst, tmp.data(), tmp.size());
    }
} // namespace crsce::common::detail::sha256
