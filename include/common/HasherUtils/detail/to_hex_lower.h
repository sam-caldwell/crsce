/**
 * @file to_hex_lower.h
 * @brief Convert a 32-byte digest to 64-char lowercase hex.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace crsce::common::hasher {
    /**
     * @name to_hex_lower
     * @brief Convert 32-byte digest to 64-character lowercase hexadecimal string.
     * @param digest Input buffer holding 32 bytes (SHA-256 digest).
     * @return Lowercase hex string of length 64.
     */
    std::string to_hex_lower(const std::array<std::uint8_t, 32> &digest);
}
