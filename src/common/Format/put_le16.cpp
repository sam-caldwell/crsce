/**
 * @file put_le16.cpp
 * @brief Definition for little-endian 16-bit store helper.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "common/Format/detail/put_le16.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace crsce::common::format::detail {
    /**
     * @name put_le16
     * @brief Store a 16-bit value in little-endian order into a buffer.
     * @param b Destination byte buffer sized for HeaderV1.
     * @param off Byte offset within buffer to write.
     * @param v  16-bit value to store (unsigned).
     * @return void No return value.
     */
    void put_le16(std::array<std::uint8_t, 28> &b, std::size_t off, std::uint16_t v) {
        b.at(off + 0) = static_cast<std::uint8_t>(v & 0xFFU);
        b.at(off + 1) = static_cast<std::uint8_t>((v >> 8U) & 0xFFU);
    }
}
