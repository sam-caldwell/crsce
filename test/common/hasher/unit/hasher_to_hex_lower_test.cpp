/**
 * @file hasher_to_hex_lower_test.cpp
 * @brief Unit tests for to_hex_lower utility.
 */
#include "common/HasherUtils/ToHexLower.h"
#include "common/BitHashBuffer/detail/Sha256.h"
#include <string>
#include <gtest/gtest.h>

using crsce::common::hasher::to_hex_lower;
using crsce::common::detail::sha256::sha256_digest;
using u8 = crsce::common::detail::sha256::u8;

TEST(HasherUtils, ToHexLowerKnownDigest) { // NOLINT(readability-identifier-naming)
    // SHA-256("") known digest
    const auto digest = sha256_digest(nullptr, 0);
    const std::string hex = to_hex_lower(digest);
    EXPECT_EQ(hex,
              "e3b0c44298fc1c149afbf4c8996fb924"
              "27ae41e4649b934ca495991b7852b855");
}
