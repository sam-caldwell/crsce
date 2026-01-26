/**
 * @file GobpSolver_odds.cpp
 * @brief Implementation of GobpSolver::odds.
 * © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Gobp/GobpSolver.h"
#include <algorithm>

namespace crsce::decompress {
    /**
     * @name GobpSolver::odds
     * @brief Convert probability to odds ratio p/(1-p).
     * @param p Probability in (0,1).
     * @return double Odds value.
     */
    double GobpSolver::odds(const double p) noexcept {
        const double eps = 1e-12;
        const double pp = std::clamp(p, eps, 1.0 - eps);
        return pp / (1.0 - pp);
    }
} // namespace crsce::decompress
