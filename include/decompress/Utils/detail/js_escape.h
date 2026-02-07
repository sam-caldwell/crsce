/**
 * @file js_escape.h
 * @brief Minimal JSON string escaper used for o11y outputs.
 * @author Sam Caldwell
 */
#pragma once

#include <array>
#include <string>

namespace crsce::decompress::detail {

inline std::string js_escape(const std::string &s) {
    std::string out; out.reserve(s.size() + 8);
    for (const unsigned char ch : s) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (ch < 0x20) {
                    static constexpr std::array<char,16> hex = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
                    out += "\\u00";
                    out.push_back(hex.at((ch >> 4) & 0xF));
                    out.push_back(hex.at(ch & 0xF));
                } else {
                    out.push_back(static_cast<char>(ch));
                }
        }
    }
    return out;
}

} // namespace crsce::decompress::detail

