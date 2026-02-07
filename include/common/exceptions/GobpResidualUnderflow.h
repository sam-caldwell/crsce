/**
 * @file GobpResidualUnderflow.h
 * @brief Exception for residual underflow or invalid assignments in GOBP.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "common/exceptions/CrsceException.h"

namespace crsce::decompress {
    /**
     * @name GobpResidualUnderflow
     * @brief Thrown when U is already zero for a line or R would underflow on assigning 1.
     */
    class GobpResidualUnderflow : public crsce::common::exceptions::CrsceException {
    public:
        enum class Kind : std::uint8_t { U_zero_on_assign, R_underflow_on_one };

        GobpResidualUnderflow(Kind kind, std::size_t r, std::size_t c, std::size_t d, std::size_t x)
            : CrsceException(make_message(kind, r, c, d, x)), kind_(kind), r_(r), c_(c), d_(d), x_(x) {}

        [[nodiscard]] Kind kind() const noexcept { return kind_; }
        [[nodiscard]] std::size_t row() const noexcept { return r_; }
        [[nodiscard]] std::size_t col() const noexcept { return c_; }
        [[nodiscard]] std::size_t diag() const noexcept { return d_; }
        [[nodiscard]] std::size_t xdiag() const noexcept { return x_; }

    private:
        static std::string make_message(Kind kind, std::size_t r, std::size_t c, std::size_t d, std::size_t x) {
            const char *k = (kind == Kind::U_zero_on_assign) ? "U already zero on a line while assigning"
                                                             : "R underflow on a line while assigning 1";
            return std::string("GOBP: ") + k + ": r=" + std::to_string(r) + ", c=" + std::to_string(c)
                   + ", d=" + std::to_string(d) + ", x=" + std::to_string(x);
        }

        Kind kind_;
        std::size_t r_;
        std::size_t c_;
        std::size_t d_;
        std::size_t x_;
    };
}
