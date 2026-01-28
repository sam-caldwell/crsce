/**
 * @file put_le64.cpp
 * @brief Definition for little-endian 64-bit store helper.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "common/Format/detail/put_le64.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace crsce::common::format::detail {
    /**
     * @name put_le64
     * @brief Store a 64-bit value in little-endian order into a buffer.
     * @param b Destination byte buffer sized for HeaderV1.
     * @param off Byte offset within buffer to write.
     * @param v  64-bit value to store (unsigned).
     * @return void No return value.
     */
    void put_le64(std::array<std::uint8_t, 28> &b, const std::size_t off, const std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            b.at(off + i) = static_cast<std::uint8_t>((v >> (8U * i)) & 0xFFU);
        }
    }
}
