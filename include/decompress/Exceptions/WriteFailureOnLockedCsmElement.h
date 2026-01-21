/**
 * @file WriteFailureOnLockedCsmElement.h
 * @brief Exception thrown when writing to a locked CSM element.
 */
#pragma once

#include <stdexcept>
#include <string>

namespace crsce::decompress {

    class WriteFailureOnLockedCsmElement : public std::runtime_error {
    public:
        explicit WriteFailureOnLockedCsmElement(const std::string &what_arg)
            : std::runtime_error(what_arg) {}
        explicit WriteFailureOnLockedCsmElement(const char *what_arg)
            : std::runtime_error(what_arg) {}
    };

} // namespace crsce::decompress
