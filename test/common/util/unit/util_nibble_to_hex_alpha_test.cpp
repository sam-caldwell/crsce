/**
 * @file: util_nibble_to_hex_alpha_test.cpp
 */
#include <gtest/gtest.h>
#include "common/Util/NibbleToHex.h"

using crsce::common::util::nibble_to_hex;

TEST(UtilMathTest, NibbleToHexAlpha) {
  EXPECT_EQ(nibble_to_hex(10), 'A');
  EXPECT_EQ(nibble_to_hex(15), 'F');
}
