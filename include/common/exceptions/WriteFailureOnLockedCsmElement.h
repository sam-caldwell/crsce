/**
 * @file WriteFailureOnLockedCsmElement.h
 * @author Sam Caldwell
 * @brief Exception thrown when writing to a locked CSM element.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <string>

#include "common/exceptions/CrsceException.h"

namespace crsce::decompress {
    /**
     * @name WriteFailureOnLockedCsmElement
     * @brief Exception thrown when attempting to write to a locked CSM element.
     */
    class WriteFailureOnLockedCsmElement : public crsce::common::exceptions::CrsceException {
    public:
        explicit WriteFailureOnLockedCsmElement(const std::string &what_arg)
            : CrsceException(what_arg) {}
        explicit WriteFailureOnLockedCsmElement(const char *what_arg)
            : CrsceException(what_arg) {}
    };
}
