/**
 * @file IsPowerOfTwo.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "common/Util/IsPowerOfTwo.h"

namespace crsce::common::util {

bool is_power_of_two(unsigned x) {
  return x != 0U && (x & (x - 1U)) == 0U;
}

}  // namespace crsce::common::util
