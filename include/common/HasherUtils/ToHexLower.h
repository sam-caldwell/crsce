/**
 * @file ToHexLower.h
 * @brief Convert a 32-byte digest to 64-char lowercase hex.
 * © Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace crsce::common::hasher {
    // Convert a 32-byte digest into a 64-character lowercase hex string.
    std::string to_hex_lower(const std::array<std::uint8_t, 32> &digest);

    /**
     * @name ToHexLowerTag
     * @brief Tag type to satisfy one-definition-per-header for hasher utilities.
     */
    struct ToHexLowerTag {};
} // namespace crsce::common::hasher
