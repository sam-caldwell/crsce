/**
* @file to_hex.cpp
 * @brief Small helpers to convert byte spans to lowercase hex strings.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include "decompress/Decompressor/detail/to_hex.h"

namespace crsce::decompress::detail {
    /**
     * @name to_hex
     * @brief Convert a byte span to a lowercase hex string.
     * @param bytes Byte span to convert.
     * @return Lowercase hex string.
     */
    inline std::string to_hex(std::span<const std::uint8_t> bytes) {
        std::string s;
        s.reserve(bytes.size() * 2U);
        for (auto b8 : bytes) {
            const auto b = static_cast<unsigned>(b8);
            s.push_back(hex_nibble((b >> 4U) & 0xFU));
            s.push_back(hex_nibble(b & 0xFU));
        }
        return s;
    }
}
