/**
 * @file CompressInputReadError.h
 * @brief Thrown when the compressor fails to read the input file.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    /**
     * @name CompressInputReadError
     * @brief Input file read failure during compression.
     */
    class CompressInputReadError : public CrsceException {
    public:
        explicit CompressInputReadError(const std::string &what_arg) : CrsceException(what_arg) {}
        explicit CompressInputReadError(const char *what_arg) : CrsceException(what_arg) {}
    };
}
