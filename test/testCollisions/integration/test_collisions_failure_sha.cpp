/**
 * @file test_collisions_failure_sha.cpp
 * @brief Integration test forcing SHA-512 unavailability to exercise failure path.
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <algorithm>
#include <optional>
#include <system_error>
#include <fstream>
#include <string>
#include <vector>
#include "helpers/exe_path.h"

#include "testRunnerRandom/detail/run_process.h"

namespace fs = std::filesystem;

namespace {
std::vector<fs::path> list_logs2(const fs::path &dir) {
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

TEST(TestCollisionsCli, CycleContinuesWhenShaUnavailableLogsFail) {
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path out_dir = bin_dir / "testCollisions";
    const auto before = list_logs2(out_dir);

    // Create a temp dir with a failing 'shasum' to override PATH
    const fs::path tmp_dir = bin_dir / ".test_tmp_sha_mask";
    std::error_code ec;
    fs::create_directories(tmp_dir, ec);
    const fs::path fake_shasum = tmp_dir / "shasum";
    {
        std::ofstream os(fake_shasum);
        ASSERT_TRUE(os.good());
        os << "#!/bin/sh\nexit 1\n";
    }
    // Make it executable
    fs::permissions(fake_shasum, fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::add);

    const std::string exe = crsce::test::exe_path("testCollisions");
    // Prepend our tmp to PATH but keep /usr/bin for 'env'
    const std::string new_path = tmp_dir.string() + ":/usr/bin:/bin";
    const std::vector<std::string> argv = {
        "env", std::string("PATH=") + new_path,
        exe,
        "-min_blocks", "1",
        "-max_blocks", "2",
        "-cycles", "1"
    };

    const auto res = crsce::testrunner::detail::run_process(argv, std::nullopt);
    ASSERT_EQ(res.exit_code, 0) << res.out << "\n" << res.err;

    const auto after = list_logs2(out_dir);
    ASSERT_GE(after.size(), before.size() + 1);
    const fs::path &log_path = after.back();

    std::ifstream is(log_path);
    ASSERT_TRUE(is.good());
    std::string header;
    std::getline(is, header);
    std::string last_line;
    std::string line;
    while (std::getline(is, line)) { if (!line.empty()) { last_line = line; } }
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
    EXPECT_EQ(cols[14], "fail");
}
