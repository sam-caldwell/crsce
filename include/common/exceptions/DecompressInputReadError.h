/**
 * @file DecompressInputReadError.h
 * @brief Thrown when the decompressor fails to read the input file.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    /**
     * @name DecompressInputReadError
     * @brief Input file read failure during decompression.
     */
    class DecompressInputReadError : public CrsceException {
    public:
        explicit DecompressInputReadError(const std::string &what_arg) : CrsceException(what_arg) {}
        explicit DecompressInputReadError(const char *what_arg) : CrsceException(what_arg) {}
    };
}
