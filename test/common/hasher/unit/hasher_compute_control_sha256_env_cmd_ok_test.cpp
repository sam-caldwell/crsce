/**
 * @file hasher_compute_control_sha256_env_cmd_ok_test.cpp
 */
#include "common/HasherUtils/detail/compute_control_sha256.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <cstdint>

using crsce::common::hasher::compute_control_sha256;

/**
 * @name HasherUtils.ComputeControlSha256EnvCmdOk
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(HasherUtils, ComputeControlSha256EnvCmdOk) { // NOLINT(readability-identifier-naming)
    const std::string helper = std::string(TEST_BINARY_DIR) + "/sha256_ok_helper";
    // Point directly to helper via env override
    ASSERT_EQ(::setenv("CRSCE_HASHER_CMD", helper.c_str(), 1), 0); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
    // Ensure candidate env is not set and won’t interfere
    (void) ::unsetenv("CRSCE_HASHER_CANDIDATE"); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
    const std::vector<std::uint8_t> data{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::string hex;
    ASSERT_TRUE(compute_control_sha256(data, hex));
    EXPECT_EQ(hex.size(), 64U);
    // Cleanup
    (void) ::unsetenv("CRSCE_HASHER_CMD"); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
}
