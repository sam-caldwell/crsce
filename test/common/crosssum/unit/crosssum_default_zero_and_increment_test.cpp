/**
 * @file crosssum_default_zero_and_increment_test.cpp
 * @brief Ensure CrossSum starts at zero and direct increment works.
 */
#include "common/CrossSum/CrossSum.h"
#include <gtest/gtest.h>

using crsce::common::CrossSum;

/**
 * @name CrossSumTest.DefaultZeroAndDirectIncrement
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(CrossSumTest, DefaultZeroAndDirectIncrement) {
    CrossSum cs;
    EXPECT_EQ(cs.value(0), 0);
    EXPECT_EQ(cs.value(123), 0);
    EXPECT_EQ(cs.value(CrossSum::kSize - 1), 0);

    cs.increment(0);
    cs.increment(123);
    cs.increment(CrossSum::kSize - 1);

    EXPECT_EQ(cs.value(0), 1);
    EXPECT_EQ(cs.value(123), 1);
    EXPECT_EQ(cs.value(CrossSum::kSize - 1), 1);
}
