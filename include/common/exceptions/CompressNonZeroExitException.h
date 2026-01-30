/**
 * @file CompressNonZeroExitException.h
 * @brief Thrown when the compress tool exits with a non-zero status.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    /**
     * @name CompressNonZeroExitException
     * @brief Non-zero exit from compress subprocess.
     */
    class CompressNonZeroExitException : public CrsceException {
    public:
        explicit CompressNonZeroExitException(const std::string &what_arg) : CrsceException(what_arg) {}
        explicit CompressNonZeroExitException(const char *what_arg) : CrsceException(what_arg) {}
    };
}
