/**
 * @file hasher_compute_control_sha256_env_cmd_fail_test.cpp
 */
#include "common/HasherUtils/detail/compute_control_sha256.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <cstdint>

using crsce::common::hasher::compute_control_sha256;

/**
 * @name HasherUtils.ComputeControlSha256EnvCmdFail
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(HasherUtils, ComputeControlSha256EnvCmdFail) { // NOLINT(readability-identifier-naming)
    // Point to a nonexistent tool so run_sha256_stdin fails
    ASSERT_EQ(::setenv("CRSCE_HASHER_CMD", "/nonexistent/sha256", 1), 0); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
    (void) ::unsetenv("CRSCE_HASHER_CANDIDATE"); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
    const std::vector<std::uint8_t> data{1, 2, 3};
    std::string hex;
    EXPECT_FALSE(compute_control_sha256(data, hex));
    (void) ::unsetenv("CRSCE_HASHER_CMD"); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
}
