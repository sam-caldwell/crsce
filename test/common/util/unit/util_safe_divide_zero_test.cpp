/**
 * @file: util_safe_divide_zero_test.cpp
 */
#include "common/Util/SafeDivide.h"
#include <gtest/gtest.h>

using crsce::common::util::safe_divide;

/**

 * @name UtilMathTest.SafeDivideZero

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(UtilMathTest, SafeDivideZero) {
  auto q = safe_divide(10, 0);
  EXPECT_FALSE(q.has_value());
}
