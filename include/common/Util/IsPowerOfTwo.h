/**
 * @file IsPowerOfTwo.h
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

namespace crsce::common::util {
    /**
     * @name is_power_of_two
     * @brief Test whether an unsigned integer is a non-zero power of two.
     * @usage bool ok = is_power_of_two(8); // true
     * @throws None
     * @param x Value to test.
     * @return true if x is one of {1, 2, 4, 8, ...}; false for zero or non-powers.
     */
    bool is_power_of_two(unsigned x);
} // namespace crsce::common::util
