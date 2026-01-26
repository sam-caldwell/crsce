/**
 * @file: util_nibble_to_hex_alpha_test.cpp
 */
#include "common/Util/detail/nibble_to_hex.h"
#include <gtest/gtest.h>

using crsce::common::util::nibble_to_hex;

/**
 * @name UtilMathTest.NibbleToHexAlpha
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(UtilMathTest, NibbleToHexAlpha) {
  EXPECT_EQ(nibble_to_hex(10), 'A');
  EXPECT_EQ(nibble_to_hex(15), 'F');
}
