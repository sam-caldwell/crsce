/**
 * @file csm_rc_dx_equivalence_test.cpp
 */
#include <gtest/gtest.h>
#include <cstddef>
#include "decompress/Csm/Csm.h"

using crsce::decompress::Csm;

TEST(CsmEquivalence, RcDxCoordinateEquivalence) {
    Csm csm;
    constexpr std::size_t S = Csm::kS;

    // Paint a small pattern via rc
    for (std::size_t r = 0; r < 8; ++r) {
        for (std::size_t c = 0; c < 8; ++c) {
            if (((r + c) & 1U) == 0U) { csm.set(r, c); }
        }
    }
    // Verify dx get sees the same bits
    for (std::size_t r = 0; r < 8; ++r) {
        for (std::size_t c = 0; c < 8; ++c) {
            const std::size_t d = Csm::calc_d(r, c);
            const std::size_t x = Csm::calc_x(r, c);
            EXPECT_EQ(csm.get(r, c), csm.get_dx(d, x));
        }
    }
    // Flip a few via dx and verify rc sees them
    for (std::size_t r = 0; r < 8; ++r) {
        for (std::size_t c = 0; c < 8; ++c) {
            const std::size_t d = Csm::calc_d(r, c);
            const std::size_t x = Csm::calc_x(r, c);
            const bool v = ((r * 3 + c * 7) % 5) == 0;
            if (v) { csm.set_dx(d, x); } else { csm.clear_dx(d, x); }
            EXPECT_EQ(csm.get(r, c), v);
        }
    }
}
