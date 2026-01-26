/**
 * @file gobp_solver_row_forced_zeros_test.cpp
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Gobp/GobpSolver.h"

using crsce::decompress::Csm;
using crsce::decompress::ConstraintState;
using crsce::decompress::GobpSolver;

/**

 * @name GobpSolverHappy.ForceRowAllZeros

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(GobpSolverHappy, ForceRowAllZeros) { // NOLINT
    Csm csm;
    ConstraintState st{};

    // Initialize all lines with U=511 and neutral R=~half
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS);
        st.R_row.at(i) = st.R_col.at(i) = st.R_diag.at(i) = st.R_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS / 2);
    }
    // Row 1: fully forced to zeros
    st.R_row.at(1) = 0;

    GobpSolver gobp{csm, st, /*damping*/ 0.0, /*assign_confidence*/ 0.99};
    const auto progress = gobp.solve_step();
    EXPECT_EQ(progress, Csm::kS);

    // Check row 1 bits and locks
    for (std::size_t c = 0; c < Csm::kS; ++c) {
        EXPECT_TRUE(csm.is_locked(1, c));
        EXPECT_FALSE(csm.get(1, c));
    }
    // Residuals for row 1 go to zero
    EXPECT_EQ(st.U_row.at(1), 0);
    EXPECT_EQ(st.R_row.at(1), 0);

    // Example column index c=7: got one assignment (0)
    const std::size_t c = 7;
    EXPECT_EQ(st.U_col.at(c), static_cast<std::uint16_t>(Csm::kS - 1));
    EXPECT_EQ(st.R_col.at(c), static_cast<std::uint16_t>(Csm::kS / 2));

    EXPECT_FALSE(gobp.solved());
}
