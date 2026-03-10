/**
 * @file CompressedPayload_unpackBits.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::unpackBits() implementation.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <cstddef>
#include <cstdint>

namespace crsce::common::format {

    /**
     * @name unpackBits
     * @brief Read n bits from data starting at the given bit offset (MSB-first).
     * @details Bits are read MSB-first within each byte (bit position 0 is 0x80)
     *          and assembled into a value with the first-read bit as the MSB.
     * @param data Input byte buffer.
     * @param bitOffset Current bit offset into data; advanced by n on return.
     * @param n Number of bits to read (1..16).
     * @return The extracted value.
     */
    std::uint16_t CompressedPayload::unpackBits(const std::uint8_t *data, std::size_t &bitOffset,
                                                const std::uint8_t n) {
        std::uint16_t value = 0;
        for (std::uint8_t i = 0; i < n; ++i) {
            const auto byteIdx = bitOffset / 8;
            const auto bitIdx = 7 - static_cast<std::uint8_t>(bitOffset % 8); // MSB-first within byte
            const auto bit = static_cast<std::uint16_t>((data[byteIdx] >> bitIdx) & 1U); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            value = static_cast<std::uint16_t>(value | static_cast<std::uint16_t>(bit << (n - 1 - i)));
            ++bitOffset;
        }
        return value;
    }

} // namespace crsce::common::format
