/**
 * @file: util_is_power_of_two_false_test.cpp
 */
#include "common/Util/IsPowerOfTwo.h"
#include <gtest/gtest.h>

using crsce::common::util::is_power_of_two;

/**

 * @name UtilMathTest.IsPowerOfTwoFalse

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(UtilMathTest, IsPowerOfTwoFalse) {
  EXPECT_FALSE(is_power_of_two(3));
  EXPECT_FALSE(is_power_of_two(12 + 1));
}
