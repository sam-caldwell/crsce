/**
 * @file hasher_extract_first_hex64_upper_to_lower_test.cpp
 * @brief Verify extract_first_hex64 lowercases uppercase hex and returns true.
 */
#include "common/HasherUtils/ExtractFirstHex64.h"
#include <gtest/gtest.h>
#include <string>

using crsce::common::hasher::extract_first_hex64;

TEST(HasherUtils, ExtractFirstHex64UppercaseLowercased) { // NOLINT
    const std::string src =
            "ABCDEFABCDEFABCDEFABCDEFABCDEFABABCDEFABCDEFABCDEFABCDEFABCDEFAB"; // 64 hex
    std::string out;
    ASSERT_TRUE(extract_first_hex64(src, out));
    EXPECT_EQ(out,
              "abcdefabcdefabcdefabcdefabcdefab"
              "abcdefabcdefabcdefabcdefabcdefab");
}
