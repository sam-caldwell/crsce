/**
 * @file hasher_compute_control_sha256_fallback_shasum_test.cpp
 */
#include "common/HasherUtils/ComputeControlSha256.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <string>
#include <vector>

using crsce::common::hasher::compute_control_sha256;

TEST(HasherUtils, ComputeControlSha256FallbackShasumIfAvailable) { // NOLINT(readability-identifier-naming)
    // Only run if a system shasum exists; otherwise skip
    std::string shasum = "/usr/bin/shasum";
    if (!std::filesystem::exists(shasum)) {
        shasum = "/opt/homebrew/bin/shasum";
        if (!std::filesystem::exists(shasum)) {
            GTEST_SKIP() << "shasum not available";
        }
    }
    // Clear env so fallback search is exercised
    (void) ::unsetenv("CRSCE_HASHER_CMD"); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
    (void) ::unsetenv("CRSCE_HASHER_CANDIDATE"); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
    const std::vector<std::uint8_t> data{9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    std::string hex;
    ASSERT_TRUE(compute_control_sha256(data, hex));
    EXPECT_EQ(hex.size(), 64U);
}
