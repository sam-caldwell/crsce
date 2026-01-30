/**
 * @file InputOpenException.h
 * @brief Thrown when an input file cannot be opened for reading or writing.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    /**
     * @name InputOpenException
     * @brief Failure to open an input file.
     */
    class InputOpenException : public CrsceException {
    public:
        explicit InputOpenException(const std::string &what_arg) : CrsceException(what_arg) {}
        explicit InputOpenException(const char *what_arg) : CrsceException(what_arg) {}
    };
}
