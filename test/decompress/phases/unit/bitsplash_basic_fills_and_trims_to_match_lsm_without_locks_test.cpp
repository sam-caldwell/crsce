/**
 * @file bitsplash_basic_fills_and_trims_to_match_lsm_without_locks_test.cpp
 * @brief BitSplash: rows match LSM; fills and trims without locks.
 */
#include <gtest/gtest.h>
#include <cstddef>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Phases/BitSplash/BitSplash.h"

using crsce::decompress::Csm;
using crsce::decompress::ConstraintState;
using crsce::decompress::BlockSolveSnapshot;
using crsce::decompress::phases::bit_splash;

TEST(BitSplashBasic, FillsAndTrimsToMatchLsmWithoutLocks) { // NOLINT
    constexpr std::size_t S = Csm::kS;
    Csm csm; csm.reset();
    ConstraintState st{}; (void)st; // unused by BitSplash
    BlockSolveSnapshot snap{S, st, {}, {}, {}, {}, 0ULL};
    snap.lsm.assign(S, 0);

    // Row r_add: needs exactly 2 ones; currently empty
    constexpr std::size_t r_add = 3;
    snap.lsm.at(r_add) = 2;

    // Row r_trim: currently has 3 ones (unlocked), but LSM=1
    constexpr std::size_t r_trim = 7;
    csm.set(r_trim, 1);
    csm.set(r_trim, 2);
    csm.set(r_trim, 3);
    snap.lsm.at(r_trim) = 1;

    const std::size_t flips = bit_splash(csm, st, snap, /*max_rows*/0);
    EXPECT_GE(flips, 0U);

    // Validate row r_add now has exactly 2 ones and none are locked
    std::size_t ones_add = 0; for (std::size_t c = 0; c < S; ++c) {
        if (csm.get(r_add, c)) { ++ones_add; EXPECT_FALSE(csm.is_locked(r_add, c)); }
    }
    EXPECT_EQ(ones_add, 2U);

    // Validate row r_trim now has exactly 1 one and none are locked
    std::size_t ones_trim = 0; for (std::size_t c = 0; c < S; ++c) {
        if (csm.get(r_trim, c)) { ++ones_trim; EXPECT_FALSE(csm.is_locked(r_trim, c)); }
    }
    EXPECT_EQ(ones_trim, 1U);

    // Snapshot instrumentation
    EXPECT_GE(snap.time_row_phase_ms, 0U);
    EXPECT_EQ(snap.row_phase_iterations, 1U);
    EXPECT_GE(snap.partial_adoptions, 1U);
}
