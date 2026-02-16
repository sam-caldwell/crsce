/**
 * @file bitsplash_basic_throws_if_locked_ones_exceed_lsm_test.cpp
 * @brief BitSplash: throws if locked ones exceed row’s LSM value.
 */
#include <gtest/gtest.h>
#include <cstddef>
#include <stdexcept>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Phases/BitSplash/BitSplash.h"

using crsce::decompress::Csm;
using crsce::decompress::ConstraintState;
using crsce::decompress::BlockSolveSnapshot;
using crsce::decompress::phases::bit_splash;

TEST(BitSplashBasic, ThrowsIfLockedOnesExceedLsm) { // NOLINT
    Csm csm; csm.reset();
    ConstraintState st{}; (void)st;
    constexpr std::size_t S = Csm::kS;
    BlockSolveSnapshot snap{S, st, {}, {}, {}, {}, 0ULL};
    snap.lsm.assign(S, 0);

    // Row r: lock a 1 but set LSM=0
    const std::size_t r = 11;
    const std::size_t c = 13;
    csm.set(r, c);
    csm.lock(r, c);
    snap.lsm.at(r) = 0;

    EXPECT_THROW((void)bit_splash(csm, st, snap, 0), std::runtime_error);
}
