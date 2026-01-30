/**
 * @file test_runner_zeros_cli_integration.cpp
 * @brief Integration test for the TestRunnerZeros pipeline via direct run().
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <vector>
#include <iterator>
#include <algorithm>

#include "testrunnerZeros/Cli/detail/run.h"
#include "helpers/tmp_dir.h"

namespace fs = std::filesystem;

namespace {
std::vector<std::uint8_t> slurp(const fs::path &p) {
    std::ifstream is(p, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(is)), {});
}
}

TEST(TestRunnerZeros, CliEndToEnd) {
    // Change cwd so compress/decompress executables are found as ./compress and ./decompress
    const fs::path old = fs::current_path();
    fs::current_path(TEST_BINARY_DIR);
    const fs::path out_dir = fs::path(tmp_dir()) / "trz_cli";
    fs::create_directories(out_dir);

    const int rc = crsce::testrunner_zeros::cli::run(out_dir);
    fs::current_path(old);
    ASSERT_EQ(rc, 0);

    // Verify inputs and reconstructed outputs match byte-for-byte
    const std::vector<std::string> suffixes = {"1", "5", "10", "20"};
    for (const auto &suf : suffixes) {
        const auto in_glob = out_dir / ("zeros_input_" + suf + "_");
        const auto dx_glob = out_dir / ("zeros_reconstructed_" + suf + "_");
        // Find the generated files by prefix match (timestamp follows)
        fs::path in_file; fs::path dx_file;
        for (const auto &de : fs::directory_iterator(out_dir)) {
            const auto name = de.path().filename().string();
            if (name.starts_with(in_glob.filename().string())) { in_file = de.path(); }
            if (name.starts_with(dx_glob.filename().string())) { dx_file = de.path(); }
        }
        ASSERT_TRUE(fs::exists(in_file));
        ASSERT_TRUE(fs::exists(dx_file));

        const auto a = slurp(in_file);
        const auto b = slurp(dx_file);
        ASSERT_EQ(a.size(), b.size());
        EXPECT_TRUE(std::equal(a.begin(), a.end(), b.begin()));
    }
}
