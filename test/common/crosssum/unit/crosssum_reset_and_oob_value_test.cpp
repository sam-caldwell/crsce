/**
 * @file crosssum_reset_and_oob_value_test.cpp
 * @brief Test CrossSum::reset and out-of-bounds value() behavior.
 */
#include "common/CrossSum/CrossSum.h"
#include <gtest/gtest.h>
#include <cstddef>

using crsce::common::CrossSum;

/**

 * @name CrossSumTest.ResetClearsAllAndOobReturnsZero

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(CrossSumTest, ResetClearsAllAndOobReturnsZero) {
    CrossSum cs;
    cs.increment(0);
    cs.increment(510);
    cs.increment_diagonal(100, 300);
    cs.increment_antidiagonal(0, 1);

    // Ensure some non-zero state
    EXPECT_GT(cs.value(0), 0);
    EXPECT_GT(cs.value(510), 0);

    cs.reset();
    for (std::size_t i = 0; i < CrossSum::kSize; ++i) {
        EXPECT_EQ(cs.value(i), 0) << "index=" << i;
    }
    // Out-of-bounds must return 0 (no throw)
    EXPECT_EQ(cs.value(9999), 0);
}
