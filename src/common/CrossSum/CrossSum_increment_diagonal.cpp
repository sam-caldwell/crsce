/**
 * @file CrossSum_increment_diagonal.cpp
 * @brief Increment DSM index using mapping (c - r) mod kSize.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "common/CrossSum/CrossSum.h"
#include <cstddef>

namespace crsce::common {
    /**
     * @name increment_diagonal
     * @brief Increment along the main diagonal mapping (r + c) mod kSize.
     * @param r Row index.
     * @param c Column index.
     * @return N/A
     * @throws None
     */
    void CrossSum::increment_diagonal(const std::size_t r, const std::size_t c) {
        // DSM[d]: cells where c = (r + d) mod S -> d = (c - r) mod S
        const std::size_t idx = (c >= r) ? (c - r) : (c + kSize - r);
        elems_.at(idx) = static_cast<ValueType>(elems_.at(idx) + 1);
    }
} // namespace crsce::common
