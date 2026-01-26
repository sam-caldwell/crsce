/**
 * @file output_exists_e2e_test.cpp
 * @brief E2E: compress CLI returns 3 when output already exists.
 */
#include "compress/Cli/detail/run.h"
#include "helpers/tmp_dir.h"
#include "helpers/remove_file.h"

#include <gtest/gtest.h>
#include <fstream>
#include <ios>
#include <span>
#include <string>
#include <vector>

/**
 * @name CompressCLI.OutputExists
 * @brief Expect return 3 when an output path already exists.
 */
TEST(CompressCLI, OutputExists) {
    const auto td = tmp_dir();
    const std::string in = td + "/compress_cli_in.tmp";
    const std::string out = td + "/compress_cli_out.tmp";
    {
        const std::ofstream f(in, std::ios::binary);
    }
    {
        const std::ofstream f(out, std::ios::binary);
    }

    std::vector<std::string> av = {"compress", "-in", in, "-out", out};
    std::vector<char *> argv;
    argv.reserve(av.size());
    for (auto &s: av) { argv.push_back(s.data()); }
    const int rc = crsce::compress::cli::run(std::span<char *>{argv.data(), argv.size()});
    EXPECT_EQ(rc, 3);
    remove_file(in);
    remove_file(out);
}
