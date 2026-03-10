/**
 * @file DecompressOutputWriteError.h
 * @brief Thrown when the decompressor encounters an error writing the output file.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    /**
     * @name DecompressOutputWriteError
     * @brief Output file write failure during decompression.
     */
    class DecompressOutputWriteError : public CrsceException {
    public:
        explicit DecompressOutputWriteError(const std::string &what_arg) : CrsceException(what_arg) {}
        explicit DecompressOutputWriteError(const char *what_arg) : CrsceException(what_arg) {}
    };
}
