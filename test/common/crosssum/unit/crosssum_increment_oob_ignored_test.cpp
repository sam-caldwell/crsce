/**
 * @file crosssum_increment_oob_ignored_test.cpp
 * @brief Ensure CrossSum::increment ignores out-of-bounds indices.
 */
#include <gtest/gtest.h>
#include "common/CrossSum/CrossSum.h"

using crsce::common::CrossSum;

TEST(CrossSumTest, IncrementOutOfBoundsIsIgnored) { // NOLINT
    CrossSum cs;
    // Sanity: all zeros initially
    EXPECT_EQ(cs.value(0), 0);
    EXPECT_EQ(cs.value(CrossSum::kSize - 1), 0);

    // Out-of-bounds index should not change any values
    cs.increment(CrossSum::kSize); // i == 511 (invalid)
    EXPECT_EQ(cs.value(0), 0);
    EXPECT_EQ(cs.value(CrossSum::kSize - 1), 0);

    // In-bounds increment still works
    cs.increment(0);
    EXPECT_EQ(cs.value(0), 1);
}
