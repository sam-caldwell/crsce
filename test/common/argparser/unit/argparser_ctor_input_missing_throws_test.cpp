/**
 * @file argparser_ctor_input_missing_throws_test.cpp
 * @brief ArgParser ctor(parse-on-ctor) throws CliInputMissing when input path does not exist.
 */

#include <gtest/gtest.h>
#include <span>
#include <string>
#include <vector>

#include "common/ArgParser/ArgParser.h"
#include "common/exceptions/CliInputMissing.h"

/**
 * @name ArgParser.CtorInputMissingThrows
 * @brief Expect CliInputMissing when -in points to a nonexistent file.
 */
TEST(ArgParser, CtorInputMissingThrows) {
    const std::string in = "/tmp/crsce_argparser_input_missing_does_not_exist.bin";
    const std::string out = "/tmp/crsce_argparser_output_missing_ok.bin";
    std::vector<std::string> av = {"prog", "-in", in, "-out", out};
    std::vector<char*> argv; argv.reserve(av.size());
    for (auto &s : av) { argv.push_back(s.data()); }
    EXPECT_THROW((crsce::common::ArgParser{"prog", std::span<char*>{argv.data(), argv.size()}}),
                 crsce::common::exceptions::CliInputMissing);
}

