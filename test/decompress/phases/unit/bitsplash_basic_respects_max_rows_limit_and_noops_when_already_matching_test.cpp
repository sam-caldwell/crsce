/**
 * @file bitsplash_basic_respects_max_rows_limit_and_noops_when_already_matching_test.cpp
 * @brief BitSplash: respects max_rows and no-ops when a row already matches LSM.
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

TEST(BitSplashBasic, RespectsMaxRowsLimitAndNoOpsWhenAlreadyMatching) { // NOLINT
    constexpr std::size_t S = Csm::kS;
    Csm csm; csm.reset();
    ConstraintState st{}; (void)st;
    BlockSolveSnapshot snap{S, st, {}, {}, {}, {}, 0ULL};
    snap.lsm.assign(S, 0);

    // Row r0 already matches LSM=1 because we pre-place one 1
    constexpr std::size_t r0 = 2;
    csm.set(r0, 0);
    snap.lsm.at(r0) = 1;

    // Row r1 needs 2 ones, but we limit max_rows=1 so it won't be processed
    const std::size_t r1 = 3;
    snap.lsm.at(r1) = 2;

    const std::size_t flips = bit_splash(csm, st, snap, /*max_rows*/1);
    // No change required for r0, and r1 not processed
    EXPECT_EQ(flips, 0U);
    std::size_t ones0 = 0;
    for (std::size_t c = 0; c < S; ++c) {
        if (csm.get(r0, c)) { ++ones0; }
    }
    EXPECT_EQ(ones0, 1U);
    std::size_t ones1 = 0;
    for (std::size_t c = 0; c < S; ++c) {
        if (csm.get(r1, c)) { ++ones1; }
    }
    EXPECT_EQ(ones1, 0U);
}
