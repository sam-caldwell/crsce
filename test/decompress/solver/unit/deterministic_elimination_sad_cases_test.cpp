/**
 * @file deterministic_elimination_sad_cases_test.cpp
 */
#include <gtest/gtest.h>

#include <cstddef>
#include "common/exceptions/ConstraintInvariantViolation.h"
#include "common/exceptions/DeterministicEliminationError.h"
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
 * @name DeterministicEliminationSad.ConstructorRejectsRGreaterThanU
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(DeterministicEliminationSad, ConstructorRejectsRGreaterThanU) { // NOLINT
    Csm csm;
    ConstraintState st{};
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = 0;
        st.R_row.at(i) = st.R_col.at(i) = st.R_diag.at(i) = st.R_xdiag.at(i) = 0;
    }
    st.U_row.at(2) = 10;
    st.R_row.at(2) = 11; // invalid
    crsce::decompress::BlockSolveSnapshot snap{Csm::kS, st, {}, {}, {}, {}, 0ULL};
    const std::span<const std::uint8_t> empty_lh{};
    EXPECT_THROW((DeterministicElimination{0ULL, csm, st, snap, empty_lh}), crsce::decompress::ConstraintInvariantViolation);
}

/**
 * @name DeterministicEliminationSad.ThrowsWhenULineZeroButCellUnlocked
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(DeterministicEliminationSad, ThrowsWhenULineZeroButCellUnlocked) { // NOLINT
    Csm csm;
    ConstraintState st{};
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS);
        st.R_row.at(i) = st.R_col.at(i) = st.R_diag.at(i) = st.R_xdiag.at(i) = static_cast<std::uint16_t>(Csm::kS - 1);
    }
    // Force row 0 to ones
    st.R_row.at(0) = static_cast<std::uint16_t>(Csm::kS);
    // Inconsistent: Column 5 claims no unassigned vars
    st.U_col.at(5) = 0;
    st.R_col.at(5) = 0;

    crsce::decompress::BlockSolveSnapshot snap2{Csm::kS, st, {}, {}, {}, {}, 0ULL};
    const std::span<const std::uint8_t> empty_lh2{};
    DeterministicElimination de{0ULL, csm, st, snap2, empty_lh2};
    EXPECT_THROW((void)de.solve_step(), crsce::decompress::DeterministicEliminationError);
}
