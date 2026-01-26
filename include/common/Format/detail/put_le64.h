/**
 * @file put_le64.h
 * @brief Declaration for little-endian 64-bit store helper.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#pragma once

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
    void put_le64(std::array<std::uint8_t, 28> &b, std::size_t off, std::uint64_t v);
}
