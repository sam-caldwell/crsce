/**
 * @file gobp_solver_diag_forced_zeros_test.cpp
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
 * @name GobpSolverHappy.ForceDiagAllZeros
 * @brief Intent: Ensure GOBP respects diagonal residuals symmetrically with rows/cols.
 */
TEST(GobpSolverHappy, ForceDiagAllZeros) { // NOLINT
    Csm csm;
    ConstraintState st{};

    // Initialize all lines with U=511 and neutral R=~half
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS);
        st.R_row.at(i) = st.R_col.at(i) = st.R_diag.at(i) = st.R_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS / 2);
    }
    // Diagonal d: fully forced to zeros
    const std::size_t d = 5;
    st.R_diag.at(d) = 0;

    GobpSolver gobp{csm, st, /*damping*/ 0.0, /*assign_confidence*/ 0.99};
    const auto progress = gobp.solve_step();
    EXPECT_EQ(progress, Csm::kS);

    // Check all cells on diagonal d are assigned 0 and locked
    for (std::size_t r = 0; r < Csm::kS; ++r) {
        const std::size_t c = (d + Csm::kS - (r % Csm::kS)) % Csm::kS; // r + c == d (mod S)
        EXPECT_TRUE(csm.is_locked(r, c));
        EXPECT_FALSE(csm.get(r, c));
    }
    // Residuals for diagonal d go to zero
    EXPECT_EQ(st.U_diag.at(d), 0);
    EXPECT_EQ(st.R_diag.at(d), 0);

    // Sample a specific (r,c) on diag d and check column residuals updated correctly for a 0 assignment
    const std::size_t r = 10;
    const std::size_t c = (d + Csm::kS - (r % Csm::kS)) % Csm::kS;
    EXPECT_EQ(st.U_col.at(c), static_cast<std::uint16_t>(Csm::kS - 1));
    EXPECT_EQ(st.R_col.at(c), static_cast<std::uint16_t>(Csm::kS / 2));

    EXPECT_FALSE(gobp.solved());
}
