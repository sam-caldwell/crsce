/**
 * @file CliParseError.h
 * @brief Exception signaling that CLI parsing failed (unknown flag or missing value).
 */
#pragma once

#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    class CliParseError : public CrsceException {
    public:
        explicit CliParseError(const std::string &usage)
            : CrsceException(usage) {}
    };
}

