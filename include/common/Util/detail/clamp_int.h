/**
 * @file clamp_int.h
 * @brief Clamp integer to a closed interval [lo, hi].
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

namespace crsce::common::util {
    /**
     * @name clamp_int
     * @brief Clamp an integer to the closed interval [lo, hi].
     * @param v Input value to clamp.
     * @param lo Lower bound (inclusive).
     * @param hi Upper bound (inclusive).
     * @return The value v constrained to lie within [lo, hi].
     */
    int clamp_int(int v, int lo, int hi);
}
