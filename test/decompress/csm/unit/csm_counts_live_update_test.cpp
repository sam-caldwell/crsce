/**
 * @file csm_counts_live_update_test.cpp
 */
#include <gtest/gtest.h>
#include "decompress/Csm/Csm.h"
#include <cstddef>

using crsce::decompress::Csm;

TEST(CsmCounts, LiveUpdatesRcAndDx) {
    Csm csm;
    constexpr std::size_t S = Csm::kS;
    // Sanity: all zero
    for (std::size_t i = 0; i < S; ++i) {
        EXPECT_EQ(csm.count_lsm(i), 0);
        EXPECT_EQ(csm.count_vsm(i), 0);
        EXPECT_EQ(csm.count_dsm(i), 0);
        EXPECT_EQ(csm.count_xsm(i), 0);
    }
    // Write a few rc bits and verify counters and versions
    csm.set(0, 0);
    EXPECT_TRUE(csm.get(0,0));
    EXPECT_EQ(csm.count_lsm(0), 1);
    EXPECT_EQ(csm.count_vsm(0), 1);
    EXPECT_EQ(csm.count_dsm(Csm::calc_d(0,0)), 1);
    EXPECT_EQ(csm.count_xsm(Csm::calc_x(0,0)), 1);
    const auto v0 = csm.row_version(0);
    // No-op write should not bump
    csm.set(0, 0);
    EXPECT_EQ(csm.row_version(0), v0);
    // Flip back to 0
    csm.clear(0, 0);
    EXPECT_FALSE(csm.get(0,0));
    EXPECT_EQ(csm.count_lsm(0), 0);
    EXPECT_EQ(csm.count_vsm(0), 0);
    // Use dx to set (r,c) where d=1,x=2
    const std::size_t r = 5;
    const std::size_t c = 6;
    const std::size_t d = Csm::calc_d(r, c);
    const std::size_t x = Csm::calc_x(r, c);
    csm.set_dx(d, x);
    EXPECT_TRUE(csm.get(r,c));
    EXPECT_EQ(csm.count_lsm(r), 1);
    EXPECT_EQ(csm.count_vsm(c), 1);
    EXPECT_EQ(csm.count_dsm(d), 1);
    EXPECT_EQ(csm.count_xsm(x), 1);
}
