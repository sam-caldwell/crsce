/**
 * @file: util_clamp_low_test.cpp
 */
#include "common/Util/ClampInt.h"
#include <gtest/gtest.h>

using crsce::common::util::clamp_int;

TEST(UtilMathTest, ClampLow) { EXPECT_EQ(clamp_int(-5, 0, 10), 0); }
