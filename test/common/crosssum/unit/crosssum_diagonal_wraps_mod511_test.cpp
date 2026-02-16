/**
 * @file crosssum_diagonal_wraps_mod511_test.cpp
 * @brief Verify DSM mapping d(r,c) = (c - r) mod 511 wraps correctly.
 */
#include "compress/CrossSum/CrossSum.h"
#include <gtest/gtest.h>

using crsce::common::CrossSum;

/**
 * @name CrossSumTest.DiagonalMappingWrapsMod511
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(CrossSumTest, DiagonalMappingWrapsMod511) {
    CrossSum cs;
    cs.increment_diagonal(0, 0);
    EXPECT_EQ(cs.value(0), 1);

    cs.increment_diagonal(510, 1); // (1 - 510) mod 511 == 2
    EXPECT_EQ(cs.value(2), 1);

    cs.increment_diagonal(100, 300); // (300 - 100) mod 511 == 200
    EXPECT_EQ(cs.value(200), 1);
}
