/**
 * @file ConstraintBoundsInvalid.h
 * @brief Exception for invalid constraint upper bounds (U out of [0,S]).
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstddef>
#include <string>

#include "common/exceptions/CrsceException.h"

namespace crsce::decompress {
    /**
     * @name ConstraintBoundsInvalid
     * @brief Thrown when a U_* value exceeds S (valid range [0,S]).
     */
    class ConstraintBoundsInvalid : public crsce::common::exceptions::CrsceException {
    public:
        ConstraintBoundsInvalid(const char *family, const std::size_t index, const std::size_t value, const std::size_t S)
            : CrsceException(make_message(family, index, value, S)), family_(family), index_(index), value_(value), S_(S) {}

        [[nodiscard]] const std::string &family() const noexcept { return family_; }
        [[nodiscard]] std::size_t index() const noexcept { return index_; }
        [[nodiscard]] std::size_t value() const noexcept { return value_; }
        [[nodiscard]] std::size_t size() const noexcept { return S_; }

    private:
        static std::string make_message(const char *family, std::size_t index, std::size_t value, std::size_t S) {
            return std::string("Invalid U_") + family + ": index=" + std::to_string(index)
                    + ", value=" + std::to_string(value) + ", allowed=[0," + std::to_string(S) + "]";
        }

        std::string family_;
        std::size_t index_;
        std::size_t value_;
        std::size_t S_;
    };
}
