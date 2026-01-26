/**
 * @file: util_classify_char_other_test.cpp
 */
#include "common/Util/CharClass.h"
#include "common/Util/detail/classify_char.h"
#include <gtest/gtest.h>

using crsce::common::util::CharClass;
using crsce::common::util::classify_char;

/**

 * @name UtilMathTest.ClassifyCharOther

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(UtilMathTest, ClassifyCharOther) {
  EXPECT_EQ(classify_char('#'), CharClass::Other);
}
