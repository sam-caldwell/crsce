/**
 * @file PossibleCollisionException.h
 * @brief Thrown when decompressed output hash does not match input hash.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    /**
     * @name PossibleCollisionException
     * @brief Input and output content hashes mismatch.
     */
    class PossibleCollisionException : public CrsceException {
    public:
        explicit PossibleCollisionException(const std::string &what_arg) : CrsceException(what_arg) {}
        explicit PossibleCollisionException(const char *what_arg) : CrsceException(what_arg) {}
    };
}
