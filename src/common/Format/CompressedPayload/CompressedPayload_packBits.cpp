/**
 * @file CompressedPayload_packBits.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::packBits() implementation.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace crsce::common::format {

    /**
     * @name packBits
     * @brief Write n bits of value into buf starting at the given bit offset (MSB-first).
     * @details Bits are written from the most-significant bit of value downward.
     *          For example, packBits(buf, off, 0b101, 3) writes 1, 0, 1 into
     *          consecutive bit positions.  Each bit is placed MSB-first within its
     *          target byte: bit position 0 within a byte is the 0x80 position.
     * @param buf Output byte buffer (must be large enough).
     * @param bitOffset Current bit offset into buf; advanced by n on return.
     * @param value The value to write (only the lowest n bits are used).
     * @param n Number of bits to write (1..16).
     */
    void CompressedPayload::packBits(std::vector<std::uint8_t> &buf, std::size_t &bitOffset,
                                     const std::uint16_t value, const std::uint8_t n) {
        for (auto i = static_cast<std::int8_t>(n - 1); i >= 0; --i) {
            const auto byteIdx = bitOffset / 8;
            const auto bitIdx = 7 - static_cast<std::uint8_t>(bitOffset % 8); // MSB-first within byte
            const auto bit = static_cast<std::uint8_t>((value >> static_cast<std::uint8_t>(i)) & 1U);
            buf[byteIdx] = static_cast<std::uint8_t>(buf[byteIdx] | static_cast<std::uint8_t>(bit << bitIdx));
            ++bitOffset;
        }
    }

} // namespace crsce::common::format
