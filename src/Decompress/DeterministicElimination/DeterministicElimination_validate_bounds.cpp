/**
 * @file DeterministicElimination_validate_bounds.cpp
 * @brief Implementation of DeterministicElimination::validate_bounds.
 * © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"

#include <cstddef>
#include "common/exceptions/ConstraintBoundsInvalid.h"
#include "common/exceptions/ConstraintInvariantViolation.h"

namespace crsce::decompress {
    /**
     * @name DeterministicElimination::validate_bounds
     * @brief Validate feasibility 0 ≤ R ≤ U across all constraint lines.
     * @param st Constraint state to validate.
     * @return void
     */
    void DeterministicElimination::validate_bounds(const ConstraintState &st) {
        for (std::size_t i = 0; i < S; ++i) {
            if (st.U_row.at(i) > S) {
                throw ConstraintBoundsInvalid("row", i, st.U_row.at(i), S);
            }
            if (st.U_col.at(i) > S) {
                throw ConstraintBoundsInvalid("col", i, st.U_col.at(i), S);
            }
            if (st.U_diag.at(i) > S) {
                throw ConstraintBoundsInvalid("diag", i, st.U_diag.at(i), S);
            }
            if (st.U_xdiag.at(i) > S) {
                throw ConstraintBoundsInvalid("xdiag", i, st.U_xdiag.at(i), S);
            }
            if (st.R_row.at(i) > st.U_row.at(i)) {
                throw ConstraintInvariantViolation("row", i, st.R_row.at(i), st.U_row.at(i));
            }
            if (st.R_col.at(i) > st.U_col.at(i)) {
                throw ConstraintInvariantViolation("col", i, st.R_col.at(i), st.U_col.at(i));
            }
            if (st.R_diag.at(i) > st.U_diag.at(i)) {
                throw ConstraintInvariantViolation("diag", i, st.R_diag.at(i), st.U_diag.at(i));
            }
            if (st.R_xdiag.at(i) > st.U_xdiag.at(i)) {
                throw ConstraintInvariantViolation("xdiag", i, st.R_xdiag.at(i), st.U_xdiag.at(i));
            }
        }
    }
} // namespace crsce::decompress
