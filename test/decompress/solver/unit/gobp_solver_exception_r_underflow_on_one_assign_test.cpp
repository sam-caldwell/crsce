/**
 * @file gobp_solver_exception_r_underflow_on_one_assign_test.cpp
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
 * @name GobpSolverSad.ThrowsOnRAssignUnderflowForOne
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(GobpSolverSad, ThrowsOnRAssignUnderflowForOne) { // NOLINT
    Csm csm;
    ConstraintState st{};

    // Initialize all lines with U=511 and R=511 so beliefs would be high, but we will override column R=0.
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS);
        st.R_row.at(i) = st.R_col.at(i) = st.R_diag.at(i) = st.R_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS);
    }

    const std::size_t r = 7;
    const std::size_t c = 9;
    // Force column R to 0 to make assigning a '1' cause underflow on R_col.
    st.R_col.at(c) = 0;

    // Make the solver choose to assign 1 regardless of current belief by setting damping=1 and prev data=1.0.
    csm.set_data(r, c, 1.0);

    GobpSolver gobp{csm, st, /*damping*/ 1.0, /*assign_confidence*/ 0.9};
    EXPECT_THROW({ (void)gobp.solve_step(); }, std::runtime_error);
}
