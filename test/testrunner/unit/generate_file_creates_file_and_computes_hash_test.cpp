/**
 * @file generate_file_creates_file_and_computes_hash_test.cpp
 * @brief Unit test: generated file exists and SHA512 matches reported value.
 */
#include <gtest/gtest.h>
#include <filesystem>
#include "testRunnerRandom/Cli/detail/generate_file.h"
#include "testRunnerRandom/Cli/detail/generated_input.h"
#include "testRunnerRandom/detail/sha512.h"
#include "helpers/tmp_dir.h"

namespace fs = std::filesystem;

TEST(TestRunnerRandom, GenerateFileCreatesFileAndComputesHash) { // NOLINT
    const fs::path out_dir = fs::path(tmp_dir()) / "trr_gen";
    fs::create_directories(out_dir);
    const auto gi = crsce::testrunner_random::cli::generate_random_file(out_dir);

    ASSERT_TRUE(fs::exists(gi.path));
    EXPECT_GE(gi.blocks, 1ULL);
    const auto hash = crsce::testrunner::detail::compute_sha512(gi.path);
    EXPECT_EQ(hash, gi.sha512);
}

