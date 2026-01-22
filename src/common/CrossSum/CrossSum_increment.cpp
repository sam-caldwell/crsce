/**
 * @file CrossSum_increment.cpp
 * @brief Increment functions for CrossSum.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "common/CrossSum/CrossSum.h"
#include <cstddef>

namespace crsce::common {
    /**
     * @brief Implementation detail.
     */
    void CrossSum::increment(std::size_t i) {
        if (i < kSize) {
            elems_.at(i) = static_cast<ValueType>(elems_.at(i) + 1);
        }
    }

    void CrossSum::increment_diagonal(std::size_t r, std::size_t c) {
        const std::size_t idx = (r + c) % kSize;
        elems_.at(idx) = static_cast<ValueType>(elems_.at(idx) + 1);
    }

    void CrossSum::increment_antidiagonal(std::size_t r, std::size_t c) {
        const std::size_t idx = (r >= c) ? (r - c) : (r + kSize - c);
        elems_.at(idx) = static_cast<ValueType>(elems_.at(idx) + 1);
    }
} // namespace crsce::common
