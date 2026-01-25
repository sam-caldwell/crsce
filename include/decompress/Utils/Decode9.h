/**
 * @file Decode9.h
 * @brief Utilities to decode MSB-first 9-bit packed streams.
 * © Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace crsce::decompress {
    /**
     * @name decode_9bit_stream
     * @brief Decode a 9-bit MSB-first packed stream into kCount 16-bit integers.
     * @tparam kCount Number of values to decode.
     * @param bytes Input byte span containing 9*kCount bits (padded to whole bytes).
     * @return Fixed-size array of decoded values.
     */
    template<std::size_t kCount>
    inline std::array<std::uint16_t, kCount>
    decode_9bit_stream(std::span<const std::uint8_t> bytes) {
        std::array<std::uint16_t, kCount> vals{};
        for (std::size_t i = 0; i < kCount; ++i) {
            std::uint16_t v = 0;
            for (std::size_t b = 0; b < 9; ++b) {
                const std::size_t bit_index = (i * 9) + b;
                const std::size_t byte_index = bit_index / 8;
                const auto bit_in_byte = static_cast<int>(bit_index % 8);
                const std::uint8_t byte = bytes[byte_index];
                const auto bit = static_cast<std::uint16_t>((byte >> (7 - bit_in_byte)) & 0x1U);
                v = static_cast<std::uint16_t>((v << 1) | bit);
            }
            vals.at(i) = v;
        }
        return vals;
    }
} // namespace crsce::decompress
