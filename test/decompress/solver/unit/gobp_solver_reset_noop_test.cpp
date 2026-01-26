/**
 * @file gobp_solver_reset_noop_test.cpp
 */
#include <gtest/gtest.h>

#include <cstddef>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Gobp/GobpSolver.h"

using crsce::decompress::Csm;
using crsce::decompress::ConstraintState;
using crsce::decompress::GobpSolver;

/**
 * @name GobpSolverBasics.ResetNoop
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(GobpSolverBasics, ResetNoop) { // NOLINT
    Csm csm;
    ConstraintState st{};
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = 0;
    }
    GobpSolver gobp{csm, st};
    gobp.reset();
    EXPECT_TRUE(gobp.solved());
}
