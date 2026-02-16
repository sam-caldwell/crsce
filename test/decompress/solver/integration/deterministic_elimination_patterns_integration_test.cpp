/**
 * @file deterministic_elimination_patterns_integration_test.cpp
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Phases/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include <span>

using crsce::decompress::Csm;
using crsce::decompress::ConstraintState;
using crsce::decompress::DeterministicElimination;

/**
 * @name DeterministicEliminationPatterns.AllZerosEliminatedByDE
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(DeterministicEliminationPatterns, AllZerosEliminatedByDE) { // NOLINT
    Csm csm;
    ConstraintState st{};
    // Initialize all lines: U=S, R=0 so rows are forced to zero.
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS);
        st.R_row.at(i) = 0;
        st.R_col.at(i) = 0; // not used by row-forcing zeros; kept consistent
        st.R_diag.at(i) = 0;
        st.R_xdiag.at(i) = 0;
    }
    crsce::decompress::BlockSolveSnapshot snap{Csm::kS, st, {}, {}, {}, {}, 0ULL};
    const std::span<const std::uint8_t> empty_lh{};
    DeterministicElimination de{0ULL, csm, st, snap, empty_lh};
    // Hash-based elimination removed; DE performs only forced moves.
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

/**
 * @name DeterministicEliminationPatterns.AllOnesEliminatedByDE
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
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
    crsce::decompress::BlockSolveSnapshot snap2{Csm::kS, st, {}, {}, {}, {}, 0ULL};
    const std::span<const std::uint8_t> empty_lh2{};
    DeterministicElimination de{0ULL, csm, st, snap2, empty_lh2};
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

/**
 * @name DeterministicEliminationPatterns.AlternatingRowsEliminatedByHashThenDENoop
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(DeterministicEliminationPatterns, AlternatingRowsAlreadySolvedNoop) { // NOLINT
    Csm csm;
    ConstraintState st{};
    // Simulate hash-based DE solved rows: lock alternating patterns and set U=0, R=0 across all families.
    for (std::size_t r = 0; r < Csm::kS; ++r) {
        for (std::size_t c = 0; c < Csm::kS; ++c) {
            const bool bit = (r % 2U == 0U)
                                 ? ((c % 2U) != 0U) // 0101...
                                 : ((c % 2U) == 0U); // 1010...
            if (bit) { csm.set(r, c); } else { csm.clear(r, c); }
            csm.lock(r, c);
        }
    }
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = 0;
        st.R_row.at(i) = st.R_col.at(i) = st.R_diag.at(i) = st.R_xdiag.at(i) = 0;
    }
    crsce::decompress::BlockSolveSnapshot snap3{Csm::kS, st, {}, {}, {}, {}, 0ULL};
    const std::span<const std::uint8_t> empty_lh3{};
    DeterministicElimination de{0ULL, csm, st, snap3, empty_lh3};
    const auto progress = de.solve_step();
    EXPECT_EQ(progress, 0U);
    EXPECT_TRUE(de.solved());
}

/**
 * @name DeterministicEliminationPatterns.SingleRowForcedOnesOnly
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
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

    crsce::decompress::BlockSolveSnapshot snap4{Csm::kS, st, {}, {}, {}, {}, 0ULL};
    const std::span<const std::uint8_t> empty_lh4{};
    DeterministicElimination de{0ULL, csm, st, snap4, empty_lh4};
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
