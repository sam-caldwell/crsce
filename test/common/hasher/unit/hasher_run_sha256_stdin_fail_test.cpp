/**
 * @file hasher_run_sha256_stdin_fail_test.cpp
 * @brief Negative test for run_sha256_stdin (non-hex output should fail).
 */
#include "common/HasherUtils/RunSha256Stdin.h"
#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <cstdint>

using crsce::common::hasher::run_sha256_stdin;

/**

 * @name HasherUtils.RunSha256StdinFailsOnNonHexOutput

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(HasherUtils, RunSha256StdinFailsOnNonHexOutput) { // NOLINT(readability-identifier-naming)
    const std::vector<std::string> cmd{"/bin/echo", "hello"};
    const std::vector<std::uint8_t> data{1, 2, 3, 4};
    std::string hex;
    EXPECT_FALSE(run_sha256_stdin(cmd, data, hex));
}
