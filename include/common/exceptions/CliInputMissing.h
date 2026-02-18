/**
 * @file CliInputMissing.h
 * @brief Exception for missing input file validation failures.
 */
#pragma once

#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    class CliInputMissing : public CrsceException {
    public:
        explicit CliInputMissing(const std::string &msg) : CrsceException(msg) {}
    };
}

