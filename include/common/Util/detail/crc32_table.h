/**
 * @file crc32_table.h
 * @brief Declaration for accessing the reflected CRC-32 lookup table.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt
 */
#pragma once

#include <array>
#include <cstdint>

namespace crsce::common::util::detail {
    /**
     * @name crc32_table
     * @brief Access the 256-entry reflected CRC-32 table (initialized on first use).
     * @return const std::array<std::uint32_t, 256>& Process-lifetime lookup table.
     */
    const std::array<std::uint32_t, 256> &crc32_table();
}
