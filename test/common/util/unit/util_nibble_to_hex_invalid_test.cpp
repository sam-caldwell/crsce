/**
 * @file: util_nibble_to_hex_invalid_test.cpp
 */
#include "common/Util/NibbleToHex.h"
#include <gtest/gtest.h>

using crsce::common::util::nibble_to_hex;

/**

 * @name UtilMathTest.NibbleToHexInvalid

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(UtilMathTest, NibbleToHexInvalid) {
  EXPECT_EQ(nibble_to_hex(16), '?');
  EXPECT_EQ(nibble_to_hex(42), '?');
}
