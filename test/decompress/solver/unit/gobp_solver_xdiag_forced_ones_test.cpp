/**
 * @file gobp_solver_xdiag_forced_ones_test.cpp
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
 * @name GobpSolverHappy.ForceXdiagAllOnes
 * @brief Intent: Ensure GOBP respects anti-diagonal residuals symmetrically with rows/cols.
 */
TEST(GobpSolverHappy, ForceXdiagAllOnes) { // NOLINT
    Csm csm;
    ConstraintState st{};

    // Initialize all lines with U=511 and neutral R=~half
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS);
        st.R_row.at(i) = st.R_col.at(i) = st.R_diag.at(i) = st.R_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS / 2);
    }
    // Anti-diagonal x: fully forced to ones
    const std::size_t x = 7;
    st.R_xdiag.at(x) = static_cast<std::uint16_t>(Csm::kS);

    GobpSolver gobp{csm, st, /*damping*/ 0.0, /*assign_confidence*/ 0.99};
    const auto progress = gobp.solve_step();
    EXPECT_EQ(progress, Csm::kS);

    // Check all cells on anti-diagonal x are assigned 1 and locked
    for (std::size_t r = 0; r < Csm::kS; ++r) {
        const std::size_t c = (r >= x) ? (r - x) : (r + Csm::kS - x); // r - c == x (mod S)
        EXPECT_TRUE(csm.is_locked(r, c));
        EXPECT_TRUE(csm.get(r, c));
    }
    // Residuals for xdiag x go to zero
    EXPECT_EQ(st.U_xdiag.at(x), 0);
    EXPECT_EQ(st.R_xdiag.at(x), 0);

    // Sample a specific (r,c) on xdiag x and check row residuals updated correctly for a 1 assignment
    const std::size_t r = 12;
    const std::size_t c = (r >= x) ? (r - x) : (r + Csm::kS - x);
    EXPECT_EQ(st.U_row.at(r), static_cast<std::uint16_t>(Csm::kS - 1));
    EXPECT_EQ(st.R_row.at(r), static_cast<std::uint16_t>((Csm::kS / 2) - 1));

    EXPECT_FALSE(gobp.solved());
}
