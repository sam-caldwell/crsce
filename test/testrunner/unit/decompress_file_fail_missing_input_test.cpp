/**
 * @file decompress_file_fail_missing_input_test.cpp
 * @brief Unit: decompress_file throws on missing input; artifacts preserved.
 */
#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include "testrunner/Cli/detail/decompress_file.h"
#include "common/exceptions/DecompressNonZeroExitException.h"
#include "helpers/tmp_dir.h"

namespace fs = std::filesystem;

namespace {}

TEST(TestRunnerRandom, DecompressFileFailsOnMissingInputWritesArtifacts) {
    const fs::path td = fs::path(tmp_dir()) / "trr_dx_fail";
    fs::create_directories(td);
    const fs::path cx = td / "missing.crsce";
    const fs::path dx = td / "out.bin";
    EXPECT_THROW({
        (void)crsce::testrunner::cli::decompress_file(cx, dx, "nohash", 5000);
    }, crsce::common::exceptions::DecompressNonZeroExitException);
    ASSERT_TRUE(fs::exists(fs::path(cx).replace_extension(".decompress.stdout.txt")));
    ASSERT_TRUE(fs::exists(fs::path(cx).replace_extension(".decompress.stderr.txt")));
}
