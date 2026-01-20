/**
 * @file crosssum_antidiagonal_wraps_mod511_test.cpp
 * @brief Verify anti-diagonal mapping x(r,c) wraps modulo 511.
 */
#include "common/CrossSum/CrossSum.h"
#include <gtest/gtest.h>

using crsce::common::CrossSum;

TEST(CrossSumTest, AntiDiagonalMappingWrapsMod511) {
    CrossSum cs;
    cs.increment_antidiagonal(0, 1); // x(0,1) == 510
    EXPECT_EQ(cs.value(510), 1);

    cs.increment_antidiagonal(500, 100); // 500-100 == 400
    EXPECT_EQ(cs.value(400), 1);

    cs.increment_antidiagonal(3, 0); // 3-0 == 3
    EXPECT_EQ(cs.value(3), 1);
}
