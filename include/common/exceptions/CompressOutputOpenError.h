/**
 * @file CompressOutputOpenError.h
 * @brief Thrown when the compressor cannot open the output file for writing.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    /**
     * @name CompressOutputOpenError
     * @brief Output file cannot be opened for writing during compression.
     */
    class CompressOutputOpenError : public CrsceException {
    public:
        explicit CompressOutputOpenError(const std::string &what_arg) : CrsceException(what_arg) {}
        explicit CompressOutputOpenError(const char *what_arg) : CrsceException(what_arg) {}
    };
}
