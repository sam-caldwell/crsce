/**
 * @file generate_file_test.cpp
 * @brief Unit tests for generate_file helper.
 */
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include "testrunner/Cli/detail/generate_file.h"
#include "testrunner/Cli/detail/generated_input.h"
#include "testrunner/detail/sha512.h"
#include "helpers/tmp_dir.h"

namespace fs = std::filesystem;

TEST(TestRunnerRandom, GenerateFileCreatesFileAndComputesHash) {
    const fs::path out_dir = fs::path(tmp_dir()) / "trr_gen";
    fs::create_directories(out_dir);
    const auto gi = crsce::testrunner::cli::generate_file(out_dir);

    ASSERT_TRUE(fs::exists(gi.path));
    EXPECT_GE(gi.blocks, 1ULL);
    const auto hash = crsce::testrunner::detail::compute_sha512(gi.path);
    EXPECT_EQ(hash, gi.sha512);
}
