/**
 * @file deterministic_elimination_validate_bounds_exceptions_test.cpp
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include <span>
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

    crsce::decompress::BlockSolveSnapshot snap{Csm::kS, st, {}, {}, {}, {}, 0ULL};
    const std::span<const std::uint8_t> empty_lh{};
    EXPECT_THROW((DeterministicElimination{0ULL, csm, st, snap, empty_lh}), crsce::decompress::ConstraintBoundsInvalid);
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

    crsce::decompress::BlockSolveSnapshot snap2{Csm::kS, st, {}, {}, {}, {}, 0ULL};
    const std::span<const std::uint8_t> empty_lh2{};
    EXPECT_THROW((DeterministicElimination{0ULL, csm, st, snap2, empty_lh2}), crsce::decompress::ConstraintInvariantViolation);
}
