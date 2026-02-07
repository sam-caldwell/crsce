/**
 * @file is_power_of_two.h
 * @brief Power-of-two predicate utility for unsigned ints.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

namespace crsce::common::util {
    /**
     * @name is_power_of_two
     * @brief Test whether an unsigned integer is a non-zero power of two.
     * @param x Value to test.
     * @return true if x is one of {1, 2, 4, 8, ...}; false for zero or non-powers.
     */
    bool is_power_of_two(unsigned x);
}
