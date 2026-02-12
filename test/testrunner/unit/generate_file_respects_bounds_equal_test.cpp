/**
 * @file generate_file_respects_bounds_equal_test.cpp
 * @brief Unit test: generator respects equal min/max bounds as a fixed size.
 */
#include <gtest/gtest.h>
#include <filesystem>
#include "testRunnerRandom/Cli/detail/generate_file.h"
#include "helpers/tmp_dir.h"

namespace fs = std::filesystem;

TEST(TestRunnerRandom, GenerateFileRespectsBoundsWhenEqual) { // NOLINT
    const fs::path out_dir = fs::path(tmp_dir()) / "trr_gen_fixed";
    fs::create_directories(out_dir);
    const auto gi = crsce::testrunner_random::cli::generate_random_file(out_dir, 65536ULL, 65536ULL);

    ASSERT_TRUE(fs::exists(gi.path));
    EXPECT_EQ(gi.bytes, 65536ULL);
    EXPECT_GE(gi.blocks, 1ULL);
}

