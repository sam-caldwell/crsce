/**
 * @file csm_counters_and_versions_test.cpp
 */
#include <gtest/gtest.h>
#include <cstddef>
#include "decompress/Csm/detail/Csm.h"

using crsce::decompress::Csm;

TEST(CsmCounters, LiveCountersAndRowVersionsRcDx) {
    Csm csm;
    constexpr std::size_t S = Csm::kS;

    // rc write and check counters + version
    const std::size_t r = 3;
    const std::size_t c = 7;
    const auto ver0 = csm.row_version(r);
    csm.put(r, c, true);
    EXPECT_EQ(csm.count_lsm(r), 1);
    EXPECT_EQ(csm.count_vsm(c), 1);
    EXPECT_EQ(csm.count_dsm(Csm::calc_d(r, c)), 1);
    EXPECT_EQ(csm.count_xsm(Csm::calc_x(r, c)), 1);
    EXPECT_GT(csm.row_version(r), ver0);
    // no-op write does not bump version
    const auto ver1 = csm.row_version(r);
    csm.put(r, c, true);
    EXPECT_EQ(csm.row_version(r), ver1);

    // dx write toggling back to 0
    const std::size_t d = Csm::calc_d(r, c);
    const std::size_t x = Csm::calc_x(r, c);
    csm.put_dx(d, x, false);
    EXPECT_EQ(csm.count_lsm(r), 0);
    EXPECT_EQ(csm.count_vsm(c), 0);
    EXPECT_EQ(csm.count_dsm(d), 0);
    EXPECT_EQ(csm.count_xsm(x), 0);
}

TEST(CsmSnapshot, CounterSnapshotConsistentWithCounters) {
    Csm csm;
    // simple pattern
    csm.put(0, 0, true);
    csm.put(1, 2, true);
    csm.put(3, 5, true);
    auto snap = csm.take_counter_snapshot();
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        EXPECT_EQ(snap.lsm.at(i), csm.count_lsm(i));
        EXPECT_EQ(snap.vsm.at(i), csm.count_vsm(i));
        EXPECT_EQ(snap.dsm.at(i), csm.count_dsm(i));
        EXPECT_EQ(snap.xsm.at(i), csm.count_xsm(i));
    }
}
