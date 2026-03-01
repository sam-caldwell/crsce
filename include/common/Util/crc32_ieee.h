/**
 * @file crc32_ieee.h
 * @brief CRC-32 (IEEE 802.3) utility.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace crsce::common::util {
    /**
     * @name crc32_ieee
     * @brief Compute CRC-32 (polynomial 0xEDB88320) over a byte buffer.
     * @param data Pointer to byte buffer.
     * @param len Number of bytes in the buffer.
     * @param seed Initial CRC seed (usually 0xFFFFFFFF for a fresh computation).
     * @return Final CRC value after bitwise negation (~crc).
     */
    std::uint32_t crc32_ieee(const std::uint8_t *data, std::size_t len,
                             std::uint32_t seed = 0xFFFFFFFFU) noexcept;
}
