/**
 * @file CompressDINotFound.h
 * @brief Thrown when enumeration exhausts without finding the original CSM.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    /**
     * @name CompressDINotFound
     * @brief Enumeration exhausted without finding the original CSM.
     */
    class CompressDINotFound : public CrsceException {
    public:
        explicit CompressDINotFound(const std::string &what_arg) : CrsceException(what_arg) {}
        explicit CompressDINotFound(const char *what_arg) : CrsceException(what_arg) {}
    };
}
