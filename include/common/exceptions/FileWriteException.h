/**
 * @file FileWriteException.h
 * @brief Thrown when writing to a file fails.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    /**
     * @name FileWriteException
     * @brief Failure to write an output file.
     */
    class FileWriteException : public CrsceException {
    public:
        explicit FileWriteException(const std::string &what_arg) : CrsceException(what_arg) {}
        explicit FileWriteException(const char *what_arg) : CrsceException(what_arg) {}
    };
}
