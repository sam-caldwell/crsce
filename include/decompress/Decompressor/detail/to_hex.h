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
    std::string to_hex(std::span<const std::uint8_t> bytes);
}
