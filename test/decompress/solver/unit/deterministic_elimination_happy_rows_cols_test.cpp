/**
 * @file deterministic_elimination_happy_rows_cols_test.cpp
 */
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"

using crsce::decompress::Csm;
using crsce::decompress::ConstraintState;
using crsce::decompress::DeterministicElimination;

/**
 * @name DeterministicEliminationHappy.ForceRowAllZeros
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(DeterministicEliminationHappy, ForceRowAllZeros) { // NOLINT
    Csm csm;
    ConstraintState st{};

    // Initialize all lines with U=511 and R=511 (consistent: R<=U)
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS);
        // Initialize residuals to 510 so setting a row to zeros remains feasible
        st.R_row.at(i) = st.R_col.at(i) = st.R_diag.at(i) = st.R_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS - 1);
    }

    // Row 1: R=0, U=511 -> force all zeros in row 1
    st.R_row.at(1) = 0;

    // Precondition sanity
    ASSERT_EQ(st.R_row.at(1), 0);
    ASSERT_EQ(st.U_row.at(1), static_cast<std::uint16_t>(Csm::kS));

    DeterministicElimination de{csm, st};
    const auto progress = de.solve_step();
    // Expect one full row assigned
    EXPECT_EQ(progress, Csm::kS);

    // Check row 1 bits and locks
    for (std::size_t c = 0; c < Csm::kS; ++c) {
        EXPECT_TRUE(csm.is_locked(1, c));
        EXPECT_FALSE(csm.get(1, c));
    }

    // Residuals for row 1 go to zero
    EXPECT_EQ(st.U_row.at(1), 0);
    EXPECT_EQ(st.R_row.at(1), 0);

    // One example column index c=5: got one assignment (row 1 = 0)
    // U_col should have decreased by 1 and R_col unchanged
    const std::size_t c = 5;
    EXPECT_EQ(st.U_col.at(c), static_cast<std::uint16_t>(Csm::kS - 1));
    EXPECT_EQ(st.R_col.at(c), static_cast<std::uint16_t>(Csm::kS - 1));

    // Not solved overall
    EXPECT_FALSE(de.solved());
}
