/**
 * @file DeterministicEliminationError.h
 * @brief Exception for deterministic elimination constraint violations.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <string>

#include "common/exceptions/CrsceException.h"

namespace crsce::decompress {
    /**
     * @name DeterministicEliminationError
     * @brief Thrown when deterministic elimination detects an invariant violation or contradiction.
     */
    class DeterministicEliminationError : public crsce::common::exceptions::CrsceException {
    public:
        explicit DeterministicEliminationError(const std::string &what_arg)
            : CrsceException(what_arg) {}
        explicit DeterministicEliminationError(const char *what_arg)
            : CrsceException(what_arg) {}
    };
}
