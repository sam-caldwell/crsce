/**
 * @file hasher_extract_first_hex64_insufficient_test.cpp
 * @brief Verify extract_first_hex64 returns false for insufficient input.
 */
#include "common/HasherUtils/ExtractFirstHex64.h"
#include <gtest/gtest.h>
#include <string>

using crsce::common::hasher::extract_first_hex64;

/**

 * @name HasherUtils.ExtractFirstHex64Insufficient

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(HasherUtils, ExtractFirstHex64Insufficient) { // NOLINT
    constexpr std::string src = "1234567890abcdef"; // too short
    std::string out;
    EXPECT_FALSE(extract_first_hex64(src, out));
}
