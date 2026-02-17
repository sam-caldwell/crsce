/**
 * @file CliHelpRequested.h
 * @brief Exception signaling that CLI help was requested.
 */
#pragma once

#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    class CliHelpRequested : public CrsceException {
    public:
        explicit CliHelpRequested(const std::string &usage)
            : CrsceException(usage) {}
    };
}

