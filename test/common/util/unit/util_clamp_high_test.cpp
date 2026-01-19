/**
 * @file: util_clamp_high_test.cpp
 */
#include "common/Util/ClampInt.h"
#include <gtest/gtest.h>

using crsce::common::util::clamp_int;

TEST(UtilMathTest, ClampHigh) { EXPECT_EQ(clamp_int(15, 0, 10), 10); }
