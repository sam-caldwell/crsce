/**
 * @file: util_clamp_inrange_test.cpp
 */
#include "common/Util/ClampInt.h"
#include <gtest/gtest.h>

using crsce::common::util::clamp_int;

TEST(UtilMathTest, ClampInRange) { EXPECT_EQ(clamp_int(7, 0, 10), 7); }
