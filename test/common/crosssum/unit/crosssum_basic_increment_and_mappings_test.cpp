/**
 * @file crosssum_basic_increment_and_mappings_test.cpp
 * @brief Unit tests for CrossSum basic increments and index mappings.
 */
#include "common/CrossSum/CrossSum.h"
#include <gtest/gtest.h>

using crsce::common::CrossSum;

TEST(CrossSumTest, DefaultZeroAndDirectIncrement) {
    CrossSum cs;
    // A few random indices are zero-initialized
    EXPECT_EQ(cs.value(0), 0);
    EXPECT_EQ(cs.value(123), 0);
    EXPECT_EQ(cs.value(CrossSum::kSize - 1), 0);

    // Increment direct index
    cs.increment(0);
    cs.increment(123);
    cs.increment(CrossSum::kSize - 1);

    EXPECT_EQ(cs.value(0), 1);
    EXPECT_EQ(cs.value(123), 1);
    EXPECT_EQ(cs.value(CrossSum::kSize - 1), 1);
}

TEST(CrossSumTest, DiagonalMappingWrapsMod511) {
    CrossSum cs;
    // d(0,0) = 0
    cs.increment_diagonal(0, 0);
    EXPECT_EQ(cs.value(0), 1);

    // d(510,1) = (511) % 511 = 0
    cs.increment_diagonal(510, 1);
    EXPECT_EQ(cs.value(0), 2);

    // d(100, 300) = 400
    cs.increment_diagonal(100, 300);
    EXPECT_EQ(cs.value(400), 1);
}

TEST(CrossSumTest, AntiDiagonalMappingWrapsMod511) {
    CrossSum cs;
    // x(0,1) = 510
    cs.increment_antidiagonal(0, 1);
    EXPECT_EQ(cs.value(510), 1);

    // x(500,100) = 400
    cs.increment_antidiagonal(500, 100);
    EXPECT_EQ(cs.value(400), 1);

    // x(3,0) = 3
    cs.increment_antidiagonal(3, 0);
    EXPECT_EQ(cs.value(3), 1);
}
