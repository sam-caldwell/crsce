/**
 * @file crc32_table.h
 * @brief Helpers for building and accessing the reflected CRC-32 lookup table.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt
 */
#pragma once

#include <array>
#include <cstdint>

namespace crsce::common::util::detail {
    /**
     * @name kPoly
     * @brief Reflected CRC-32 polynomial (0xEDB88320).
     */
    inline constexpr std::uint32_t kPoly = 0xEDB88320U;

    /**
     * @name make_entry_rt
     * @brief Compute one reflected CRC-32 table entry for byte value i.
     * @param i Byte value [0..255].
     * @return std::uint32_t Reflected table entry for i.
     */
    inline std::uint32_t make_entry_rt(std::uint32_t i) {
        std::uint32_t c = i;
        for (int j = 0; j < 8; ++j) {
            if (c & 1U) {
                c = kPoly ^ (c >> 1U);
            } else {
                c >>= 1U;
            }
        }
        return c;
    }

    /**
     * @name crc32_table
     * @brief Lazily construct and return the 256-entry reflected CRC-32 table.
     * @return const std::array<std::uint32_t, 256>& Process-lifetime lookup table.
     */
    inline const std::array<std::uint32_t, 256> &crc32_table() {
        static const std::array<std::uint32_t, 256> t = [] {
            std::array<std::uint32_t, 256> tmp{};
            for (std::uint32_t i = 0; i < 256U; ++i) {
                tmp.at(i) = make_entry_rt(i);
            }
            return tmp;
        }();
        return t;
    }
} // namespace crsce::common::util::detail

