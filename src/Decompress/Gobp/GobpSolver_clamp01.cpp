/**
 * @file GobpSolver_clamp01.cpp
 * @brief Implementation of GobpSolver::clamp01.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Gobp/GobpSolver.h"

namespace crsce::decompress {
    /**
     * @name GobpSolver::clamp01
     * @brief Clamp probability value to [0,1].
     * @param v Input value.
     * @return double Clamped value in [0,1].
     */
    double GobpSolver::clamp01(const double v) noexcept {
        if (v < 0.0) {
            return 0.0;
        }
        if (v > 1.0) {
            return 1.0;
        }
        return v;
    }
} // namespace crsce::decompress
