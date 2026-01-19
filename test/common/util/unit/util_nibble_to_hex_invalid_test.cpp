/**
 * @file: util_nibble_to_hex_invalid_test.cpp
 */
#include "common/Util/NibbleToHex.h"
#include <gtest/gtest.h>

using crsce::common::util::nibble_to_hex;

TEST(UtilMathTest, NibbleToHexInvalid) {
  EXPECT_EQ(nibble_to_hex(16), '?');
  EXPECT_EQ(nibble_to_hex(42), '?');
}
