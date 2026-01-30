/**
 * @file json_escape.cpp
 * @brief Minimal JSON string escaper.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testrunner/detail/json_escape.h"
#include <iomanip>
#include <ios>
#include <sstream>
#include <string>

namespace crsce::testrunner::detail {
    /**
     * @name json_escape
     * @brief Escape a string for safe JSON string inclusion.
     * @param s Input string.
     * @return Escaped string.
     */
    std::string json_escape(const std::string &s) {
        std::string out;
        out.reserve(s.size());
        for (const unsigned char c: s) {
            if (c < 0x20) {
                std::ostringstream oss;
                oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                out += oss.str();
                continue;
            }
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out.push_back(static_cast<char>(c)); break;
            }
        }
        return out;
    }
}
