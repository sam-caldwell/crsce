/**
 * @file ClampInt.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "common/Util/ClampInt.h"

namespace crsce::common::util {
/**
 * @name clamp_int
 * @brief Clamp an integer to the closed interval [lo, hi].
 * @param v Input value.
 * @param lo Lower bound (inclusive).
 * @param hi Upper bound (inclusive).
 * @return v constrained to lie within [lo, hi].
 * @details Small utility used where inputs must be bounded without branching at call sites.
 */
int clamp_int(const int v, const int lo, const int hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}
} // namespace crsce::common::util
