/**
 * @file compress_file_ok_test.cpp
 * @brief Unit: compress_file succeeds and writes artifacts.
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <cstdlib>
#include <fstream>
#include <random>
#include <vector>
#include <cstddef>
#include <ios>
#include <system_error>

#include "testrunner/Cli/detail/compress_file.h"
#include "testrunner/detail/sha512.h"
#include "helpers/tmp_dir.h"

namespace fs = std::filesystem;

namespace {
    fs::path write_small_file(const fs::path &p, const std::size_t n) {
        std::mt19937 rng(123);
        std::uniform_int_distribution<int> dist(0, 255);
        std::vector<char> buf(n);
        for (auto &c : buf) { c = static_cast<char>(dist(rng)); }
        std::ofstream os(p, std::ios::binary);
        os.write(buf.data(), static_cast<std::streamsize>(buf.size()));
        return p;
    }
}

TEST(TestRunnerRandom, CompressFileSucceedsWritesArtifacts) {
    fs::current_path(TEST_BINARY_DIR);
    const fs::path td = fs::path(tmp_dir()) / "trr_cx_ok";
    fs::create_directories(td);
    const fs::path in = td / "in.bin";
    const fs::path cx = td / "out.crsce";
    {
        std::error_code ec;
        fs::remove(cx, ec);
    }
    write_small_file(in, 128);
    const auto in_hash = crsce::testrunner::detail::compute_sha512(in);
    (void)crsce::testrunner::cli::compress_file(in, cx, in_hash, 5000);
    ASSERT_TRUE(fs::exists(cx));
    ASSERT_TRUE(fs::exists(fs::path(cx).replace_extension(".compress.stdout.txt")));
    ASSERT_TRUE(fs::exists(fs::path(cx).replace_extension(".compress.stderr.txt")));
}
