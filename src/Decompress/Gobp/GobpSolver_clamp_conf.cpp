/**
 * @file GobpSolver_clamp_conf.cpp
 * @brief Implementation of GobpSolver::clamp_conf.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Gobp/GobpSolver.h"
#include <algorithm>

namespace crsce::decompress {
    /**
     * @name GobpSolver::clamp_conf
     * @brief Clamp assignment confidence to (0.5,1].
     * @param v Input value.
     * @return double Clamped value in (0.5,1].
     */
    double GobpSolver::clamp_conf(const double v) noexcept {
        // Clamp to (0.5, 1]; use a small epsilon to avoid equality with 0.5.
        const double clamped = std::clamp(v, 0.5000001, 1.0);
        return clamped;
    }
} // namespace crsce::decompress
