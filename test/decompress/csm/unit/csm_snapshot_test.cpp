/**
 * @file csm_snapshot_test.cpp
 */
#include <gtest/gtest.h>
#include "decompress/Csm/detail/Csm.h"

using crsce::decompress::Csm;

TEST(CsmSnapshot, CounterSnapshotConsistent) {
    Csm csm;
    // Set a small pattern
    csm.put(0,0,true);
    csm.put(1,2,true);
    csm.put(3,5,true);
    auto snap = csm.take_counter_snapshot();
    EXPECT_EQ(snap.lsm[0], csm.count_lsm(0));
    EXPECT_EQ(snap.lsm[1], csm.count_lsm(1));
    EXPECT_EQ(snap.lsm[3], csm.count_lsm(3));
    // spot check others remain zero
    EXPECT_EQ(snap.vsm[0], csm.count_vsm(0));
    EXPECT_EQ(snap.vsm[2], csm.count_vsm(2));
}

