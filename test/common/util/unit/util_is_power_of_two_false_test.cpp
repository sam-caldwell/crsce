/**
 * @file: util_is_power_of_two_false_test.cpp
 */
#include "common/Util/IsPowerOfTwo.h"
#include <gtest/gtest.h>

using crsce::common::util::is_power_of_two;

TEST(UtilMathTest, IsPowerOfTwoFalse) {
  EXPECT_FALSE(is_power_of_two(3));
  EXPECT_FALSE(is_power_of_two(12 + 1));
}
