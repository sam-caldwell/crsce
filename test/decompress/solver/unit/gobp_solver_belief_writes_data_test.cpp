/**
 * @file gobp_solver_belief_writes_data_test.cpp
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include "decompress/Csm/Csm.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Gobp/GobpSolver.h"

using crsce::decompress::Csm;
using crsce::decompress::ConstraintState;
using crsce::decompress::GobpSolver;

TEST(GobpSolverBasics, BeliefWritesDataAndNoAssignmentsUnderNeutral) { // NOLINT
    Csm csm;
    ConstraintState st{};

    // Neutral constraints everywhere: U=511, R=~half so belief ~0.5
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS);
        st.R_row.at(i) = st.R_col.at(i) = st.R_diag.at(i) = st.R_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS / 2);
    }

    GobpSolver gobp{csm, st, /*damping*/ 0.0, /*assign_confidence*/ 0.99};
    const auto progress = gobp.solve_step();
    EXPECT_EQ(progress, 0U);

    // Expected belief under neutral constraints: combine 4 identical p=R/U via odds-product
    const auto odds = [](double p) { return p / (1.0 - p); };
    const auto prob_from_odds = [](double o) { return o / (1.0 + o); };
    constexpr double p_line = static_cast<double>(Csm::kS / 2) / static_cast<double>(Csm::kS);
    constexpr double expected = prob_from_odds(odds(p_line) * odds(p_line) * odds(p_line) * odds(p_line));

    // Check a few cells have data set to expected and are not locked
    const std::size_t r = 10;
    const std::size_t c = 20;
    EXPECT_FALSE(csm.is_locked(r, c));
    const double v = csm.get_data(r, c);
    EXPECT_NEAR(v, expected, 1e-9);
}
