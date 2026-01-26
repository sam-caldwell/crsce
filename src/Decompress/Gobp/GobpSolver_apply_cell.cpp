/**
 * @file GobpSolver_apply_cell.cpp
 * @brief Implementation of GobpSolver::apply_cell.
 * © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Gobp/GobpSolver.h"
#include <cstddef>
#include <stdexcept>

namespace crsce::decompress {
    /**
     * @name GobpSolver::apply_cell
     * @brief Apply concrete assignment to (r,c), update residuals, lock cell.
     * @param r Row index.
     * @param c Column index.
     * @param value Assigned value.
     * @return void
     */
    void GobpSolver::apply_cell(const std::size_t r, const std::size_t c, const bool value) {
        const auto d = diag_index(r, c);
        const auto x = xdiag_index(r, c);

        if (csm_.is_locked(r, c)) {
            return; // already assigned
        }
        if (st_.U_row.at(r) == 0 || st_.U_col.at(c) == 0 || st_.U_diag.at(d) == 0 || st_.U_xdiag.at(x) == 0) {
            throw std::runtime_error("GOBP: U already zero on a line while assigning");
        }
        // Update U first
        --st_.U_row.at(r);
        --st_.U_col.at(c);
        --st_.U_diag.at(d);
        --st_.U_xdiag.at(x);
        // Update R if placing a 1
        if (value) {
            if (st_.R_row.at(r) == 0 || st_.R_col.at(c) == 0 || st_.R_diag.at(d) == 0 || st_.R_xdiag.at(x) == 0) {
                throw std::runtime_error("GOBP: R underflow on a line while assigning 1");
            }
            --st_.R_row.at(r);
            --st_.R_col.at(c);
            --st_.R_diag.at(d);
            --st_.R_xdiag.at(x);
        }

        csm_.put(r, c, value);
        csm_.lock(r, c);
    }
} // namespace crsce::decompress
