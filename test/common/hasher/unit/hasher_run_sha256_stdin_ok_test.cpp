/**
 * @file hasher_run_sha256_stdin_ok_test.cpp
 */
#include "common/HasherUtils/detail/run_sha256_stdin.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>

using crsce::common::hasher::run_sha256_stdin;

/**
 * @name HasherUtils.RunSha256StdinOk
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(HasherUtils, RunSha256StdinOk) { // NOLINT(readability-identifier-naming)
    std::string shasum = "/usr/bin/shasum";
    if (!std::filesystem::exists(shasum)) {
        shasum = "/opt/homebrew/bin/shasum";
    }
    if (!std::filesystem::exists(shasum)) {
        GTEST_SKIP() << "shasum not found";
    }
    const std::vector<std::string> cmd{shasum, "-a", "256", "-"};
    const std::vector<std::uint8_t> data{1, 2, 3, 4, 5, 6, 7, 8};
    std::string hex;
    ASSERT_TRUE(run_sha256_stdin(cmd, data, hex));
    EXPECT_EQ(hex.size(), 64U);
}
