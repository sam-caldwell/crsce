/**
 * @file crc64_ecma.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Constexpr CRC-64-ECMA-182 (polynomial 0xC96C5795D7870F42, reflected).
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace crsce::common::util {

    namespace detail {

        /**
         * @name kPoly64
         * @brief Reflected CRC-64-ECMA polynomial.
         */
        inline constexpr std::uint64_t kPoly64 = 0xC96C5795D7870F42ULL;

        /**
         * @name make_crc64_entry
         * @brief Compute one CRC-64 table entry.
         */
        constexpr std::uint64_t make_crc64_entry(std::uint64_t idx) {
            std::uint64_t c = idx;
            for (int k = 0; k < 8; ++k) {
                if (c & 1U) { c = (c >> 1) ^ kPoly64; }
                else { c >>= 1; }
            }
            return c;
        }

        /**
         * @name make_crc64_table
         * @brief Build the full 256-entry CRC-64 lookup table at compile time.
         */
        constexpr std::array<std::uint64_t, 256> make_crc64_table() {
            std::array<std::uint64_t, 256> t{};
            for (std::uint64_t i = 0; i < 256; ++i) { t[i] = make_crc64_entry(i); }
            return t;
        }

        /**
         * @name kCrc64Table
         * @brief Compile-time CRC-64 lookup table.
         */
        inline constexpr std::array<std::uint64_t, 256> kCrc64Table = make_crc64_table();

    } // namespace detail

    /**
     * @name crc64_ecma
     * @brief Compute CRC-64-ECMA of a byte buffer.
     * @param data Pointer to data.
     * @param len Length in bytes.
     * @return CRC-64 digest.
     */
    constexpr std::uint64_t crc64_ecma(const std::uint8_t *data, const std::size_t len) {
        std::uint64_t c = 0xFFFFFFFFFFFFFFFFULL;
        for (std::size_t i = 0; i < len; ++i) {
            const auto idx = static_cast<std::uint8_t>(c ^ data[i]);
            c = detail::kCrc64Table[idx] ^ (c >> 8); // NOLINT
        }
        return ~c;
    }

} // namespace crsce::common::util
