/**
 * @file test_collisions_failure_compress_nonzero.cpp
 * @brief Integration test: force compress to exit non-zero to exercise fallback end timestamps.
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
#include <sstream>

#include "testRunnerRandom/detail/run_process.h"

namespace fs = std::filesystem;

namespace {
std::vector<fs::path> list_logs4(const fs::path &dir) {
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

TEST(TestCollisionsCli, CompressNonZeroExitHandledAndLoggedFail) {
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path out_dir = bin_dir / "testCollisions";
    std::error_code ec_mk; fs::create_directories(out_dir, ec_mk);
    const auto before = list_logs4(out_dir);

    // Create wrapper dir with failing compress
    const fs::path wrap_dir = bin_dir / ".test_tmp_compress_fail";
    std::error_code ec; fs::create_directories(wrap_dir, ec);
    // compress wrapper: exit 1
    {
        std::ofstream os(wrap_dir / "compress");
        ASSERT_TRUE(os.good());
        os << "#!/bin/sh\nexit 1\n";
    }
    // decompress wrapper: not used but provide a stub
    {
        std::ofstream os(wrap_dir / "decompress");
        ASSERT_TRUE(os.good());
        os << "#!/bin/sh\nexit 0\n";
    }
    fs::permissions(wrap_dir / "compress", fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::add);
    fs::permissions(wrap_dir / "decompress", fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::add);

    const std::string exe = crsce::test::exe_path("testCollisions");
    const std::string path_keep = std::string("/usr/bin:/bin");
    const std::vector<std::string> argv = {
        "env",
        std::string("TEST_BINARY_DIR=") + wrap_dir.string(),
        std::string("PATH=") + path_keep,
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
        const auto after = list_logs4(out_dir);
        ASSERT_GE(after.size(), before.size() + 1);
        log_path = after.back();
    }
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
