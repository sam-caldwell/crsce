/**
 * @file WriteFailureOnLockedCsmElement.h
 * @brief Exception thrown when writing to a locked CSM element.
 * © Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <stdexcept>
#include <string>

namespace crsce::decompress {
    /**
     * @name WriteFailureOnLockedCsmElement
     * @brief Exception thrown when attempting to write to a locked CSM element.
     */
    class WriteFailureOnLockedCsmElement : public std::runtime_error {
    public:
        /**
         * @name WriteFailureOnLockedCsmElement
         * @brief Construct with a message string.
         * @param what_arg Reason for the write failure.
         */
        explicit WriteFailureOnLockedCsmElement(const std::string &what_arg)
            : std::runtime_error(what_arg) {
        }

        /**
         * @name WriteFailureOnLockedCsmElement
         * @brief Construct with a C-string message.
         * @param what_arg Reason for the write failure.
         */
        explicit WriteFailureOnLockedCsmElement(const char *what_arg)
            : std::runtime_error(what_arg) {
        }
    };
} // namespace crsce::decompress
