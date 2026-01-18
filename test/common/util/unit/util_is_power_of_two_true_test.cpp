/**
 * @file: util_is_power_of_two_true_test.cpp
 */
#include <gtest/gtest.h>
#include "common/Util/IsPowerOfTwo.h"

using crsce::common::util::is_power_of_two;

TEST(UtilMathTest, IsPowerOfTwoTrue) {
  EXPECT_TRUE(is_power_of_two(1));
  EXPECT_TRUE(is_power_of_two(8));
}
