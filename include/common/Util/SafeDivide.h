/**
 * @file SafeDivide.h
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <optional>

namespace crsce::common::util {
    /**
     * @name safe_divide
     * @brief Perform integer division with zero-division safety.
     * @usage auto q = safe_divide(9, 3); // q.has_value() == true
     * @throws None
     * @param a Dividend.
     * @param b Divisor; if zero, returns std::nullopt.
     * @return std::optional<int> containing the quotient if b != 0; otherwise std::nullopt.
     */
    std::optional<int> safe_divide(int a, int b);

    /**
     * @name SafeDivideTag
     * @brief Tag type to satisfy one-definition-per-header for safe-divide utility.
     */
    struct SafeDivideTag {};
} // namespace crsce::common::util
