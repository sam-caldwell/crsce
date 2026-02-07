/**
 * @file safe_divide.h
 * @brief Perform integer division with zero-division safety.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <optional>

namespace crsce::common::util {
    /**
     * @name safe_divide
     * @brief Perform integer division with zero-division safety.
     * @param a Dividend.
     * @param b Divisor; if zero, returns std::nullopt.
     * @return std::optional<int> containing the quotient if b != 0; otherwise std::nullopt.
     */
    std::optional<int> safe_divide(int a, int b);
}
