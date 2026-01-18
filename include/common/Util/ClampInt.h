/**
 * @file ClampInt.h
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

namespace crsce::common::util {
    /**
     * @name clamp_int
     * @brief Clamp an integer to the closed interval [lo, hi].
     * @usage int y = clamp_int(x, 0, 10);
     * @throws None
     * @param v Input value to clamp.
     * @param lo Lower bound (inclusive).
     * @param hi Upper bound (inclusive).
     * @return The value v constrained to lie within [lo, hi].
     */
    int clamp_int(int v, int lo, int hi);
} // namespace crsce::common::util
