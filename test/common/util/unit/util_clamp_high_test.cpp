/**
 * @file: util_clamp_high_test.cpp
 */
#include <gtest/gtest.h>
#include "common/Util/ClampInt.h"

using crsce::common::util::clamp_int;

TEST(UtilMathTest, ClampHigh) {
  EXPECT_EQ(clamp_int(15, 0, 10), 10);
}
