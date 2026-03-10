/**
 * @file DecompressHeaderInvalid.h
 * @brief Thrown when a CRSCE file header fails validation (magic, CRC, size mismatch).
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    /**
     * @name DecompressHeaderInvalid
     * @brief Header validation failure (bad magic, CRC-32, or buffer too small).
     */
    class DecompressHeaderInvalid : public CrsceException {
    public:
        explicit DecompressHeaderInvalid(const std::string &what_arg) : CrsceException(what_arg) {}
        explicit DecompressHeaderInvalid(const char *what_arg) : CrsceException(what_arg) {}
    };
}
