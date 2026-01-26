/**
 * @file DeterministicElimination_validate_bounds.cpp
 * @brief Implementation of DeterministicElimination::validate_bounds.
 * © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"

#include <cstddef>
#include <stdexcept>

namespace crsce::decompress {
    /**
     * @name DeterministicElimination::validate_bounds
     * @brief Validate feasibility 0 ≤ R ≤ U across all constraint lines.
     * @param st Constraint state to validate.
     * @return void
     */
    void DeterministicElimination::validate_bounds(const ConstraintState &st) {
        for (std::size_t i = 0; i < S; ++i) {
            if (st.U_row.at(i) > S || st.U_col.at(i) > S || st.U_diag.at(i) > S || st.U_xdiag.at(i) > S) {
                throw std::invalid_argument("U out of range");
            }
            if (st.R_row.at(i) > st.U_row.at(i) || st.R_col.at(i) > st.U_col.at(i) ||
                st.R_diag.at(i) > st.U_diag.at(i) || st.R_xdiag.at(i) > st.U_xdiag.at(i)) {
                throw std::invalid_argument("R > U invariant violated");
            }
        }
    }
} // namespace crsce::decompress
