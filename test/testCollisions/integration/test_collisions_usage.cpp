/**
 * @file test_collisions_usage.cpp
 * @brief Integration tests for testCollisions CLI: usage and validation errors.
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <string>
#include <optional>
#include <vector>

#include "testRunnerRandom/detail/run_process.h"

namespace fs = std::filesystem;

TEST(TestCollisionsCli, FailsWithoutRequiredArgs) {
    const fs::path bin_dir{TEST_BINARY_DIR};
    const std::string exe = (bin_dir / "bin" / "testCollisions").string();
    const std::vector<std::string> argv = { exe };
    const auto res = crsce::testrunner::detail::run_process(argv, std::nullopt);
    EXPECT_NE(res.exit_code, 0);
    EXPECT_EQ(res.exit_code, 2) << res.out << "\n" << res.err;
}

TEST(TestCollisionsCli, FailsWhenMinNotLessThanMax) {
    const fs::path bin_dir{TEST_BINARY_DIR};
    const std::string exe = (bin_dir / "bin" / "testCollisions").string();
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
