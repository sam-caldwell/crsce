/**
 * @file gobp_solver_reset_noop_test.cpp
 */
#include <gtest/gtest.h>

#include <cstddef>

#include "decompress/Csm/Csm.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Gobp/GobpSolver.h"

using crsce::decompress::Csm;
using crsce::decompress::ConstraintState;
using crsce::decompress::GobpSolver;

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
