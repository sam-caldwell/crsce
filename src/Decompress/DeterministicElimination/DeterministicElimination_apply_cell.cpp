/**
 * @file DeterministicElimination_apply_cell.cpp
 * @brief Implementation of DeterministicElimination::apply_cell.
 * © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "common/exceptions/DeterministicEliminationError.h"
#include <cstddef>

#include <cstddef>

namespace crsce::decompress {
    /**
     * @name DeterministicElimination::apply_cell
     * @brief Assign value to (r,c), update constraints, and lock cell.
     * @param r Row index.
     * @param c Column index.
     * @param value Assigned value for the cell.
     * @return void
     */
    void DeterministicElimination::apply_cell(const std::size_t r, const std::size_t c, const bool value) {
        const auto d = diag_index(r, c);
        const auto x = xdiag_index(r, c);

        if (csm_.is_locked(r, c)) {
            // Already assigned; forced moves apply only to remaining unassigned vars on the line.
            return;
        }
        // Update residuals first to ensure invariants, then write/lock
        if (st_.U_row.at(r) == 0 || st_.U_col.at(c) == 0 || st_.U_diag.at(d) == 0 || st_.U_xdiag.at(x) == 0) {
            throw DeterministicEliminationError("U already zero on a line while assigning");
        }
        --st_.U_row.at(r);
        --st_.U_col.at(c);
        --st_.U_diag.at(d);
        --st_.U_xdiag.at(x);
        if (value) {
            if (st_.R_row.at(r) == 0) {
                throw DeterministicEliminationError("R_row underflow");
            }
            if (st_.R_col.at(c) == 0) {
                throw DeterministicEliminationError("R_col underflow");
            }
            if (st_.R_diag.at(d) == 0) {
                throw DeterministicEliminationError("R_diag underflow");
            }
            if (st_.R_xdiag.at(x) == 0) {
                throw DeterministicEliminationError("R_xdiag underflow");
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
