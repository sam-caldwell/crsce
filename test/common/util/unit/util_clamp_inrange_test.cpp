/**
 * @file: util_clamp_inrange_test.cpp
 */
#include <gtest/gtest.h>
#include "common/Util/ClampInt.h"

using crsce::common::util::clamp_int;

TEST(UtilMathTest, ClampInRange) {
  EXPECT_EQ(clamp_int(7, 0, 10), 7);
}
