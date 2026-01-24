/**
 * @file IsPowerOfTwo.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "common/Util/IsPowerOfTwo.h"

namespace crsce::common::util {
/**
 * @name is_power_of_two
 * @brief Return true if x is a non-zero power of two.
 * @param x Unsigned integer to test.
 * @return true when exactly one bit is set (x ∈ {1,2,4,...}); false for 0 or non-powers.
 * @details Common check for sizes/alignments where a power-of-two constraint is required.
 */
bool is_power_of_two(unsigned const x) {
  return x != 0U && (x & (x - 1U)) == 0U;
}
} // namespace crsce::common::util
