/**
 * @file DecompressDIOutOfRange.h
 * @brief Thrown when solver enumeration does not reach the DI-th solution.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    /**
     * @name DecompressDIOutOfRange
     * @brief Enumeration did not reach the DI-th solution.
     */
    class DecompressDIOutOfRange : public CrsceException {
    public:
        explicit DecompressDIOutOfRange(const std::string &what_arg) : CrsceException(what_arg) {}
        explicit DecompressDIOutOfRange(const char *what_arg) : CrsceException(what_arg) {}
    };
}
