/**
 * @file CompressOutputWriteError.h
 * @brief Thrown when the compressor encounters an error writing the output file.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    /**
     * @name CompressOutputWriteError
     * @brief Output file write failure during compression.
     */
    class CompressOutputWriteError : public CrsceException {
    public:
        explicit CompressOutputWriteError(const std::string &what_arg) : CrsceException(what_arg) {}
        explicit CompressOutputWriteError(const char *what_arg) : CrsceException(what_arg) {}
    };
}
