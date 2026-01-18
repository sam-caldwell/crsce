/**
 * @file: util_clamp_low_test.cpp
 */
#include <gtest/gtest.h>
#include "common/Util/ClampInt.h"

using crsce::common::util::clamp_int;

TEST(UtilMathTest, ClampLow) {
  EXPECT_EQ(clamp_int(-5, 0, 10), 0);
}
