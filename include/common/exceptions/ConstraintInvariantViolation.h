/**
 * @file ConstraintInvariantViolation.h
 * @brief Exception for violations of R ≤ U invariant in constraint lines.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstddef>
#include <string>

#include "common/exceptions/CrsceException.h"

namespace crsce::decompress {
    /**
     * @name ConstraintInvariantViolation
     * @brief Thrown when R > U for a given family/index.
     */
    class ConstraintInvariantViolation : public crsce::common::exceptions::CrsceException {
    public:
        ConstraintInvariantViolation(const char *family, std::size_t index, std::size_t R, std::size_t U)
            : CrsceException(make_message(family, index, R, U)), family_(family), index_(index), R_(R), U_(U) {}

        [[nodiscard]] const std::string &family() const noexcept { return family_; }
        [[nodiscard]] std::size_t index() const noexcept { return index_; }
        [[nodiscard]] std::size_t R() const noexcept { return R_; }
        [[nodiscard]] std::size_t U() const noexcept { return U_; }

    private:
        static std::string make_message(const char *family, std::size_t index, std::size_t R, std::size_t U) {
            return std::string("R > U invariant violated for ") + family + "[" + std::to_string(index) + "] : R="
                   + std::to_string(R) + ", U=" + std::to_string(U);
        }

        std::string family_;
        std::size_t index_;
        std::size_t R_;
        std::size_t U_;
    };
}
