/**
 * @file to_hex.h
 * @brief Small helpers to convert byte spans to lowercase hex strings.
 */
#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace crsce::decompress::detail {
    inline char hex_nibble(unsigned v) {
        return static_cast<char>((v < 10U) ? ('0' + v) : ('a' + (v - 10U)));
    }

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

