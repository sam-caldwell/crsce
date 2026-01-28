/**
 * @file CrsceException.h
 * @brief Base exception type for CRSCE that enables targeted catching.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <stdexcept>
#include <string>

namespace crsce::common::exceptions {
    /**
     * @name CrsceException
     * @brief Base type for all custom CRSCE exceptions.
     */
    class CrsceException : public std::runtime_error {
    public:
        explicit CrsceException(const std::string &what_arg) : std::runtime_error(what_arg) {}
        explicit CrsceException(const char *what_arg) : std::runtime_error(what_arg) {}
        // Rule of zero: rely on std::runtime_error's special members.
    };
}
