/**
 * @file decompress_file_ok_test.cpp
 * @brief Unit: decompress_file succeeds and writes artifacts.
 */
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <system_error>
#include <string>

#include "testrunner/Cli/detail/compress_file.h"
#include "testrunner/Cli/detail/decompress_file.h"
#include "testrunner/detail/sha512.h"
#include "helpers/tmp_dir.h"

namespace fs = std::filesystem;

namespace {

    fs::path write_small_file(const fs::path &p) {
        std::ofstream os(p, std::ios::binary);
        const std::string payload = "Hello, CRSCE!";
        os.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        return p;
    }
}

TEST(TestRunnerRandom, DecompressFileSucceedsWritesArtifacts) {
    fs::current_path(TEST_BINARY_DIR);
    const fs::path td = fs::path(tmp_dir()) / "trr_dx_ok";
    fs::create_directories(td);
    const fs::path in = td / "in.bin";
    const fs::path cx = td / "out.crsce";
    const fs::path dx = td / "out.bin";
    {
        std::error_code ec;
        fs::remove(cx, ec);
        fs::remove(dx, ec);
    }
    write_small_file(in);
    const auto in_hash = crsce::testrunner::detail::compute_sha512(in);
    (void)crsce::testrunner::cli::compress_file(in, cx, in_hash, 5000);
    const auto out_hash = crsce::testrunner::cli::decompress_file(cx, dx, in_hash, 5000);
    ASSERT_TRUE(fs::exists(dx));
    EXPECT_EQ(out_hash, in_hash);
    ASSERT_TRUE(fs::exists(fs::path(cx).replace_extension(".decompress.stdout.txt")));
    ASSERT_TRUE(fs::exists(fs::path(cx).replace_extension(".decompress.stderr.txt")));
}
