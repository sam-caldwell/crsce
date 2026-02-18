/**
 * @file argparser_ctor_output_exists_throws_test.cpp
 * @brief ArgParser ctor(parse-on-ctor) throws CliOutputExists when output path already exists.
 */

#include <gtest/gtest.h>
#include <span>
#include <string>
#include <vector>
#include <fstream>
#include <ios>

#include "common/ArgParser/ArgParser.h"
#include "common/exceptions/CliOutputExists.h"
#include "helpers/tmp_dir.h"
#include "helpers/remove_file.h"

/**
 * @name ArgParser.CtorOutputExistsThrows
 * @brief Expect CliOutputExists when -out path already exists.
 */
TEST(ArgParser, CtorOutputExistsThrows) {
    const auto td = tmp_dir();
    const std::string in = td + "/argparser_ctor_out_exists_in.bin";
    const std::string out = td + "/argparser_ctor_out_exists_out.bin";
    remove_file(in); remove_file(out);
    {
        std::ofstream f(in, std::ios::binary); ASSERT_TRUE(f.good()); f.put(static_cast<char>(0x01));
    }
    {
        std::ofstream f(out, std::ios::binary); ASSERT_TRUE(f.good()); f.put(static_cast<char>(0x00));
    }
    std::vector<std::string> av = {"prog", "-in", in, "-out", out};
    std::vector<char*> argv; argv.reserve(av.size());
    for (auto &s : av) { argv.push_back(s.data()); }
    EXPECT_THROW((crsce::common::ArgParser{"prog", std::span<char*>{argv.data(), argv.size()}}),
                 crsce::common::exceptions::CliOutputExists);
    remove_file(in); remove_file(out);
}

