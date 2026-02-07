/**
 * @file CsmIndexOutOfBounds.h
 * @brief Exception for CSM (r,c) index out-of-bounds errors.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstddef>
#include <string>

#include "common/exceptions/CrsceException.h"

namespace crsce::decompress {
    /**
     * @name CsmIndexOutOfBounds
     * @brief Thrown when a CSM API is called with r or c outside [0,kS).
     */
    class CsmIndexOutOfBounds : public crsce::common::exceptions::CrsceException {
    public:
        CsmIndexOutOfBounds(std::size_t r, std::size_t c, std::size_t limit)
            : CrsceException(make_message(r, c, limit)), r_(r), c_(c), limit_(limit) {}

        [[nodiscard]] std::size_t row() const noexcept { return r_; }
        [[nodiscard]] std::size_t col() const noexcept { return c_; }
        [[nodiscard]] std::size_t limit() const noexcept { return limit_; }

    private:
        static std::string make_message(std::size_t r, std::size_t c, std::size_t limit) {
            return std::string("CSM index out of bounds: r=") + std::to_string(r)
                   + ", c=" + std::to_string(c) + ", valid range=[0," + std::to_string(limit) + ")";
        }

        std::size_t r_;
        std::size_t c_;
        std::size_t limit_;
    };
}
