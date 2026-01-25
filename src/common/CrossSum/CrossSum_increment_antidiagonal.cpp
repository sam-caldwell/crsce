/**
 * @file CrossSum_increment_antidiagonal.cpp
 * @brief Increment along the anti-diagonal mapping (r - c) mod kSize.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "common/CrossSum/CrossSum.h"
#include <cstddef>

namespace crsce::common {
    /**
     * @name increment_antidiagonal
     * @brief Increment along the anti-diagonal mapping (r - c) mod kSize.
     * @param r Row index.
     * @param c Column index.
     * @return N/A
     * @throws None
     */
    void CrossSum::increment_antidiagonal(const std::size_t r, const std::size_t c) {
        const std::size_t idx = (r >= c) ? (r - c) : (r + kSize - c);
        elems_.at(idx) = static_cast<ValueType>(elems_.at(idx) + 1);
    }
} // namespace crsce::common
