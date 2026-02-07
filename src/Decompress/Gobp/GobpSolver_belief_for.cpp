/**
 * @file GobpSolver_belief_for.cpp
 * @brief Implementation of GobpSolver::belief_for.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Gobp/GobpSolver.h"
#include <cstddef>
#include <cstdint> //NOLINT

namespace crsce::decompress {
    /**
     * @name GobpSolver::belief_for
     * @brief Compute per-cell belief based on current residuals.
     * @param r Row index.
     * @param c Column index.
     * @return double Belief value in [0,1].
     */
    double GobpSolver::belief_for(const std::size_t r, const std::size_t c) const {
        const auto d = diag_index(r, c);
        const auto x = xdiag_index(r, c);

        const double pr = calculate_belief(st_.R_row.at(r), st_.U_row.at(r)); // Lateral
        const double pc = calculate_belief(st_.R_col.at(c), st_.U_col.at(c)); // Vertical
        const double pd = calculate_belief(st_.R_diag.at(d), st_.U_diag.at(d)); // Diagonal
        const double px = calculate_belief(st_.R_xdiag.at(x), st_.U_xdiag.at(x)); // Antidiagonal

        // Combine line beliefs by multiplying odds (naive independence assumption)
        const double o = odds(pr) * odds(pc) * odds(pd) * odds(px);
        return prob_from_odds(o);
    }
} // namespace crsce::decompress
