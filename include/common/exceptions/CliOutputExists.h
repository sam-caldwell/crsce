/**
 * @file CliOutputExists.h
 * @brief Exception for output path already exists validation failures.
 */
#pragma once

#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    class CliOutputExists : public CrsceException {
    public:
        explicit CliOutputExists(const std::string &msg) : CrsceException(msg) {}
    };
}

