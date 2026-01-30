/**
 * @file compress_file_fail_missing_input_test.cpp
 * @brief Unit: compress_file throws on missing input; artifacts preserved.
 */
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>

#include "testrunner/Cli/detail/compress_file.h"
#include "common/exceptions/CompressNonZeroExitException.h"
#include "helpers/tmp_dir.h"

namespace fs = std::filesystem;

namespace {}

TEST(TestRunnerRandom, CompressFileFailsOnMissingInputWritesArtifacts) {
    const fs::path td = fs::path(tmp_dir()) / "trr_cx_fail";
    fs::create_directories(td);
    const fs::path in = td / "missing.bin";
    const fs::path cx = td / "out.crsce";
    EXPECT_THROW({
        (void)crsce::testrunner::cli::compress_file(in, cx, "nohash", 5000);
    }, crsce::common::exceptions::CompressNonZeroExitException);
    ASSERT_TRUE(fs::exists(fs::path(cx).replace_extension(".compress.stdout.txt")));
    ASSERT_TRUE(fs::exists(fs::path(cx).replace_extension(".compress.stderr.txt")));
}
