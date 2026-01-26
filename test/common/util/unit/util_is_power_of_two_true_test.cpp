/**
 * @file: util_is_power_of_two_true_test.cpp
 */
#include "common/Util/detail/is_power_of_two.h"
#include <gtest/gtest.h>

using crsce::common::util::is_power_of_two;

/**

 * @name UtilMathTest.IsPowerOfTwoTrue

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(UtilMathTest, IsPowerOfTwoTrue) {
  EXPECT_TRUE(is_power_of_two(1));
  EXPECT_TRUE(is_power_of_two(8));
}
