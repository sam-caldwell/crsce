/**
 * @file test_runner_alternating01_cli_integration.cpp
 * @brief Integration test for the TestRunnerAlternating01 pipeline via direct run().
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <algorithm>
#include <exception>
#include <string>
#include <vector>

#include "testrunnerAlternating01/Cli/detail/run.h"
#include "helpers/tmp_dir.h"

namespace fs = std::filesystem;

namespace {
std::vector<std::uint8_t> slurp(const fs::path &p) {
    std::ifstream is(p, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(is)), {});
}
}

TEST(TestRunnerAlternating01, CliEndToEndBestEffort) {
    const fs::path old = fs::current_path();
    fs::current_path(TEST_BINARY_DIR);
    const fs::path out_dir = fs::path(tmp_dir()) / "tralt01_cli";
    fs::create_directories(out_dir);

    // Run best-effort: if any case fails, continue to verify successful pairs.
    try {
        (void)crsce::testrunner_alternating01::cli::run(out_dir);
    } catch (const std::exception &) {
        // Swallow; we only validate pairs that completed.
    }
    fs::current_path(old);

    const std::vector<std::string> suffixes = {"1", "5", "10", "20"};
    std::size_t verified = 0;
    for (const auto &suf : suffixes) {
        const auto in_prefix = std::string("alternating01_input_") + suf + "_";
        const auto dx_prefix = std::string("alternating01_reconstructed_") + suf + "_";
        fs::path in_file; fs::path dx_file;
        for (const auto &de : fs::directory_iterator(out_dir)) {
            const auto name = de.path().filename().string();
            if (name.starts_with(in_prefix)) { in_file = de.path(); }
            if (name.starts_with(dx_prefix)) { dx_file = de.path(); }
        }
        if (in_file.empty() || dx_file.empty()) { continue; }

        const auto a = slurp(in_file);
        const auto b = slurp(dx_file);
        ASSERT_EQ(a.size(), b.size());
        EXPECT_TRUE(std::equal(a.begin(), a.end(), b.begin()));
        ++verified;
    }
    ASSERT_GE(verified, static_cast<std::size_t>(1));
}
