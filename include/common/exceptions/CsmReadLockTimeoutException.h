/**
 * @file CsmReadLockTimeoutException.h
 * @brief Exception thrown when waiting on a CSM cell MU lock times out during read.
 */
#pragma once

#include <string>
#include "common/exceptions/CrsceException.h"

namespace crsce::decompress {
    class CsmReadLockTimeoutException : public crsce::common::exceptions::CrsceException {
    public:
        explicit CsmReadLockTimeoutException(const std::string &what_arg)
            : CrsceException(what_arg) {}
        explicit CsmReadLockTimeoutException(const char *what_arg)
            : CrsceException(what_arg) {}
    };
}

