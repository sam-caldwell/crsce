/**
 * @file crosssum_antidiagonal_wraps_mod511_test.cpp
 * @brief Verify XSM mapping x(r,c) = (r + c) mod 511 wraps correctly.
 */
#include "common/CrossSum/CrossSum.h"
#include <gtest/gtest.h>

using crsce::common::CrossSum;

/**
 * @name CrossSumTest.AntiDiagonalMappingWrapsMod511
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(CrossSumTest, AntiDiagonalMappingWrapsMod511) {
    CrossSum cs;
    cs.increment_antidiagonal(0, 1); // (0+1) % 511 == 1
    EXPECT_EQ(cs.value(1), 1);

    cs.increment_antidiagonal(500, 100); // (500+100) % 511 == 89
    EXPECT_EQ(cs.value(89), 1);

    cs.increment_antidiagonal(3, 0); // (3+0) % 511 == 3
    EXPECT_EQ(cs.value(3), 1);
}
