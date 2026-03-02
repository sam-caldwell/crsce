/**
 * @file DecompressInputOpenError.h
 * @brief Thrown when the decompressor cannot open the input file for reading.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    /**
     * @name DecompressInputOpenError
     * @brief Input file cannot be opened for reading during decompression.
     */
    class DecompressInputOpenError : public CrsceException {
    public:
        explicit DecompressInputOpenError(const std::string &what_arg) : CrsceException(what_arg) {}
        explicit DecompressInputOpenError(const char *what_arg) : CrsceException(what_arg) {}
    };
}
