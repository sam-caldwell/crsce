/**
 * @file hasher_compute_control_sha256_cmd_with_cmd_test.cpp
 */
#include "common/HasherUtils/detail/compute_control_sha256_cmd.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>

using crsce::common::hasher::compute_control_sha256_cmd;

/**
 * @name HasherUtils.ComputeControlCmdWithCmdOk
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(HasherUtils, ComputeControlCmdWithCmdOk) { // NOLINT(readability-identifier-naming)
    std::string shasum = "/usr/bin/shasum";
    if (!std::filesystem::exists(shasum)) {
        shasum = "/opt/homebrew/bin/shasum";
    }
    if (!std::filesystem::exists(shasum)) {
        GTEST_SKIP() << "shasum not found";
    }
    const std::vector<std::string> cmd{shasum, "-a", "256", "-"};
    const std::vector<std::uint8_t> data{42, 0, 1, 2, 3, 4, 5};
    std::string hex;
    ASSERT_TRUE(compute_control_sha256_cmd(cmd, data, hex));
    EXPECT_EQ(hex.size(), 64U);
}
