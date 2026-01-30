/**
 * @file DecompressNonZeroExitException.h
 * @brief Thrown when the decompress tool exits with a non-zero status.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    /**
     * @name DecompressNonZeroExitException
     * @brief Non-zero exit from decompress subprocess.
     */
    class DecompressNonZeroExitException : public CrsceException {
    public:
        explicit DecompressNonZeroExitException(const std::string &what_arg) : CrsceException(what_arg) {}
        explicit DecompressNonZeroExitException(const char *what_arg) : CrsceException(what_arg) {}
    };
}
