/**
 * @file radditz_sift_phase_basic_test.cpp
 * @brief Unit tests for RadditzSiftPhase column-focused adoption.
 */
#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Phases/RadditzSiftPhase.h"

using crsce::decompress::Csm;
using crsce::decompress::ConstraintState;
using crsce::decompress::BlockSolveSnapshot;
using crsce::decompress::phases::radditz_sift_phase;

namespace {
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

TEST(RadditzSiftPhaseBasic, FillsColumnDeficitSafely) { // NOLINT
    constexpr std::size_t S = Csm::kS;
    Csm csm; csm.reset();
    ConstraintState st{}; init_state_all_unknowns(st);

    // Two rows each need one 1
    const std::size_t r0 = 2;
    const std::size_t r1 = 9;
    st.R_row.at(r0) = 1; st.R_row.at(r1) = 1;

    // A single column needs two ones
    const std::size_t cDef = 7;
    st.R_col.at(cDef) = 2;

    // Diag/xdiag permissive everywhere
    for (std::size_t i = 0; i < S; ++i) {
        st.R_diag.at(i) = 100; st.R_xdiag.at(i) = 100;
    }

    BlockSolveSnapshot snap{};
    const std::size_t adopted = radditz_sift_phase(csm, st, snap, /*max_cols*/1);

    EXPECT_EQ(adopted, 2U);
    EXPECT_EQ(st.R_col.at(cDef), 0U);
    EXPECT_EQ(st.R_row.at(r0), 0U);
    EXPECT_EQ(st.R_row.at(r1), 0U);
    EXPECT_TRUE(csm.is_locked(r0, cDef));
    EXPECT_TRUE(csm.get(r0, cDef));
    EXPECT_TRUE(csm.is_locked(r1, cDef));
    EXPECT_TRUE(csm.get(r1, cDef));

    // Snapshot instrumentation
    EXPECT_GE(snap.time_radditz_ms, 0U);
    EXPECT_EQ(snap.radditz_iterations, 1U);
    EXPECT_GE(snap.partial_adoptions, 2U);
}
