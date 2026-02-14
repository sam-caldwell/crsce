/**
 * @file radditz_vsm_sift_reduces_deficit_preserves_rows_test.cpp
 */
#include <gtest/gtest.h>
#include <cstddef>
#include <vector>
#include <span>
#include <cstdint>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Phases/RadditzSift/RadditzSift.h"
#include "decompress/Phases/RadditzSift/compute_col_counts.h"
#include "decompress/Phases/RadditzSift/compute_deficits.h"
#include "decompress/Phases/RadditzSift/deficit_abs_sum.h"
#include "decompress/Phases/RadditzSift/verify_row_sums.h"

using crsce::decompress::Csm;
using crsce::decompress::ConstraintState;
using crsce::decompress::BlockSolveSnapshot;
using crsce::decompress::phases::radditz_sift_phase;
using crsce::decompress::phases::detail::compute_col_counts;
using crsce::decompress::phases::detail::compute_deficits;
using crsce::decompress::phases::detail::deficit_abs_sum;
using crsce::decompress::phases::detail::verify_row_sums;

TEST(RadditzVsm, SiftReducesDeficitPreservesRows) {
    Csm csm;
    constexpr std::size_t S = Csm::kS;
    const std::size_t cA = 5U;   // surplus column
    const std::size_t cB = 100U; // deficit column

    // Seed a simple state: 20 rows with a 1 at cA and 0 at cB
    for (std::size_t r = 0; r < 20; ++r) {
        csm.put(r, cA, true);
    }

    // Prepare snapshot with LSM (row sums) based on initial state
    BlockSolveSnapshot snap{};
    snap.lsm.assign(S, 0);
    for (std::size_t r = 0; r < 20; ++r) { snap.lsm[r] = 1; }

    // Build a VSM target that shifts 8 ones from cA to cB
    const auto col_before = compute_col_counts(csm);
    ASSERT_EQ(static_cast<std::size_t>(col_before[cA]), 20U);
    ASSERT_EQ(static_cast<std::size_t>(col_before[cB]), 0U);

    snap.vsm.assign(S, 0);
    snap.vsm[cA] = static_cast<std::uint16_t>(col_before[cA] - 8); // 12
    snap.vsm[cB] = static_cast<std::uint16_t>(col_before[cB] + 8); // 8

    const auto def_before = compute_deficits(col_before, std::span<const std::uint16_t>(snap.vsm.data(), snap.vsm.size()));
    const auto abs_before = deficit_abs_sum(def_before);
    ASSERT_GT(abs_before, 0); // we constructed an actual deficit

    ConstraintState st{}; // not used by simplified radditz_sift_phase
    // Run VSM-focused Radditz Sift (no column cap)
    (void)radditz_sift_phase(csm, st, snap, 0);

    // Recompute deficits after sifting
    const auto col_after = compute_col_counts(csm);
    const auto def_after = compute_deficits(col_after, std::span<const std::uint16_t>(snap.vsm.data(), snap.vsm.size()));
    const auto abs_after = deficit_abs_sum(def_after);

    // Expect improvement in total absolute deficit
    EXPECT_LT(abs_after, abs_before);
    // And row sums preserved
    EXPECT_TRUE(verify_row_sums(csm, std::span<const std::uint16_t>(snap.lsm.data(), snap.lsm.size())));
}

