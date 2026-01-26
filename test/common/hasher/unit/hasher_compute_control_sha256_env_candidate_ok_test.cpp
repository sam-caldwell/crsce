/**
 * @file hasher_compute_control_sha256_env_candidate_ok_test.cpp
 */
#include "common/HasherUtils/detail/compute_control_sha256.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <cstdint>

using crsce::common::hasher::compute_control_sha256;

/**

 * @name HasherUtils.ComputeControlSha256EnvCandidateOk

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(HasherUtils, ComputeControlSha256EnvCandidateOk) { // NOLINT(readability-identifier-naming)
    const std::string helper = std::string(TEST_BINARY_DIR) + "/sha256_ok_helper";
    // Use candidate env override, ensure CMD is not set
    (void) ::unsetenv("CRSCE_HASHER_CMD"); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
    ASSERT_EQ(::setenv("CRSCE_HASHER_CANDIDATE", helper.c_str(), 1), 0); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
    const std::vector<std::uint8_t> data{42, 7, 1, 2, 3, 99};
    std::string hex;
    ASSERT_TRUE(compute_control_sha256(data, hex));
    EXPECT_EQ(hex.size(), 64U);
    // Cleanup
    (void) ::unsetenv("CRSCE_HASHER_CANDIDATE"); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
}
