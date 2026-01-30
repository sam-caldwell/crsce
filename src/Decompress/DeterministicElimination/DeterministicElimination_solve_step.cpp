/**
 * @file DeterministicElimination_solve_step.cpp
 * @brief Implementation of DeterministicElimination::solve_step.
 * © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "common/exceptions/DeterministicEliminationError.h"

#include <cstddef>
#include <array>
#include <iostream>

namespace crsce::decompress {
    /**
     * @name DeterministicElimination::solve_step
     * @brief Perform one pass of forced-move propagation.
     * @return std::size_t Number of newly solved cells this step.
     */
    std::size_t DeterministicElimination::solve_step() {
        validate_bounds(st_);
        std::size_t progress = 0;
        // Snapshot state at the beginning of the step so that we only enforce
        // moves that are already forced before any assignments in this step.
        const auto U_row0 = st_.U_row;
        const auto U_col0 = st_.U_col;
        const auto U_diag0 = st_.U_diag;
        const auto U_xdg0 = st_.U_xdiag;
        const auto R_row0 = st_.R_row;
        const auto R_col0 = st_.R_col;
        const auto R_diag0 = st_.R_diag;
        const auto R_xdg0 = st_.R_xdiag;

        for (std::size_t i = 0; i < S; ++i) {
            {
                // Rows
                const auto r = i;
                const auto U = U_row0.at(r);
                const auto R = R_row0.at(r);
                if (U == 0) {
                    if (R != 0) {
                        throw DeterministicEliminationError("row contradiction: U=0 but R>0");
                    }
                } else if (R == 0) {
                    force_row(r, false, progress);
                } else if (R == U) {
                    force_row(r, true, progress);
                }
            }
            {
                // Columns
                const auto c = i;
                const auto U = U_col0.at(c);
                const auto R = R_col0.at(c);
                if (U == 0) {
                    if (R != 0) {
                        throw DeterministicEliminationError("col contradiction: U=0 but R>0");
                    }
                } else if (R == 0) {
                    force_col(c, false, progress);
                } else if (R == U) {
                    force_col(c, true, progress);
                }
            }
            {
                // Main diagonals
                const auto d = i;
                const auto U = U_diag0.at(d);
                const auto R = R_diag0.at(d);
                if (U == 0) {
                    if (R != 0) {
                        throw DeterministicEliminationError("diag contradiction: U=0 but R>0");
                    }
                } else if (R == 0) {
                    force_diag(d, false, progress);
                } else if (R == U) {
                    force_diag(d, true, progress);
                }
            }
            {
                // Anti-diagonals
                const auto x = i;
                const auto U = U_xdg0.at(x);
                const auto R = R_xdg0.at(x);
                if (U == 0) {
                    if (R != 0) {
                        throw DeterministicEliminationError("xdiag contradiction: U=0 but R>0");
                    }
                } else if (R == 0) {
                    force_xdiag(x, false, progress);
                } else if (R == U) {
                    force_xdiag(x, true, progress);
                }
            }
        }
        // Note: This function intentionally aggregates progress across all families in one step.
        std::cerr << "Deterministic Elimination exit with no error (progress: " << progress << ")\n";
        return progress;
    }
} // namespace crsce::decompress
