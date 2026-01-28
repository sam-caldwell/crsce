/**
 * @file put_le32.cpp
 * @brief Definition for little-endian 32-bit store helper.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "common/Format/detail/put_le32.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace crsce::common::format::detail {
    /**
     * @name put_le32
     * @brief Store a 32-bit value in little-endian order into a buffer.
     * @param b Destination byte buffer sized for HeaderV1.
     * @param off Byte offset within buffer to write.
     * @param v  32-bit value to store (unsigned).
     * @return void No return value.
     */
    void put_le32(std::array<std::uint8_t, 28> &b, const std::size_t off, std::uint32_t v) {
        b.at(off + 0) = static_cast<std::uint8_t>(v & 0xFFU);
        b.at(off + 1) = static_cast<std::uint8_t>((v >> 8U) & 0xFFU);
        b.at(off + 2) = static_cast<std::uint8_t>((v >> 16U) & 0xFFU);
        b.at(off + 3) = static_cast<std::uint8_t>((v >> 24U) & 0xFFU);
    }
}
