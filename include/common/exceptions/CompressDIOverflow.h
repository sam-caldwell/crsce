/**
 * @file CompressDIOverflow.h
 * @brief Thrown when the disambiguation index exceeds 255 during compression.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include "common/exceptions/CrsceException.h"

namespace crsce::common::exceptions {
    /**
     * @name CompressDIOverflow
     * @brief DI exceeds 255; block is not compressible.
     */
    class CompressDIOverflow : public CrsceException {
    public:
        explicit CompressDIOverflow(const std::string &what_arg) : CrsceException(what_arg) {}
        explicit CompressDIOverflow(const char *what_arg) : CrsceException(what_arg) {}
    };
}
