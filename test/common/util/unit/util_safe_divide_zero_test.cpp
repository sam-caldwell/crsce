/**
 * @file: util_safe_divide_zero_test.cpp
 */
#include "common/Util/SafeDivide.h"
#include <gtest/gtest.h>

using crsce::common::util::safe_divide;

TEST(UtilMathTest, SafeDivideZero) {
  auto q = safe_divide(10, 0);
  EXPECT_FALSE(q.has_value());
}
