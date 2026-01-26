/**
 * @file hasher_compute_control_sha256_env_candidate_fail_test.cpp
 */
#include "common/HasherUtils/detail/compute_control_sha256.h"
#include <gtest/gtest.h>
#include <cstdint>
#include <string>
#include <vector>

using crsce::common::hasher::compute_control_sha256;

/**
 * @name HasherUtils.ComputeControlSha256EnvCandidateFail
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(HasherUtils, ComputeControlSha256EnvCandidateFail) { // NOLINT
    // Ensure CRSCE_HASHER_CMD is unset; set candidate to a nonexistent tool
    (void) ::unsetenv("CRSCE_HASHER_CMD"); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
    ASSERT_EQ(::setenv("CRSCE_HASHER_CANDIDATE", "/nonexistent/sha256", 1), 0); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
    const std::vector<std::uint8_t> data{1, 2, 3, 4};
    std::string hex;
    EXPECT_FALSE(compute_control_sha256(data, hex));
    (void) ::unsetenv("CRSCE_HASHER_CANDIDATE"); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
}
