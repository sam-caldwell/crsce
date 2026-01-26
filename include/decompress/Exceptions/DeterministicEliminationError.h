/**
 * @file DeterministicEliminationError.h
 * @brief Exception for deterministic elimination constraint violations.
 * © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#pragma once

#include <stdexcept>
#include <string>

namespace crsce::decompress {
    /**
     * @name DeterministicEliminationError
     * @brief Thrown when deterministic elimination detects an invariant violation or contradiction.
     */
    class DeterministicEliminationError : public std::runtime_error {
    public:
        /**
         * @name DeterministicEliminationError
         * @brief Construct with a message string.
         * @param what_arg Description of the violation.
         */
        explicit DeterministicEliminationError(const std::string &what_arg)
            : std::runtime_error(what_arg) {}

        /**
         * @name DeterministicEliminationError
         * @brief Construct with a C-string message.
         * @param what_arg Description of the violation.
         */
        explicit DeterministicEliminationError(const char *what_arg)
            : std::runtime_error(what_arg) {}
    };
} // namespace crsce::decompress
