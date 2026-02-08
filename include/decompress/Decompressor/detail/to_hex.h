/**
 * @file to_hex.h
 * @author Sam Caldwell
 * @brief Small helpers to convert byte spans to lowercase hex strings.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include "decompress/Decompressor/detail/hex_nibble.h"

namespace crsce::decompress::detail {
    /**
     * @name to_hex
     * @brief Convert a byte span to a lowercase hex string.
     * @param bytes Byte span to convert.
     * @return Lowercase hex string.
     */
    inline std::string to_hex(std::span<const std::uint8_t> bytes) {
        std::string out;
        out.resize(bytes.size() * 2U);
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            const auto b = static_cast<unsigned>(bytes[i]);
            const std::size_t j = 2U * i;
            out[j]     = hex_nibble(b >> 4U);
            out[j + 1] = hex_nibble(b & 0x0F);
        }
        return out;
    }
}
