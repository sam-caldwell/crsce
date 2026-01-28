/**
 * @file validate_in_out_happy_returns_0_test.cpp
 * @brief validate_in_out should return 0 when -in exists and -out does not.
 */
#include "common/Cli/ValidateInOut.h"
#include "common/ArgParser/ArgParser.h"
#include "helpers/tmp_dir.h"
#include "helpers/remove_file.h"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <ios>
#include <span>
#include <string>
#include <vector>

using crsce::common::ArgParser;
namespace fs = std::filesystem;

/**
 * @name ValidateInOut.HappyReturns0
 * @brief Expect return code 0 when input exists and output does not.
 */
TEST(ValidateInOut, HappyReturns0) { // NOLINT
    const auto td = tmp_dir();
    const std::string in = td + "/vio_in.bin";
    const std::string out = td + "/vio_out.crsce";
    remove_file(in);
    remove_file(out);

    {
        std::ofstream f(in, std::ios::binary);
        ASSERT_TRUE(f.good());
        f.put(static_cast<char>(0x01));
    }
    ASSERT_TRUE(fs::exists(in));
    ASSERT_FALSE(fs::exists(out));

    ArgParser p("dummy");
    std::vector<std::string> av = {"dummy", "-in", in, "-out", out};
    std::vector<char*> argv;
    argv.reserve(av.size());
    for (auto &s : av) {
        argv.push_back(s.data());
    }
    const int rc = crsce::common::cli::validate_in_out(p, std::span<char*>{argv.data(), argv.size()});
    EXPECT_EQ(rc, 0);
}
