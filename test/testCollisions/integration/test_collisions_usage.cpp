/**
 * @file test_collisions_usage.cpp
 * @brief Integration tests for testCollisions CLI: usage and validation errors.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */

#include <gtest/gtest.h>
#include <string>
#include <optional>
#include <vector>
#include "helpers/exe_path.h"

#include "testRunnerRandom/detail/run_process.h"

TEST(TestCollisionsCli, FailsWithoutRequiredArgs) {
    const std::string exe = crsce::test::exe_path("testCollisions");
    const std::vector<std::string> argv = { exe };
    const auto res = crsce::testrunner::detail::run_process(argv, std::nullopt);
    EXPECT_NE(res.exit_code, 0);
    EXPECT_EQ(res.exit_code, 2) << res.out << "\n" << res.err;
}

TEST(TestCollisionsCli, FailsWhenMinNotLessThanMax) {
    const std::string exe = crsce::test::exe_path("testCollisions");
    const std::vector<std::string> argv = {
        exe,
        "-min_blocks", "2",
        "-max_blocks", "2",
        "-cycles", "1"
    };
    const auto res = crsce::testrunner::detail::run_process(argv, std::nullopt);
    EXPECT_NE(res.exit_code, 0);
    EXPECT_EQ(res.exit_code, 2) << res.out << "\n" << res.err;
}
