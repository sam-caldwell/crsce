/**
 * @file CompressTimeoutException.h
 * @brief Thrown when compression exceeds the allowed time per block.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    /**
     * @name CompressTimeoutException
     * @brief Timeout while waiting for compress subprocess.
     */
    class CompressTimeoutException : public CrsceException {
    public:
        explicit CompressTimeoutException(const std::string &what_arg) : CrsceException(what_arg) {}
        explicit CompressTimeoutException(const char *what_arg) : CrsceException(what_arg) {}
    };
}
