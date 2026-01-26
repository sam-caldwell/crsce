/**
 * @file output_exists_e2e_test.cpp
 * @brief E2E: decompress CLI returns 3 when output already exists.
 */
#include "decompress/Cli/detail/run.h"
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
 * @name DecompressCLI.OutputExists
 * @brief Expect return 3 when output path already exists.
 */
TEST(DecompressCLI, OutputExists) {
    const auto td = tmp_dir();
    const std::string in = td + "/decompress_cli_in.crsc";
    const std::string out = td + "/decompress_cli_out.tmp";
    const std::string raw = td + "/decompress_cli_raw.tmp";
    {
        const std::ofstream f(raw, std::ios::binary);
    }
    {
        const std::ofstream f(out, std::ios::binary);
    }
    {
        std::vector<std::string> cav = {"compress", "-in", raw, "-out", in};
        std::vector<char *> cargv;
        cargv.reserve(cav.size());
        for (auto &s: cav) { cargv.push_back(s.data()); }
        ASSERT_EQ(crsce::compress::cli::run(std::span<char*>{cargv.data(), cargv.size()}), 0);
    }

    std::vector<std::string> av = {"decompress", "-in", in, "-out", out};
    std::vector<char *> argv;
    argv.reserve(av.size());
    for (auto &s: av) { argv.push_back(s.data()); }
    const int rc = crsce::decompress::cli::run(std::span<char *>{argv.data(), argv.size()});
    EXPECT_EQ(rc, 3);
    remove_file(raw);
    remove_file(in);
    remove_file(out);
}
