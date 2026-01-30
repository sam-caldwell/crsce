/**
 * @file GobpSolver_prob_from_odds.cpp
 * @brief Implementation of GobpSolver::prob_from_odds.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Gobp/GobpSolver.h"
#include <algorithm>

namespace crsce::decompress {
    /**
     * @name GobpSolver::prob_from_odds
     * @brief Convert odds ratio to probability o/(1+o).
     * @param o Odds value > 0.
     * @return double Probability in (0,1).
     */
    double GobpSolver::prob_from_odds(const double o) noexcept {
        const double eps = 1e-12;
        const double oo = std::max(o, eps);
        return oo / (1.0 + oo);
    }
} // namespace crsce::decompress
