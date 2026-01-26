/**
 * @file hasher_run_sha256_stdin_status_fail_test.cpp
 */
#include "common/HasherUtils/detail/run_sha256_stdin.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <cstdint>

using crsce::common::hasher::run_sha256_stdin;

/**

 * @name HasherUtils.RunSha256StdinStatusFail

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(HasherUtils, RunSha256StdinStatusFail) { // NOLINT(readability-identifier-naming)
    const std::vector<std::string> cmd{"/bin/false"};
    const std::vector<std::uint8_t> data{1, 2, 3};
    std::string hex;
    EXPECT_FALSE(run_sha256_stdin(cmd, data, hex));
}
