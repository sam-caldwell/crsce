/**
 * @file crc8.h
 * @brief CRC-8 utility — constexpr-capable.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 *
 * CRC-8-CCITT: polynomial 0x07 (normal) / 0xE0 (reflected),
 * init 0xFF, final XOR 0xFF.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace crsce::common::util {

    namespace detail {

        /**
         * @name kPoly8
         * @brief Reflected CRC-8-CCITT polynomial constant 0xE0.
         */
        inline constexpr std::uint8_t kPoly8 = 0xE0U;

        /**
         * @name make_crc8_entry
         * @brief Compute one reflected CRC-8 table entry.
         * @param i Byte value [0..255].
         * @return Reflected table entry.
         */
        constexpr std::uint8_t make_crc8_entry(const std::uint8_t i) {
            std::uint8_t c = i;
            for (int j = 0; j < 8; ++j) {
                if (c & 1U) {
                    c = kPoly8 ^ (c >> 1U);
                } else {
                    c >>= 1U;
                }
            }
            return c;
        }

        /**
         * @name make_crc8_table
         * @brief Build the 256-entry reflected CRC-8 table at compile time.
         * @return constexpr lookup table.
         */
        constexpr std::array<std::uint8_t, 256> make_crc8_table() {
            std::array<std::uint8_t, 256> t{};
            for (std::uint32_t i = 0; i < 256U; ++i) {
                t[i] = make_crc8_entry(static_cast<std::uint8_t>(i)); // NOLINT
            }
            return t;
        }

        /**
         * @name kCrc8Table
         * @brief Compile-time 256-entry reflected CRC-8 lookup table.
         */
        inline constexpr std::array<std::uint8_t, 256> kCrc8Table = make_crc8_table();

    } // namespace detail

    /**
     * @name crc8
     * @brief Compute CRC-8 over a byte buffer (constexpr).
     * @param data Pointer to byte buffer.
     * @param len Number of bytes.
     * @param seed Initial seed (default 0xFF).
     * @return Final CRC-8 after bitwise negation.
     */
    constexpr std::uint8_t crc8(const std::uint8_t *data, std::size_t len,
                                 std::uint8_t seed = 0xFFU) noexcept {
        std::uint8_t c = seed;
        for (std::size_t i = 0; i < len; ++i) {
            const auto idx = static_cast<std::size_t>(c ^ data[i]); // NOLINT
            c = detail::kCrc8Table[idx]; // NOLINT
        }
        return static_cast<std::uint8_t>(~c);
    }

} // namespace crsce::common::util
