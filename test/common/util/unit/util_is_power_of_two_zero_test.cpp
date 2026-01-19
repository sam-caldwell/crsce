/**
 * @file: util_is_power_of_two_zero_test.cpp
 */
#include "common/Util/IsPowerOfTwo.h"
#include <gtest/gtest.h>

using crsce::common::util::is_power_of_two;

TEST(UtilMathTest, IsPowerOfTwoZero) { EXPECT_FALSE(is_power_of_two(0)); }
