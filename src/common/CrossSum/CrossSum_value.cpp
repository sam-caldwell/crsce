/**
 * @file CrossSum_value.cpp
 * @brief Accessor for CrossSum values.
 */
#include "common/CrossSum/CrossSum.h"
#include <cstddef>

namespace crsce::common {

auto CrossSum::value(std::size_t i) const noexcept -> ValueType {
  if (i < kSize) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    return elems_[i];
  }
  return static_cast<ValueType>(0);
}

}  // namespace crsce::common
