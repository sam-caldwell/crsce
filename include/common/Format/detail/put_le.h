/**
 * @file put_le.h
 * @brief Little-endian store helpers for HeaderV1 packing.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#pragma once

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
    inline void put_le16(std::array<std::uint8_t, 28> &b, std::size_t off, std::uint16_t v) {
        b.at(off + 0) = static_cast<std::uint8_t>(v & 0xFFU);
        b.at(off + 1) = static_cast<std::uint8_t>((v >> 8U) & 0xFFU);
    }

    /**
     * @name put_le32
     * @brief Store a 32-bit value in little-endian order into a buffer.
     * @param b Destination byte buffer sized for HeaderV1.
     * @param off Byte offset within buffer to write.
     * @param v  32-bit value to store (unsigned).
     * @return void No return value.
     */
    inline void put_le32(std::array<std::uint8_t, 28> &b, std::size_t off, std::uint32_t v) {
        b.at(off + 0) = static_cast<std::uint8_t>(v & 0xFFU);
        b.at(off + 1) = static_cast<std::uint8_t>((v >> 8U) & 0xFFU);
        b.at(off + 2) = static_cast<std::uint8_t>((v >> 16U) & 0xFFU);
        b.at(off + 3) = static_cast<std::uint8_t>((v >> 24U) & 0xFFU);
    }

    /**
     * @name put_le64
     * @brief Store a 64-bit value in little-endian order into a buffer.
     * @param b Destination byte buffer sized for HeaderV1.
     * @param off Byte offset within buffer to write.
     * @param v  64-bit value to store (unsigned).
     * @return void No return value.
     */
    inline void put_le64(std::array<std::uint8_t, 28> &b, std::size_t off, std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            b.at(off + i) = static_cast<std::uint8_t>((v >> (8U * i)) & 0xFFU);
        }
    }
} // namespace crsce::common::format::detail

