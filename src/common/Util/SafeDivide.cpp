/**
 * @file SafeDivide.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "common/Util/SafeDivide.h"
#include <optional>

namespace crsce::common::util {
/**
 * @brief Implementation detail.
 */
std::optional<int> safe_divide(const int a, const int b) {
  if (b == 0) {
    return std::nullopt;
  }
  return a / b;
}
} // namespace crsce::common::util
