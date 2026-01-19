/**
 * @file: util_safe_divide_ok_test.cpp
 */
#include "common/Util/SafeDivide.h"
#include <gtest/gtest.h>

using crsce::common::util::safe_divide;

TEST(UtilMathTest, SafeDivideOk) {
  auto q = safe_divide(9, 3);
  ASSERT_TRUE(q.has_value());
  EXPECT_EQ(*q, 3);
}
