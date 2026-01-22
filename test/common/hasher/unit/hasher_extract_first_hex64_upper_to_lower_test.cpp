/**
 * @file hasher_extract_first_hex64_upper_to_lower_test.cpp
 * @brief Verify extract_first_hex64 lowercases uppercase hex and returns true.
 */
#include "common/HasherUtils/ExtractFirstHex64.h"
#include <gtest/gtest.h>
#include <string>

using crsce::common::hasher::extract_first_hex64;

/**

 * @name HasherUtils.ExtractFirstHex64UppercaseLowercased

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(HasherUtils, ExtractFirstHex64UppercaseLowercased) { // NOLINT
    const std::string src =
            "ABCDEFABCDEFABCDEFABCDEFABCDEFABABCDEFABCDEFABCDEFABCDEFABCDEFAB"; // 64 hex
    std::string out;
    ASSERT_TRUE(extract_first_hex64(src, out));
    EXPECT_EQ(out,
              "abcdefabcdefabcdefabcdefabcdefab"
              "abcdefabcdefabcdefabcdefabcdefab");
}
