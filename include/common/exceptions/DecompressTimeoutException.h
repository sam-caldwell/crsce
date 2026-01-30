/**
 * @file DecompressTimeoutException.h
 * @brief Thrown when decompression exceeds the allowed time per block.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    /**
     * @name DecompressTimeoutException
     * @brief Timeout while waiting for decompress subprocess.
     */
    class DecompressTimeoutException : public CrsceException {
    public:
        explicit DecompressTimeoutException(const std::string &what_arg) : CrsceException(what_arg) {}
        explicit DecompressTimeoutException(const char *what_arg) : CrsceException(what_arg) {}
    };
}
