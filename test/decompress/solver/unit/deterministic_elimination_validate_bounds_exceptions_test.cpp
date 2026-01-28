/**
 * @file deterministic_elimination_validate_bounds_exceptions_test.cpp
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "common/exceptions/ConstraintBoundsInvalid.h"
#include "common/exceptions/ConstraintInvariantViolation.h"

using crsce::decompress::Csm;
using crsce::decompress::ConstraintState;
using crsce::decompress::DeterministicElimination;

TEST(DeterministicEliminationBounds, ThrowsOnUOutOfRange) { // NOLINT
    Csm csm;
    ConstraintState st{};
    // Initialize to zeros
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = 0;
        st.R_row.at(i) = st.R_col.at(i) = st.R_diag.at(i) = st.R_xdiag.at(i) = 0;
    }
    st.U_diag.at(7) = static_cast<std::uint16_t>(Csm::kS + 1);

    EXPECT_THROW((DeterministicElimination{csm, st}), crsce::decompress::ConstraintBoundsInvalid);
}

TEST(DeterministicEliminationBounds, ThrowsOnRGreaterThanU) { // NOLINT
    Csm csm;
    ConstraintState st{};
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        st.U_row.at(i) = st.U_col.at(i) = st.U_diag.at(i) = st.U_xdiag.at(i) = 0;
        st.R_row.at(i) = st.R_col.at(i) = st.R_diag.at(i) = st.R_xdiag.at(i) = 0;
    }
    st.U_col.at(5) = 10;
    st.R_col.at(5) = 11;

    EXPECT_THROW((DeterministicElimination{csm, st}), crsce::decompress::ConstraintInvariantViolation);
}
