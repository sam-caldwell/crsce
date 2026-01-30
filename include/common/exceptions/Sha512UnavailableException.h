/**
 * @file Sha512UnavailableException.h
 * @brief Thrown when neither sha512sum nor shasum is available to compute SHA-512.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    /**
     * @name Sha512UnavailableException
     * @brief SHA-512 binary not found on system.
     */
    class Sha512UnavailableException : public CrsceException {
    public:
        explicit Sha512UnavailableException(const std::string &what_arg) : CrsceException(what_arg) {}
        explicit Sha512UnavailableException(const char *what_arg) : CrsceException(what_arg) {}
    };
}
