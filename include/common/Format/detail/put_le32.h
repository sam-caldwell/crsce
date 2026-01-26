/**
 * @file put_le32.h
 * @brief Declaration for little-endian 32-bit store helper.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#pragma once

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
    void put_le32(std::array<std::uint8_t, 28> &b, std::size_t off, std::uint32_t v);
}
