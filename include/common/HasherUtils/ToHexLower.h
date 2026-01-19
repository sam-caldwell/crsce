/**
 * @file ToHexLower.h
 * @brief Convert a 32-byte digest to 64-char lowercase hex.
 */
#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace crsce::common::hasher {
    // Convert a 32-byte digest into a 64-character lowercase hex string.
    std::string to_hex_lower(const std::array<std::uint8_t, 32> &digest);
} // namespace crsce::common::hasher
