/**
 * @file SafeDivide.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "common/Util/SafeDivide.h"
#include <optional>

namespace crsce::common::util {
/**
 * @name safe_divide
 * @brief Perform integer division with zero-division safety.
 * @param a Dividend.
 * @param b Divisor; if zero, returns std::nullopt.
 * @return Quotient wrapped in std::optional when b != 0; otherwise std::nullopt.
 * @details Avoids undefined behavior and exception overhead for common error path.
 */
std::optional<int> safe_divide(const int a, const int b) {
  if (b == 0) {
    return std::nullopt;
  }
  return a / b;
}
} // namespace crsce::common::util
