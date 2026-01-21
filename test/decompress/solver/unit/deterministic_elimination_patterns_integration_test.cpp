/**
 * @file deterministic_elimination_patterns_integration_test.cpp
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include "decompress/Csm/Csm.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"

using crsce::decompress::Csm;
using crsce::decompress::ConstraintState;
using crsce::decompress::DeterministicElimination;

TEST(DeterministicEliminationPatterns, AllZerosEliminatedByDE) { // NOLINT
    Csm csm;
    ConstraintState st{};
    // Initialize all lines: U=S, R=0 so rows are forced to zero.
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS);
        st.R_row.at(i) = 0;
        st.R_col.at(i) = 0;      // not used by row-forcing zeros; kept consistent
        st.R_diag.at(i) = 0;
        st.R_xdiag.at(i) = 0;
    }
    DeterministicElimination de{csm, st};
    // Placeholder hash-based elimination should be a no-op for this scenario
    EXPECT_EQ(de.hash_step(), 0U);
    const auto progress = de.solve_step();
    EXPECT_EQ(progress, Csm::kS * Csm::kS);
    // All bits 0 and locked
    for (std::size_t r = 0; r < Csm::kS; ++r) {
        for (std::size_t c = 0; c < Csm::kS; ++c) {
            EXPECT_TRUE(csm.is_locked(r, c));
            EXPECT_FALSE(csm.get(r, c));
        }
    }
    EXPECT_TRUE(de.solved());
}

TEST(DeterministicEliminationPatterns, AllOnesEliminatedByDE) { // NOLINT
    Csm csm;
    ConstraintState st{};
    // Initialize all lines: U=S, R=U=S so rows are forced to one and residuals will reach zero.
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS);
        st.R_row.at(i) = static_cast<std::uint16_t>(Csm::kS);
        st.R_col.at(i) = static_cast<std::uint16_t>(Csm::kS);
        st.R_diag.at(i) = static_cast<std::uint16_t>(Csm::kS);
        st.R_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS);
    }
    DeterministicElimination de{csm, st};
    EXPECT_EQ(de.hash_step(), 0U);
    const auto progress = de.solve_step();
    EXPECT_EQ(progress, Csm::kS * Csm::kS);
    for (std::size_t r = 0; r < Csm::kS; ++r) {
        for (std::size_t c = 0; c < Csm::kS; ++c) {
            EXPECT_TRUE(csm.is_locked(r, c));
            EXPECT_TRUE(csm.get(r, c));
        }
    }
    EXPECT_TRUE(de.solved());
}

TEST(DeterministicEliminationPatterns, AlternatingRowsEliminatedByHashThenDENoop) { // NOLINT
    Csm csm;
    ConstraintState st{};
    // Simulate hash-based DE solved rows: lock alternating patterns and set U=0, R=0 across all families.
    for (std::size_t r = 0; r < Csm::kS; ++r) {
        for (std::size_t c = 0; c < Csm::kS; ++c) {
            const bool bit = (r % 2U == 0U) ? ((c % 2U) != 0U)  // 0101...
                                            : ((c % 2U) == 0U); // 1010...
            csm.put(r, c, bit);
            csm.lock(r, c);
        }
    }
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = 0;
        st.R_row.at(i) = st.R_col.at(i) = st.R_diag.at(i) = st.R_xdiag.at(i) = 0;
    }
    DeterministicElimination de{csm, st};
    EXPECT_EQ(de.hash_step(), 0U);
    const auto progress = de.solve_step();
    EXPECT_EQ(progress, 0U);
    EXPECT_TRUE(de.solved());
}

TEST(DeterministicEliminationPatterns, SingleRowForcedOnesOnly) { // NOLINT
    Csm csm;
    ConstraintState st{};

    // Initialize all lines: U=S. Configure row k to be forced to ones (R=U=S),
    // while other rows are neutral (R=1, not 0 or U). Set other families' R to 1
    // so assigning a single row of ones is feasible (each family sees one '1').
    constexpr std::size_t k = 2;
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS);
        st.R_row.at(i) = 1;
        st.R_col.at(i) = 1;
        st.R_diag.at(i) = 1;
        st.R_xdiag.at(i) = 1;
    }
    st.R_row.at(k) = static_cast<std::uint16_t>(Csm::kS); // force row k to ones

    DeterministicElimination de{csm, st};
    const auto progress = de.solve_step();

    // Expect exactly one full row assigned
    EXPECT_EQ(progress, Csm::kS);

    for (std::size_t c = 0; c < Csm::kS; ++c) {
        EXPECT_TRUE(csm.is_locked(k, c));
        EXPECT_TRUE(csm.get(k, c));
    }

    // Not solved overall (other rows remain with U>0)
    EXPECT_FALSE(de.solved());
}
