/**
 * @file hasher_extract_first_hex64_test.cpp
 * @brief Unit tests for extract_first_hex64 utility.
 */
#include "common/HasherUtils/ExtractFirstHex64.h"
#include <gtest/gtest.h>
#include <string>

using crsce::common::hasher::extract_first_hex64;

TEST(HasherUtils, ExtractFirstHex64UppercaseLowercased) { // NOLINT(readability-identifier-naming)
    const std::string src =
            "ABCDEFABCDEFABCDEFABCDEFABCDEFABABCDEFABCDEFABCDEFABCDEFABCDEFAB"; // 64 hex chars
    std::string out;
    EXPECT_TRUE(extract_first_hex64(src, out));
    EXPECT_EQ(out,
              "abcdefabcdefabcdefabcdefabcdefab"
              "abcdefabcdefabcdefabcdefabcdefab");
}

TEST(HasherUtils, ExtractFirstHex64Insufficient) { // NOLINT(readability-identifier-naming)
    constexpr std::string src = "1234567890abcdef"; // too short
    std::string out;
    EXPECT_FALSE(extract_first_hex64(src, out));
}
