/**
 * @file bitsplash_must_complete_integration_test.cpp
 * @brief High-level integration test: BitSplash completes and satisfies row LSM quickly.
 */
#include <gtest/gtest.h>
#include <cstddef>
#include <vector>
#include <cstdint>
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Phases/BitSplash/BitSplash.h"

using crsce::decompress::Csm;
using crsce::decompress::ConstraintState;
using crsce::decompress::BlockSolveSnapshot;
using crsce::decompress::phases::bit_splash;

TEST(BitSplashIntegration, MustCompleteAndMatchLsm) { // NOLINT
    Csm csm; csm.reset();
    ConstraintState st{}; // unused by BitSplash, signature consistency only
    BlockSolveSnapshot snap{};

    // Prepare LSM with small diverse counts to avoid pathological toggles.
    constexpr std::size_t S = Csm::kS;
    snap.lsm.assign(S, 0);
    for (std::size_t r = 0; r < S; ++r) {
        // Cycle small targets in [0,7] to keep work bounded
        snap.lsm[r] = static_cast<std::uint16_t>(r % 8U);
    }

    // Run BitSplash; should finish well under one second on a 511x511 board.
    const std::size_t flips = bit_splash(csm, st, snap, /*max_rows=*/0);
    (void)flips;

    // Verify each row’s 1-count equals its LSM target
    for (std::size_t r = 0; r < S; ++r) {
        std::size_t ones = 0;
        for (std::size_t c = 0; c < S; ++c) {
            if (csm.get(r, c)) { ++ones; }
        }
        ASSERT_EQ(ones, static_cast<std::size_t>(snap.lsm[r])) << "row " << r;
    }
}
