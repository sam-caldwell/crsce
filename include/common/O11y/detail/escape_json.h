/**
 * @file escape_json.h
 * @brief Escape a string for safe JSON emission.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include "common/O11y/detail/hex_nibble.h"

namespace crsce::o11y::detail {
    /**
     * @name escape_json
     * @brief Escape a string for JSON output.
     * @param s Input string.
     * @return std::string Escaped string safe for JSON values.
     */
    inline std::string escape_json(const std::string &s) {
        std::string out; out.reserve(s.size() + 8U);
        for (const char ch : s) {
            switch (ch) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(ch) < 0x20U) {
                        const auto uc = static_cast<unsigned char>(ch);
                        out += "\\u00";
                        out += hex_nibble((uc >> 4U) & 0x0FU);
                        out += hex_nibble(uc & 0x0FU);
                    } else {
                        out += ch;
                    }
            }
        }
        return out;
    }
}

