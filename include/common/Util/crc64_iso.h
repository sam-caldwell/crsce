/**
 * @file crc64_iso.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Constexpr CRC-64-ISO (polynomial 0xD800819323B808A9, reflected).
 *
 * Second CRC-64 polynomial for generating linearly independent GF(2) equations.
 * Used together with CRC-64-ECMA to produce CRC-128 (128 independent equations).
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace crsce::common::util {

    namespace detail {

        inline constexpr std::uint64_t kPoly64Iso = 0xD800819323B808A9ULL;

        constexpr std::uint64_t make_crc64_iso_entry(std::uint64_t idx) {
            std::uint64_t c = idx;
            for (int k = 0; k < 8; ++k) {
                if (c & 1U) { c = (c >> 1) ^ kPoly64Iso; }
                else { c >>= 1; }
            }
            return c;
        }

        constexpr std::array<std::uint64_t, 256> make_crc64_iso_table() {
            std::array<std::uint64_t, 256> t{};
            for (std::uint64_t i = 0; i < 256; ++i) { t[i] = make_crc64_iso_entry(i); }
            return t;
        }

        inline constexpr std::array<std::uint64_t, 256> kCrc64IsoTable = make_crc64_iso_table();

    } // namespace detail

    constexpr std::uint64_t crc64_iso(const std::uint8_t *data, const std::size_t len) {
        std::uint64_t c = 0xFFFFFFFFFFFFFFFFULL;
        for (std::size_t i = 0; i < len; ++i) {
            const auto idx = static_cast<std::uint8_t>(c ^ data[i]);
            c = detail::kCrc64IsoTable[idx] ^ (c >> 8); // NOLINT
        }
        return ~c;
    }

} // namespace crsce::common::util
