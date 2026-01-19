/**
 * @file: util_nibble_to_hex_digits_test.cpp
 */
#include "common/Util/NibbleToHex.h"
#include <gtest/gtest.h>

using crsce::common::util::nibble_to_hex;

TEST(UtilMathTest, NibbleToHexDigits) {
  EXPECT_EQ(nibble_to_hex(0), '0');
  EXPECT_EQ(nibble_to_hex(9), '9');
}
