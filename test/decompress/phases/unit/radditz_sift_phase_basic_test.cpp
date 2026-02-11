/**
 * @file radditz_sift_phase_basic_test.cpp
 * @brief Unit tests for RadditzSiftPhase swap-based sifting (preserves rows; satisfies VSM; no locks).
 */
#include <gtest/gtest.h>
#include <cstddef>
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Phases/RadditzSiftPhase.h"
#include "decompress/Phases/detail/RadditzSiftImpl.h"
#include <vector>
#include <cstdint>
#include <span>

using crsce::decompress::Csm;
using crsce::decompress::ConstraintState;
using crsce::decompress::BlockSolveSnapshot;
using crsce::decompress::phases::radditz_sift_phase;
using crsce::decompress::phases::detail::compute_col_counts;
using crsce::decompress::phases::detail::compute_deficits;
using crsce::decompress::phases::detail::deficit_abs_sum;
using crsce::decompress::phases::detail::collect_row_candidates;
using crsce::decompress::phases::detail::swap_lateral;
using crsce::decompress::phases::detail::all_deficits_zero;
using crsce::decompress::phases::detail::verify_row_sums;

TEST(RadditzSiftPhaseBasic, SwapsToSatisfyVsmPreservingRows) { // NOLINT
    constexpr std::size_t S = Csm::kS;
    Csm csm; csm.reset();
    ConstraintState st{}; // unused by Radditz
    BlockSolveSnapshot snap{};

    // Setup: two rows each with a single 1 in donor columns (will be moved)
    const std::size_t r0 = 2;
    const std::size_t r1 = 9;
    const std::size_t cDon0 = 4;
    const std::size_t cDon1 = 8;
    csm.put(r0, cDon0, true);
    csm.put(r1, cDon1, true);

    // Target VSM: place ones into two specific columns
    const std::size_t cT0 = 7;
    const std::size_t cT1 = 12;
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
            if (csm.get(r, c)) {
                ++k;
            }
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
            if (csm.get(r, c)) {
                ++k;
            }
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

TEST(RadditzSiftPhaseBasic, TelemetryOnUnresolvableDeficits) { // NOLINT
    constexpr std::size_t S = Csm::kS;
    Csm csm; csm.reset();
    ConstraintState st{};
    BlockSolveSnapshot snap{};

    // Lock two ones in donor columns that vsm sets to zero; target columns need ones
    const std::size_t r = 5;
    const std::size_t cDonA = 10;
    const std::size_t cDonB = 20;
    const std::size_t cNeedA = 30;
    const std::size_t cNeedB = 40;
    csm.put(r, cDonA, true); csm.lock(r, cDonA);
    csm.put(r, cDonB, true); csm.lock(r, cDonB);
    // No other ones anywhere

    snap.vsm.assign(S, 0);
    snap.vsm.at(cDonA) = 0;
    snap.vsm.at(cDonB) = 0;
    snap.vsm.at(cNeedA) = 1;
    snap.vsm.at(cNeedB) = 1;

    const std::size_t swaps = radditz_sift_phase(csm, st, snap, 0);
    (void)swaps;

    // Expect unresolved deficits due to locked donors preventing movement
    EXPECT_GT(snap.radditz_cols_remaining, 0U);
    EXPECT_GT(snap.radditz_deficit_abs_after, 0U);
    EXPECT_GE(snap.radditz_passes_last, 1U);
}

TEST(RadditzSiftPhaseBasic, DoesNotRepeatSamePairWithinRow) { // NOLINT
    // Repro a case where a single row can donate only once to a target column
    // whose deficit is > 1. The sifter must not keep reusing the same (from,to)
    // pair on that row, which would falsely drive the running deficit to zero
    // without additional changes to the CSM.
    constexpr std::size_t S = Csm::kS;
    Csm csm; csm.reset();
    ConstraintState st{};
    BlockSolveSnapshot snap{};

    const std::size_t r = 3;
    const std::size_t cFrom = 5;   // surplus column
    const std::size_t cTo   = 11;  // target column needs 3 ones total
    csm.put(r, cFrom, true);
    // No other bits are set anywhere

    snap.vsm.assign(S, 0);
    snap.vsm.at(cFrom) = 0; // this column should end up with 0 ones
    snap.vsm.at(cTo)   = 3; // requires three ones overall

    const std::size_t swaps = radditz_sift_phase(csm, st, snap, 0);
    // Only one real swap is possible from this row
    EXPECT_EQ(swaps, 1U);

    auto col_count = [&](std::size_t c){
        std::size_t k = 0;
        for (std::size_t rr = 0; rr < S; ++rr) { if (csm.get(rr, c)) { ++k; } }
        return k;
    };
    EXPECT_EQ(col_count(cFrom), 0U);
    EXPECT_EQ(col_count(cTo), 1U);

    // Deficits remain unresolved (need 2 more in cTo), and telemetry reflects this
    EXPECT_GT(snap.radditz_cols_remaining, 0U);
    EXPECT_GE(snap.radditz_deficit_abs_after, 2U);
}

TEST(RadditzSiftPhaseBasic, HelpersWorkAndEarlyExitWhenNoDeficits) { // NOLINT
    constexpr std::size_t S = Csm::kS;
    Csm csm; csm.reset();
    // Place some bits in a single column to produce nonzero col_count initially
    csm.put(0, 5, true);
    csm.put(1, 5, true);
    auto cols = compute_col_counts(csm);
    ASSERT_EQ(cols[5], 2);
    std::vector<std::uint16_t> vsm(S, 0);
    vsm[5] = 2; // match exactly
    auto def = compute_deficits(cols, std::span<const std::uint16_t>(vsm.data(), vsm.size()));
    EXPECT_TRUE(all_deficits_zero(def));
    EXPECT_EQ(deficit_abs_sum(def), 0U);
    // Candidate collection should be empty for rows that cannot help
    std::vector<std::size_t> from;
    std::vector<std::size_t> to;
    collect_row_candidates(csm, def, 0, from, to);
    EXPECT_TRUE(from.empty());
    EXPECT_TRUE(to.empty());
}

TEST(RadditzSiftPhaseBasic, VerifyRowSumsDetectsMismatch) { // NOLINT
    constexpr std::size_t S = Csm::kS;
    Csm csm; csm.reset();
    // csm has a single 1 in row 2
    csm.put(2, 10, true);
    std::vector<std::uint16_t> lsm(S, 0);
    lsm[2] = 0; // mismatch on purpose
    EXPECT_FALSE(verify_row_sums(csm, std::span<const std::uint16_t>(lsm.data(), lsm.size())));
}

TEST(RadditzSiftPhaseBasic, SwapLateralRespectsLocksAndState) { // NOLINT
    Csm csm; csm.reset();
    const std::size_t r = 4;
    const std::size_t cf = 3;
    const std::size_t ct = 9;
    // invalid because from is 0
    EXPECT_FALSE(swap_lateral(csm, r, cf, ct));
    csm.put(r, cf, true);
    // invalid because to already 1
    csm.put(r, ct, true);
    EXPECT_FALSE(swap_lateral(csm, r, cf, ct));
    // unlock to 0 and lock donor
    csm.put(r, ct, false);
    csm.lock(r, cf);
    EXPECT_FALSE(swap_lateral(csm, r, cf, ct));
}
