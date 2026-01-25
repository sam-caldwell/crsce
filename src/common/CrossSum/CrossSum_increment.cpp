/**
 * @file CrossSum_increment.cpp
 * @brief Increment a single CrossSum element by index.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "common/CrossSum/CrossSum.h"
#include <cstddef>

namespace crsce::common {
    /**
     * @name increment
     * @brief Increment a single CrossSum element by index.
     * @param i Zero-based element index (0..kSize-1). Out-of-range is ignored.
     * @return N/A
     * @throws None
     */
    void CrossSum::increment(const std::size_t i) {
        if (i < kSize) {
            elems_.at(i) = static_cast<ValueType>(elems_.at(i) + 1);
        }
    }
} // namespace crsce::common
