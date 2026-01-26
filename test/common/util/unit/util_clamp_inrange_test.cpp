/**
 * @file: util_clamp_inrange_test.cpp
 */
#include "common/Util/detail/clamp_int.h"
#include <gtest/gtest.h>

using crsce::common::util::clamp_int;

/**

 * @name UtilMathTest.ClampInRange

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(UtilMathTest, ClampInRange) { EXPECT_EQ(clamp_int(7, 0, 10), 7); }
