/**
 * @file crc16_ccitt.h
 * @brief CRC-16-CCITT utility — constexpr-capable.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 *
 * Reflected CRC-16-CCITT: polynomial 0x1021 (reflected 0x8408),
 * init 0xFFFF, final XOR 0xFFFF. Same algorithmic structure as crc32_ieee.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace crsce::common::util {

    namespace detail {

        /**
         * @name kPoly16
         * @brief Reflected CRC-16-CCITT polynomial constant 0x8408.
         */
        inline constexpr std::uint16_t kPoly16 = 0x8408U;

        /**
         * @name make_crc16_entry
         * @brief Compute one reflected CRC-16 table entry.
         * @param i Byte value [0..255].
         * @return Reflected table entry.
         */
        constexpr std::uint16_t make_crc16_entry(const std::uint16_t i) {
            std::uint16_t c = i;
            for (int j = 0; j < 8; ++j) {
                if (c & 1U) {
                    c = kPoly16 ^ (c >> 1U);
                } else {
                    c >>= 1U;
                }
            }
            return c;
        }

        /**
         * @name make_crc16_table
         * @brief Build the 256-entry reflected CRC-16 table at compile time.
         * @return constexpr lookup table.
         */
        constexpr std::array<std::uint16_t, 256> make_crc16_table() {
            std::array<std::uint16_t, 256> t{};
            for (std::uint32_t i = 0; i < 256U; ++i) {
                t[i] = make_crc16_entry(static_cast<std::uint16_t>(i)); // NOLINT
            }
            return t;
        }

        /**
         * @name kCrc16Table
         * @brief Compile-time 256-entry reflected CRC-16 lookup table.
         */
        inline constexpr std::array<std::uint16_t, 256> kCrc16Table = make_crc16_table();

    } // namespace detail

    /**
     * @name crc16_ccitt
     * @brief Compute CRC-16-CCITT over a byte buffer (constexpr).
     * @param data Pointer to byte buffer.
     * @param len Number of bytes.
     * @param seed Initial seed (default 0xFFFF).
     * @return Final CRC-16 after bitwise negation.
     */
    constexpr std::uint16_t crc16_ccitt(const std::uint8_t *data, std::size_t len,
                                         std::uint16_t seed = 0xFFFFU) noexcept {
        std::uint16_t c = seed;
        for (std::size_t i = 0; i < len; ++i) {
            const auto idx = static_cast<std::size_t>((c ^ static_cast<std::uint16_t>(data[i])) & 0xFFU); // NOLINT
            c = detail::kCrc16Table[idx] ^ (c >> 8U); // NOLINT
        }
        return static_cast<std::uint16_t>(~c);
    }

} // namespace crsce::common::util
