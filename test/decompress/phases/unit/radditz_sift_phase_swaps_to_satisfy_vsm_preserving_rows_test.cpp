/**
 * @file radditz_sift_phase_swaps_to_satisfy_vsm_preserving_rows_test.cpp
 * @brief RadditzSift: swaps to satisfy VSM while preserving row sums and no locks.
 */
#include <gtest/gtest.h>
#include <cstddef>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Phases/RadditzSift/RadditzSift.h"

using crsce::decompress::Csm;
using crsce::decompress::ConstraintState;
using crsce::decompress::BlockSolveSnapshot;
using crsce::decompress::phases::radditz_sift_phase;

TEST(RadditzSiftPhaseBasic, SwapsToSatisfyVsmPreservingRows) { // NOLINT
    constexpr std::size_t S = Csm::kS;
    Csm csm; csm.reset();
    ConstraintState st{}; // unused by Radditz
    const ConstraintState st_pre{};
    BlockSolveSnapshot snap{S, st_pre, {}, {}, {}, {}, 0ULL};

    // Setup: two rows each with a single 1 in donor columns (will be moved)
    constexpr std::size_t r0 = 2;
    constexpr std::size_t r1 = 9;
    constexpr std::size_t cDon0 = 4;
    constexpr std::size_t cDon1 = 8;
    csm.set(r0, cDon0);
    csm.set(r1, cDon1);

    // Target VSM: place ones into two specific columns
    constexpr std::size_t cT0 = 7;
    constexpr std::size_t cT1 = 12;
    snap.vsm.assign(S, 0);
    snap.vsm.at(cT0) = 1;
    snap.vsm.at(cT1) = 1;

    // Run sifter (all columns)
    const std::size_t swaps = radditz_sift_phase(csm, st, snap, /*max_cols*/0);
    EXPECT_EQ(swaps, 2U);

    // Column counts should match VSM at targets
    auto col_count = [&](std::size_t c){
        std::size_t k = 0;
        for (std::size_t r = 0; r < S; ++r) {
            if (csm.get(r, c)) { ++k; }
        }
        return k;
    };
    EXPECT_EQ(col_count(cT0), 1U);
    EXPECT_EQ(col_count(cT1), 1U);
    EXPECT_EQ(col_count(cDon0), 0U);
    EXPECT_EQ(col_count(cDon1), 0U);

    // Row sums preserved and no locks placed
    auto row_count = [&](std::size_t r){
        std::size_t k = 0;
        for (std::size_t c = 0; c < S; ++c) {
            if (csm.get(r, c)) { ++k; }
        }
        return k;
    };
    EXPECT_EQ(row_count(r0), 1U);
    EXPECT_EQ(row_count(r1), 1U);
    for (std::size_t c=0;c<S;++c){ EXPECT_FALSE(csm.is_locked(r0, c)); EXPECT_FALSE(csm.is_locked(r1, c)); }

    // Snapshot instrumentation
    EXPECT_GE(snap.time_radditz_ms, 0U);
    EXPECT_EQ(snap.radditz_iterations, 1U);
    EXPECT_GE(snap.partial_adoptions, 2U);
    EXPECT_GE(snap.radditz_passes_last, 1U);
    EXPECT_EQ(snap.radditz_swaps_last, swaps);
    EXPECT_EQ(snap.radditz_cols_remaining, 0U);
    EXPECT_EQ(snap.radditz_deficit_abs_after, 0U);
}
