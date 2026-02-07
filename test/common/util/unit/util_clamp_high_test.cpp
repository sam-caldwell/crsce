/**
 * @file: util_clamp_high_test.cpp
 * @brief Unit test for clamp_high.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "common/Util/detail/clamp_int.h"
#include <gtest/gtest.h>

using crsce::common::util::clamp_int;

/**
 * @name UtilMathTest.ClampHigh
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(UtilMathTest, ClampHigh) { EXPECT_EQ(clamp_int(15, 0, 10), 10); }
