/**
 * @file CompressInputOpenError.h
 * @brief Thrown when the compressor cannot open the input file for reading.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    /**
     * @name CompressInputOpenError
     * @brief Input file cannot be opened for reading during compression.
     */
    class CompressInputOpenError : public CrsceException {
    public:
        explicit CompressInputOpenError(const std::string &what_arg) : CrsceException(what_arg) {}
        explicit CompressInputOpenError(const char *what_arg) : CrsceException(what_arg) {}
    };
}
