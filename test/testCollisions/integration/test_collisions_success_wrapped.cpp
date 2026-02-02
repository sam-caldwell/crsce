/**
 * @file test_collisions_success_wrapped.cpp
 * @brief Integration test: wrap compress/decompress with pass-through copies to force 'pass' flow and cleanup.
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <algorithm>
#include <optional>
#include "helpers/exe_path.h"
#include <system_error>
#include <fstream>
#include <string>
#include <vector>
#include <optional>

#include "testRunnerRandom/detail/run_process.h"

namespace fs = std::filesystem;

namespace {
std::vector<fs::path> list_logs3(const fs::path &dir) {
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

TEST(TestCollisionsCli, WrappedCompressDecompressForcesPassAndCleanup) {
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path out_dir = bin_dir / "testCollisions";
    std::error_code ec_mk; fs::create_directories(out_dir, ec_mk);
    const auto before = list_logs3(out_dir);

    // Create wrapper dir
    const fs::path wrap_dir = bin_dir / ".test_tmp_wrappers";
    std::error_code ec; fs::create_directories(wrap_dir, ec);
    // compress wrapper: cp in out
    {
        std::ofstream os(wrap_dir / "compress");
        ASSERT_TRUE(os.good());
        os << "#!/bin/sh\n"
              "IN=\"\"; OUT=\"\";\n"
              "while [ $# -gt 0 ]; do\n"
              "  case \"$1\" in\n"
              "    -in) IN=\"$2\"; shift 2;;\n"
              "    -out) OUT=\"$2\"; shift 2;;\n"
              "    *) shift;;\n"
              "  esac\n"
              "done\n"
              "cp \"$IN\" \"$OUT\"\n";
    }
    // decompress wrapper: cp in out
    {
        std::ofstream os(wrap_dir / "decompress");
        ASSERT_TRUE(os.good());
        os << "#!/bin/sh\n"
              "IN=\"\"; OUT=\"\";\n"
              "while [ $# -gt 0 ]; do\n"
              "  case \"$1\" in\n"
              "    -in) IN=\"$2\"; shift 2;;\n"
              "    -out) OUT=\"$2\"; shift 2;;\n"
              "    *) shift;;\n"
              "  esac\n"
              "done\n"
              "cp \"$IN\" \"$OUT\"\n";
    }
    // Make wrappers executable
    fs::permissions(wrap_dir / "compress", fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::add);
    fs::permissions(wrap_dir / "decompress", fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::add);

    const std::string exe = crsce::test::exe_path("testCollisions");
    // Ensure PATH includes /usr/bin for 'env'
    const std::string new_path = std::string("/usr/bin:/bin");
    const std::vector<std::string> argv = {
        "env",
        std::string("TEST_BINARY_DIR=") + wrap_dir.string(),
        std::string("PATH=") + new_path,
        exe,
        "-min_blocks", "1",
        "-max_blocks", "2",
        "-cycles", "1"
    };

    const auto res = crsce::testrunner::detail::run_process(argv, std::nullopt);
    ASSERT_EQ(res.exit_code, 0) << res.out << "\n" << res.err;

    const auto after = list_logs3(out_dir);
    ASSERT_GE(after.size(), before.size() + 1);
    const fs::path &log_path = after.back();

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
    const std::string &result = cols[14];
    EXPECT_EQ(result, "pass");
    EXPECT_FALSE(fs::exists(compressed_file));
    const fs::path dx_path = fs::path(compressed_file).replace_extension(".reconstructed");
    EXPECT_FALSE(fs::exists(dx_path));
}
