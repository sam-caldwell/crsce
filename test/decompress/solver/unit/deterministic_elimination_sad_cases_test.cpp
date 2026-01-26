/**
 * @file deterministic_elimination_sad_cases_test.cpp
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>
#include <cstdint>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"

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
    EXPECT_THROW((DeterministicElimination{csm, st}), std::invalid_argument);
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

    DeterministicElimination de{csm, st};
    EXPECT_THROW((void)de.solve_step(), std::runtime_error);
}
