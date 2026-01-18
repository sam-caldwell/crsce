/**
 * @file ClampInt.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "common/Util/ClampInt.h"

namespace crsce::common::util {
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
