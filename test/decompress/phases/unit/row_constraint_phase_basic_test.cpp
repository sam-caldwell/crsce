/**
 * @file row_constraint_phase_basic_test.cpp
 * @brief Unit tests for RowConstraintPhase greedy adoption.
 */
#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Phases/RowConstraintPhase.h"

using crsce::decompress::Csm;
using crsce::decompress::ConstraintState;
using crsce::decompress::BlockSolveSnapshot;
using crsce::decompress::phases::row_constraint_phase;

namespace {
    // Helper to initialize a default state with all unknowns = S and zero residuals.
    void init_state_all_unknowns(ConstraintState &st) {
        constexpr std::size_t S = Csm::kS;
        st.R_row.fill(0);
        st.R_col.fill(0);
        st.R_diag.fill(0);
        st.R_xdiag.fill(0);
        st.U_row.fill(static_cast<std::uint16_t>(S));
        st.U_col.fill(static_cast<std::uint16_t>(S));
        st.U_diag.fill(static_cast<std::uint16_t>(S));
        st.U_xdiag.fill(static_cast<std::uint16_t>(S));
    }
}

TEST(RowConstraintPhaseBasic, PlacesTopScoredOnesInRow) { // NOLINT
    constexpr std::size_t S = Csm::kS;
    Csm csm;
    csm.reset();
    ConstraintState st{}; init_state_all_unknowns(st);

    // Target row r: needs exactly 2 ones
    const std::size_t r = 3;
    st.R_row.at(r) = 2;

    // Make two columns highly needy to bias selection: cA and cB
    const std::size_t cA = 5;
    const std::size_t cB = 12;
    st.R_col.at(cA) = static_cast<std::uint16_t>(S); // maximal pressure
    st.R_col.at(cB) = static_cast<std::uint16_t>(S);

    // Diag/XDiag residuals broadly permissive
    for (std::size_t i = 0; i < S; ++i) {
        st.R_diag.at(i) = static_cast<std::uint16_t>(S/2);
        st.R_xdiag.at(i) = static_cast<std::uint16_t>(S/2);
    }

    BlockSolveSnapshot snap{};
    const std::size_t adopted = row_constraint_phase(csm, st, snap, /*max_rows*/0);

    // Should adopt exactly 2 cells in row r
    EXPECT_EQ(adopted, 2U);
    EXPECT_EQ(st.R_row.at(r), 0U);

    // Check the two strongest columns were chosen (cA and cB) and are locked to 1
    EXPECT_TRUE(csm.is_locked(r, cA));
    EXPECT_TRUE(csm.get(r, cA));
    EXPECT_TRUE(csm.is_locked(r, cB));
    EXPECT_TRUE(csm.get(r, cB));

    // Residuals in those columns decreased by 1 each
    EXPECT_LT(st.R_col.at(cA), static_cast<std::uint16_t>(S));
    EXPECT_LT(st.R_col.at(cB), static_cast<std::uint16_t>(S));

    // Snapshot instrumentation
    EXPECT_GE(snap.time_row_phase_ms, 0U);
    EXPECT_EQ(snap.row_phase_iterations, 1U);
    EXPECT_GE(snap.partial_adoptions, 2U);
}
