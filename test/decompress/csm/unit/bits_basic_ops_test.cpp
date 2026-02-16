/**
 * @file bits_basic_ops_test.cpp
 */
#include <gtest/gtest.h>
#include "decompress/Csm/detail/Bits.h"

using crsce::decompress::Bits;

TEST(BitsTest, DataLockMuOps) {
    Bits b;
    EXPECT_FALSE(b.data());
    EXPECT_FALSE(b.resolved());
    EXPECT_FALSE(b.mu_locked());
    b.set_data(true);
    EXPECT_TRUE(b.data());
    b.set_resolved(true);
    EXPECT_TRUE(b.resolved());
    // try_lock_mu transitions from 0->1
    EXPECT_TRUE(b.try_lock_mu());
    EXPECT_TRUE(b.mu_locked());
    // second acquisition fails
    EXPECT_FALSE(b.try_lock_mu());
    b.unlock_mu();
    EXPECT_FALSE(b.mu_locked());
    b.flip_data();
    EXPECT_FALSE(b.data());
}
