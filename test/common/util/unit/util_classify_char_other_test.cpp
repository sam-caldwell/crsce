/**
 * @file: util_classify_char_other_test.cpp
 */
#include "common/Util/CharClass.h"
#include "common/Util/ClassifyChar.h"
#include <gtest/gtest.h>

using crsce::common::util::CharClass;
using crsce::common::util::classify_char;

TEST(UtilMathTest, ClassifyCharOther) {
  EXPECT_EQ(classify_char('#'), CharClass::Other);
}
