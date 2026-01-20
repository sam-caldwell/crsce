/**
 * @file crosssum_diagonal_wraps_mod511_test.cpp
 * @brief Verify diagonal mapping d(r,c) wraps modulo 511.
 */
#include "common/CrossSum/CrossSum.h"
#include <gtest/gtest.h>

using crsce::common::CrossSum;

TEST(CrossSumTest, DiagonalMappingWrapsMod511) {
    CrossSum cs;
    cs.increment_diagonal(0, 0);
    EXPECT_EQ(cs.value(0), 1);

    cs.increment_diagonal(510, 1); // (510+1)%511 == 0
    EXPECT_EQ(cs.value(0), 2);

    cs.increment_diagonal(100, 300); // (100+300)%511 == 400
    EXPECT_EQ(cs.value(400), 1);
}
