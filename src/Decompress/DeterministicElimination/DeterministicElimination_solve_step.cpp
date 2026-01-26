/**
 * @file DeterministicElimination_solve_step.cpp
 * @brief Implementation of DeterministicElimination::solve_step.
 * © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Exceptions/DeterministicEliminationError.h"

#include <cstddef>

namespace crsce::decompress {
    /**
     * @name DeterministicElimination::solve_step
     * @brief Perform one pass of forced-move propagation.
     * @return std::size_t Number of newly solved cells this step.
     */
    std::size_t DeterministicElimination::solve_step() {
        validate_bounds(st_);
        std::size_t progress = 0;
        // Rows
        for (std::size_t r = 0; r < S; ++r) {
            const auto U = st_.U_row.at(r);
            const auto R = st_.R_row.at(r);
            if (U == 0) {
                if (R != 0) {
                    throw DeterministicEliminationError("row contradiction: U=0 but R>0");
                }
                continue;
            }
            if (R == 0) {
                force_row(r, false, progress);
            } else if (R == U) {
                force_row(r, true, progress);
            }
        }
        // Column/diag/xdiag passes are deferred to subsequent iterations to avoid double-counting within one step.
        return progress;
    }
} // namespace crsce::decompress
