/**
 * @file CliNoArgs.h
 * @brief Exception signaling that no CLI arguments were provided (informational, not an error).
 */
#pragma once

#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    class CliNoArgs : public CrsceException {
    public:
        explicit CliNoArgs(const std::string &msg)
            : CrsceException(msg) {}
    };
}

