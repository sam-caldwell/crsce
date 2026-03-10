/**
 * @file DecompressOutputOpenError.h
 * @brief Thrown when the decompressor cannot open the output file for writing.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    /**
     * @name DecompressOutputOpenError
     * @brief Output file cannot be opened for writing during decompression.
     */
    class DecompressOutputOpenError : public CrsceException {
    public:
        explicit DecompressOutputOpenError(const std::string &what_arg) : CrsceException(what_arg) {}
        explicit DecompressOutputOpenError(const char *what_arg) : CrsceException(what_arg) {}
    };
}
