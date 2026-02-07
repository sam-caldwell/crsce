/**
 * @file GobpSolver_solved.cpp
 * @brief Implementation of GobpSolver::solved.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Gobp/GobpSolver.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name GobpSolver::solved
     * @brief Whether the target matrix is fully solved.
     * @return bool True if solved; false otherwise.
     */
    bool GobpSolver::solved() const {
        for (std::size_t i = 0; i < S; ++i) {
            if (st_.U_row.at(i) != 0 || st_.U_col.at(i) != 0 || st_.U_diag.at(i) != 0 || st_.U_xdiag.at(i) != 0) {
                return false;
            }
        }
        return true;
    }
} // namespace crsce::decompress
