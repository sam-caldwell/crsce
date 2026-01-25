/**
 * @file Crc32.h
 * @brief CRC-32 (IEEE 802.3) utility.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace crsce::common::util {
    /**
     * Compute CRC-32 (polynomial 0xEDB88320) over a byte buffer.
     * seed should be 0xFFFFFFFF for a fresh computation; result is ~crc.
     */
    std::uint32_t crc32_ieee(const std::uint8_t *data, std::size_t len,
                             std::uint32_t seed = 0xFFFFFFFFU) noexcept;

    /**
     * @name Crc32Tag
     * @brief Tag type to satisfy one-definition-per-header for CRC utilities.
     */
    struct Crc32Tag {};
} // namespace crsce::common::util
