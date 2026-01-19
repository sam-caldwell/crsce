/**
 * @file: util_classify_char_digit_test.cpp
 */
#include "common/Util/CharClass.h"
#include "common/Util/ClassifyChar.h"
#include <gtest/gtest.h>

using crsce::common::util::CharClass;
using crsce::common::util::classify_char;

TEST(UtilMathTest, ClassifyCharDigit) {
  EXPECT_EQ(classify_char('5'), CharClass::Digit);
}
