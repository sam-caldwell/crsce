/**
 * @file make_input_zero_bytes_blocks_guard_test.cpp
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <ios>

#include "testrunner/Cli/detail/make_input.h"
#include "helpers/tmp_dir.h"

namespace fs = std::filesystem;

TEST(TestRunnerCommon, MakeInputZeroBytesSetsBlocksToOne) {
    // Create a zero-length file and ensure make_input clamps blocks to 1
    const fs::path td = fs::path(tmp_dir()) / "trcommon_mkinput";
    fs::create_directories(td);
    const fs::path p = td / "empty.bin";
    std::ofstream os(p, std::ios::binary); // zero-length
    os.close();

    const auto gi = crsce::testrunner::cli::make_input(p, 0ULL);
    ASSERT_EQ(gi.path, p);
    EXPECT_EQ(gi.bytes, 0ULL);
    EXPECT_EQ(gi.blocks, 1ULL); // clamped
    EXPECT_FALSE(gi.sha512.empty());
}
