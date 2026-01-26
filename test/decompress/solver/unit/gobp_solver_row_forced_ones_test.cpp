/**
 * @file gobp_solver_row_forced_ones_test.cpp
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

 * @name GobpSolverHappy.ForceRowAllOnes

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(GobpSolverHappy, ForceRowAllOnes) { // NOLINT
    Csm csm;
    ConstraintState st{};

    // Initialize all lines with U=511 and neutral R=~half
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS);
        st.R_row.at(i) = st.R_col.at(i) = st.R_diag.at(i) = st.R_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS / 2);
    }
    // Row 0: fully forced to ones
    st.R_row.at(0) = static_cast<std::uint16_t>(Csm::kS);

    GobpSolver gobp{csm, st, /*damping*/ 0.0, /*assign_confidence*/ 0.99};
    const auto progress = gobp.solve_step();
    EXPECT_EQ(progress, Csm::kS);

    // Check row 0 bits and locks
    for (std::size_t c = 0; c < Csm::kS; ++c) {
        EXPECT_TRUE(csm.is_locked(0, c));
        EXPECT_TRUE(csm.get(0, c));
    }
    // Residuals for row 0 go to zero
    EXPECT_EQ(st.U_row.at(0), 0);
    EXPECT_EQ(st.R_row.at(0), 0);

    // One example column index c=5: got one assignment of 1
    const std::size_t c = 5;
    EXPECT_EQ(st.U_col.at(c), static_cast<std::uint16_t>(Csm::kS - 1));
    EXPECT_EQ(st.R_col.at(c), static_cast<std::uint16_t>((Csm::kS / 2) - 1));

    EXPECT_FALSE(gobp.solved());
}
