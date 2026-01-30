/**
 * @file GobpSolver_solve_step.cpp
 * @brief Implementation of GobpSolver::solve_step.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Gobp/GobpSolver.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name GobpSolver::solve_step
     * @brief Perform one iteration of GOBP smoothing and assignments.
     * @return std::size_t Number of newly solved cells.
     */
    std::size_t GobpSolver::solve_step() {
        std::size_t progress = 0;
        const double hi = assign_confidence_;
        const double lo = 1.0 - assign_confidence_;
        // Iterate over grid row-major; compute belief; smooth with existing data; store; assign if extreme
        for (std::size_t r = 0; r < S; ++r) {
            for (std::size_t c = 0; c < S; ++c) {
                if (csm_.is_locked(r, c)) {
                    continue; // skip assigned cells
                }
                const double belief = belief_for(r, c);
                // Exponential smoothing (damping)
                const double prev = csm_.get_data(r, c);
                const double blended = (damping_ * prev) + ((1.0 - damping_) * belief);
                csm_.set_data(r, c, blended);

                if (blended >= hi) {
                    apply_cell(r, c, true);
                    ++progress;
                } else if (blended <= lo) {
                    apply_cell(r, c, false);
                    ++progress;
                }
            }
        }
        return progress;
    }
} // namespace crsce::decompress
