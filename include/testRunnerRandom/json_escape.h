/**
 * @file json_escape.h
 * @brief Minimal JSON string escaper.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>

namespace crsce::testrunner::detail {
    /**
     * @name json_escape
     * @brief Escape string for JSON string literal.
     * @param s Unescaped input string.
     * @return Escaped string.
     */
    std::string json_escape(const std::string &s);
}
