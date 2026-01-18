/**
 * @file: util_nibble_to_hex_digits_test.cpp
 */
#include <gtest/gtest.h>
#include "common/Util/NibbleToHex.h"

using crsce::common::util::nibble_to_hex;

TEST(UtilMathTest, NibbleToHexDigits) {
  EXPECT_EQ(nibble_to_hex(0), '0');
  EXPECT_EQ(nibble_to_hex(9), '9');
}
