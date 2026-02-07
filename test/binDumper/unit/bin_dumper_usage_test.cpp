/**
 * @file bin_dumper_usage_test.cpp
 * @brief Verify binDumper usage and exit code on missing args.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>
#include <optional>

#include "testRunnerRandom/detail/run_process.h"

namespace fs = std::filesystem;

TEST(BinDumper, UsageOnNoArgs) {
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path exe = bin_dir / "binDumper";
    const auto res = crsce::testrunner::detail::run_process({exe.string()}, std::nullopt);
    EXPECT_NE(res.exit_code, 0);
    EXPECT_NE(res.err.find("usage: binDumper <file>"), std::string::npos);
}
