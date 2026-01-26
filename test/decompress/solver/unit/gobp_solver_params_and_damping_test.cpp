/**
 * @file gobp_solver_params_and_damping_test.cpp
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
 * @name GobpSolverParams.ClampingAndGetters
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(GobpSolverParams, ClampingAndGetters) { // NOLINT
    Csm csm;
    ConstraintState st{};
    // Fill U/R to neutral so solver can run if needed
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS);
        st.R_row.at(i) = st.R_col.at(i) = st.R_diag.at(i) = st.R_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS / 2);
    }

    GobpSolver gobp{csm, st, /*damping*/ -0.5, /*assign_confidence*/ 2.0};
    EXPECT_DOUBLE_EQ(gobp.damping(), 0.0);
    EXPECT_DOUBLE_EQ(gobp.assign_confidence(), 1.0);

    gobp.set_damping(1.5);
    EXPECT_DOUBLE_EQ(gobp.damping(), 1.0);

    gobp.set_damping(0.25);
    EXPECT_DOUBLE_EQ(gobp.damping(), 0.25);

    gobp.set_assign_confidence(0.1);
    EXPECT_GT(gobp.assign_confidence(), 0.5); // clamped strictly above 0.5
    EXPECT_LE(gobp.assign_confidence(), 1.0);
}

/**
 * @name GobpSolverParams.DampingBlendsPreviousData
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(GobpSolverParams, DampingBlendsPreviousData) { // NOLINT
    Csm csm;
    ConstraintState st{};
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS);
        st.R_row.at(i) = st.R_col.at(i) = st.R_diag.at(i) = st.R_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS / 2);
    }

    const std::size_t r = 10;
    const std::size_t c = 20;
    csm.set_data(r, c, 1.0);

    GobpSolver gobp{csm, st, /*damping*/ 0.25, /*assign_confidence*/ 0.999999};
    (void) gobp.solve_step();

    const auto odds = [](double p) { return p / (1.0 - p); };
    const auto prob_from_odds = [](double o) { return o / (1.0 + o); };
    const double p_line = static_cast<double>(Csm::kS / 2) / static_cast<double>(Csm::kS);
    const double belief = prob_from_odds(odds(p_line) * odds(p_line) * odds(p_line) * odds(p_line));
    const double expected = (0.25 * 1.0) + (0.75 * belief);
    EXPECT_NEAR(csm.get_data(r, c), expected, 1e-9);
    EXPECT_FALSE(csm.is_locked(r, c));
}

/**
 * @name GobpSolverParams.SkipsLockedCells
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(GobpSolverParams, SkipsLockedCells) { // NOLINT
    Csm csm;
    ConstraintState st{};
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS);
        st.R_row.at(i) = st.R_col.at(i) = st.R_diag.at(i) = st.R_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS / 2);
    }

    const std::size_t r = 50;
    const std::size_t c = 60;
    csm.set_data(r, c, 0.123);
    csm.lock(r, c);

    GobpSolver gobp{csm, st, /*damping*/ 0.0, /*assign_confidence*/ 0.99};
    (void) gobp.solve_step();

    EXPECT_TRUE(csm.is_locked(r, c));
    EXPECT_DOUBLE_EQ(csm.get_data(r, c), 0.123);
}

/**
 * @name GobpSolverParams.SolvedReturnsTrueWhenAllUZero
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(GobpSolverParams, SolvedReturnsTrueWhenAllUZero) { // NOLINT
    Csm csm;
    ConstraintState st{};
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = 0;
    }
    const GobpSolver gobp{csm, st};
    EXPECT_TRUE(gobp.solved());
}
