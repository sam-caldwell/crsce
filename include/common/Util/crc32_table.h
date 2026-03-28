/**
 * @file crc32_table.h
 * @brief Compile-time reflected CRC-32 lookup table.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt
 */
#pragma once

#include <array>
#include <cstdint>

#include "common/Util/make_entry_rt.h"

namespace crsce::common::util::detail {

    /**
     * @name make_crc32_table
     * @brief Build the 256-entry reflected CRC-32 table at compile time.
     * @return constexpr std::array<std::uint32_t, 256> The lookup table.
     */
    constexpr std::array<std::uint32_t, 256> make_crc32_table() {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t i = 0; i < 256U; ++i) {
            t[i] = make_entry_rt(i); // NOLINT
        }
        return t;
    }

    /**
     * @name kCrc32Table
     * @brief Compile-time 256-entry reflected CRC-32 lookup table.
     */
    inline constexpr std::array<std::uint32_t, 256> kCrc32Table = make_crc32_table();

    /**
     * @name crc32_table
     * @brief Access the CRC-32 table (backward compatibility).
     * @return const std::array<std::uint32_t, 256>& Reference to the constexpr table.
     */
    inline const std::array<std::uint32_t, 256> &crc32_table() { return kCrc32Table; }

}
