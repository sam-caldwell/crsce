/**
 * @file radditz_sift_phase_swap_lateral_respects_locks_and_state_test.cpp
 * @brief RadditzSift helpers: swap_lateral respects locks and CSM state.
 */
#include <gtest/gtest.h>
#include <cstddef>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/RadditzSift/swap_lateral.h"

using crsce::decompress::Csm;
using crsce::decompress::phases::detail::swap_lateral;

TEST(RadditzSiftPhaseBasic, SwapLateralRespectsLocksAndState) { // NOLINT
    Csm csm; csm.reset();
    const std::size_t r = 4;
    const std::size_t cf = 3;
    const std::size_t ct = 9;
    // invalid because from is 0
    EXPECT_FALSE(swap_lateral(csm, r, cf, ct));
    csm.set(r, cf);
    // invalid because to already 1
    csm.set(r, ct);
    EXPECT_FALSE(swap_lateral(csm, r, cf, ct));
    // unlock to 0 and lock donor
    csm.clear(r, ct);
    csm.lock(r, cf);
    EXPECT_FALSE(swap_lateral(csm, r, cf, ct));
}
