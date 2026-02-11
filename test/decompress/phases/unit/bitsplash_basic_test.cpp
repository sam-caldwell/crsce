/**
 * @file bitsplash_basic_test.cpp
 * @brief Unit tests for BitSplash (rows-only; no locks; satisfies LSM per row).
 */
#include <gtest/gtest.h>
#include <cstddef>
#include <vector>
#include <stdexcept>
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Phases/BitSplash.h"

using crsce::decompress::Csm;
using crsce::decompress::ConstraintState;
using crsce::decompress::BlockSolveSnapshot;
using crsce::decompress::phases::bit_splash;

TEST(BitSplashBasic, FillsAndTrimsToMatchLsmWithoutLocks) { // NOLINT
    constexpr std::size_t S = Csm::kS;
    Csm csm; csm.reset();
    ConstraintState st{}; (void)st; // unused by BitSplash
    BlockSolveSnapshot snap{};
    snap.lsm.assign(S, 0);

    // Row r_add: needs exactly 2 ones; currently empty
    const std::size_t r_add = 3;
    snap.lsm.at(r_add) = 2;

    // Row r_trim: currently has 3 ones (unlocked), but LSM=1
    const std::size_t r_trim = 7;
    csm.put(r_trim, 1, true);
    csm.put(r_trim, 2, true);
    csm.put(r_trim, 3, true);
    snap.lsm.at(r_trim) = 1;

    const std::size_t flips = bit_splash(csm, st, snap, /*max_rows*/0);
    EXPECT_GE(flips, 0U);

    // Validate row r_add now has exactly 2 ones and none are locked
    std::size_t ones_add = 0; for (std::size_t c = 0; c < S; ++c) { if (csm.get(r_add, c)) { ++ones_add; EXPECT_FALSE(csm.is_locked(r_add, c)); } }
    EXPECT_EQ(ones_add, 2U);

    // Validate row r_trim now has exactly 1 one and none are locked
    std::size_t ones_trim = 0; for (std::size_t c = 0; c < S; ++c) { if (csm.get(r_trim, c)) { ++ones_trim; EXPECT_FALSE(csm.is_locked(r_trim, c)); } }
    EXPECT_EQ(ones_trim, 1U);

    // Snapshot instrumentation
    EXPECT_GE(snap.time_row_phase_ms, 0U);
    EXPECT_EQ(snap.row_phase_iterations, 1U);
    EXPECT_GE(snap.partial_adoptions, 1U);
}

TEST(BitSplashBasic, ThrowsIfLockedOnesExceedLsm) { // NOLINT
    constexpr std::size_t S = Csm::kS;
    Csm csm; csm.reset();
    ConstraintState st{}; (void)st;
    BlockSolveSnapshot snap{};
    snap.lsm.assign(S, 0);

    // Row r: lock a 1 but set LSM=0
    const std::size_t r = 11;
    const std::size_t c = 13;
    csm.put(r, c, true);
    csm.lock(r, c);
    snap.lsm.at(r) = 0;

    EXPECT_THROW((void)bit_splash(csm, st, snap, 0), std::runtime_error);
}

TEST(BitSplashBasic, RespectsMaxRowsLimitAndNoOpsWhenAlreadyMatching) { // NOLINT
    constexpr std::size_t S = Csm::kS;
    Csm csm; csm.reset();
    ConstraintState st{}; (void)st;
    BlockSolveSnapshot snap{};
    snap.lsm.assign(S, 0);

    // Row r0 already matches LSM=1 because we pre-place one 1
    const std::size_t r0 = 2;
    csm.put(r0, 0, true);
    snap.lsm.at(r0) = 1;

    // Row r1 needs 2 ones, but we limit max_rows=1 so it won't be processed
    const std::size_t r1 = 3;
    snap.lsm.at(r1) = 2;

    const std::size_t flips = bit_splash(csm, st, snap, /*max_rows*/1);
    // No change required for r0, and r1 not processed
    EXPECT_EQ(flips, 0U);
    std::size_t ones0 = 0; for (std::size_t c = 0; c < S; ++c) { if (csm.get(r0, c)) { ++ones0; } }
    EXPECT_EQ(ones0, 1U);
    std::size_t ones1 = 0; for (std::size_t c = 0; c < S; ++c) { if (csm.get(r1, c)) { ++ones1; } }
    EXPECT_EQ(ones1, 0U);
}
