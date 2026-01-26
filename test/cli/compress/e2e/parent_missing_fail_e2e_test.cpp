/**
 * @file parent_missing_fail_e2e_test.cpp
 * @brief E2E: compress CLI returns 4 when parent dir is missing.
 */
#include "compress/Cli/detail/run.h"
#include "helpers/tmp_dir.h"
#include "helpers/remove_file.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <fstream>
#include <ios>
#include <span>
#include <string>
#include <vector>

namespace fs = std::filesystem;

/**
 * @name CompressCLI.CompressionFailsOnParentMissing
 * @brief Expect return 4 when output's parent directory doesn't exist.
 */
TEST(CompressCLI, CompressionFailsOnParentMissing) {
    const auto td = tmp_dir();
    const std::string in = td + "/compress_cli_in_no_parent.bin";
    const std::string out_dir = td + "/compress_cli_missing_parent";
    const std::string out = out_dir + "/out.crsc";
    fs::remove_all(out_dir);
    {
        std::ofstream f(in, std::ios::binary);
        std::string s = "data";
        f.write(s.data(), static_cast<std::streamsize>(s.size()));
    }

    std::vector<std::string> av = {"compress", "-in", in, "-out", out};
    std::vector<char *> argv;
    argv.reserve(av.size());
    for (auto &s: av) { argv.push_back(s.data()); }
    const int rc = crsce::compress::cli::run(std::span<char *>{argv.data(), argv.size()});
    EXPECT_EQ(rc, 4);

    remove_file(in);
}
