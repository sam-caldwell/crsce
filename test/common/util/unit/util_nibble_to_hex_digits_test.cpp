/**
 * @file: util_nibble_to_hex_digits_test.cpp
 */
#include "common/Util/detail/nibble_to_hex.h"
#include <gtest/gtest.h>

using crsce::common::util::nibble_to_hex;

/**

 * @name UtilMathTest.NibbleToHexDigits

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(UtilMathTest, NibbleToHexDigits) {
  EXPECT_EQ(nibble_to_hex(0), '0');
  EXPECT_EQ(nibble_to_hex(9), '9');
}
