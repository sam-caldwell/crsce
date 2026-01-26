/**
 * @file gobp_solver_exception_u_zero_on_assign_test.cpp
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Gobp/GobpSolver.h"

using crsce::decompress::Csm;
using crsce::decompress::ConstraintState;
using crsce::decompress::GobpSolver;

/**

 * @name GobpSolverSad.ThrowsIfULineZeroButAssigns

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(GobpSolverSad, ThrowsIfULineZeroButAssigns) { // NOLINT
    Csm csm;
    ConstraintState st{};

    // Make one row have U=0 while other lines are fully forced to ones, so belief ~1 for that row.
    const std::size_t r = 3;
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS);
        st.R_row.at(i) = st.R_col.at(i) = st.R_diag.at(i) = st.R_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS);
    }
    st.U_row.at(r) = 0; // No uncertainty remains on the row per residuals (contradiction if assigning)

    // Pick one cell in that row and ensure its other line indices are saturated
    const std::size_t c = 4;
    const std::size_t d = (r + c) % Csm::kS;
    const std::size_t x = (r >= c) ? (r - c) : (r + Csm::kS - c);
    st.U_col.at(c) = st.U_diag.at(d) = st.U_xdiag.at(x) = static_cast<std::uint16_t>(Csm::kS);
    st.R_col.at(c) = st.R_diag.at(d) = st.R_xdiag.at(x) = static_cast<std::uint16_t>(Csm::kS);

    GobpSolver gobp{csm, st, /*damping*/ 0.0, /*assign_confidence*/ 0.9};
    EXPECT_THROW({ (void)gobp.solve_step(); }, std::runtime_error);
}
