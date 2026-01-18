/**
 * @file: util_safe_divide_zero_test.cpp
 */
#include <gtest/gtest.h>
#include "common/Util/SafeDivide.h"

using crsce::common::util::safe_divide;

TEST(UtilMathTest, SafeDivideZero) {
  auto q = safe_divide(10, 0);
  EXPECT_FALSE(q.has_value());
}
