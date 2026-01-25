/**
 * @file CrossSum_value.cpp
 * @brief Accessor for CrossSum values.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "common/CrossSum/CrossSum.h"
#include <cstddef>

namespace crsce::common {
    /**
     * @brief Return the value at index i; 0 if out of range.
     * @param i Zero-based element index (0..kSize-1).
     * @return Stored value for index i, or 0 when out of range.
     */
    auto CrossSum::value(std::size_t i) const noexcept -> ValueType {
        if (i < kSize) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            return elems_[i];
        }
        return static_cast<ValueType>(0);
    }
} // namespace crsce::common
