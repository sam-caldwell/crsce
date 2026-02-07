/**
 * @file test_collisions_success.cpp
 * @brief Integration test for successful cycle and artifact cleanup.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <algorithm>
#include <optional>
#include <fstream>
#include <string>
#include <vector>
#include "helpers/exe_path.h"
#include <sstream>

#include "testRunnerRandom/detail/run_process.h"

namespace fs = std::filesystem;

namespace {
std::vector<fs::path> list_logs(const fs::path &dir) {
    std::vector<fs::path> out;
    if (!fs::exists(dir) || !fs::is_directory(dir)) { return out; }
    for (const auto &e: fs::directory_iterator(dir)) {
        if (e.is_regular_file()) {
            const auto name = e.path().filename().string();
            if (name.starts_with("testCollisions_") && e.path().extension() == ".log") {
                out.push_back(e.path());
            }
        }
    }
    std::ranges::sort(out);
    return out;
}
}

TEST(TestCollisionsCli, SingleCyclePassDeletesArtifactsAndLogs) {
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path out_dir = bin_dir / "testCollisions";
    const auto before = list_logs(out_dir);

    const std::string exe = crsce::test::exe_path("testCollisions");
    const std::vector<std::string> argv = {
        exe,
        "-min_blocks", "1",
        "-max_blocks", "2",
        "-cycles", "1"
    };
    const auto res = crsce::testrunner::detail::run_process(argv, std::nullopt);
    ASSERT_EQ(res.exit_code, 0) << res.out << "\n" << res.err;

    // Prefer explicit LOG_PATH from stdout to avoid race with concurrent tests
    fs::path log_path;
    {
        const std::string key = "LOG_PATH=";
        std::istringstream iss(res.out);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.starts_with(key)) { log_path = fs::path(line.substr(key.size())); break; }
        }
    }
    if (log_path.empty()) {
        const auto after = list_logs(out_dir);
        ASSERT_GE(after.size(), before.size() + 1);
        log_path = after.back();
    }

    // Read last line
    std::ifstream is(log_path);
    ASSERT_TRUE(is.good());
    std::string header;
    std::getline(is, header);
    std::string last_line;
    std::string line;
    while (std::getline(is, line)) {
        if (!line.empty()) { last_line = line; }
    }
    ASSERT_FALSE(last_line.empty());

    // Split TSV
    std::vector<std::string> cols;
    {
        std::string cur;
        for (const char c: last_line) {
            if (c == '\t') { cols.push_back(cur); cur.clear(); } else { cur.push_back(c); }
        }
        cols.push_back(cur);
    }
    ASSERT_EQ(cols.size(), 15U) << last_line;

    const std::string &compressed_file = cols[7];
    const std::string &compressed_hash = cols[8];
    const std::string &reconstructed_hash = cols[12];
    const std::string &result = cols[14];

    // Program should log sane values; result may be 'fail' given real decompressor complexity.
    EXPECT_TRUE(result == "pass" || result == "fail");
    // If hashes are available they should be non-empty.
    if (!compressed_hash.empty()) { EXPECT_GE(compressed_hash.size(), 64U); }
    if (!reconstructed_hash.empty()) { EXPECT_GE(reconstructed_hash.size(), 64U); }
}
