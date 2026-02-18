/**
 * @file argparser_ctor_unknown_flag_parse_error_test.cpp
 * @brief ArgParser ctor(parse-on-ctor) throws CliParseError on unknown flags.
 */

#include <gtest/gtest.h>
#include <span>
#include <string>
#include <vector>

#include "common/ArgParser/ArgParser.h"
#include "common/exceptions/CliParseError.h"

/**
 * @name ArgParser.CtorUnknownFlagParseError
 * @brief Expect CliParseError when an unknown flag is present.
 */
TEST(ArgParser, CtorUnknownFlagParseError) {
    std::vector<std::string> av = {"prog", "--unknown"};
    std::vector<char*> argv; argv.reserve(av.size());
    for (auto &s : av) { argv.push_back(s.data()); }
    EXPECT_THROW((crsce::common::ArgParser{"prog", std::span<char*>{argv.data(), argv.size()}}),
                 crsce::common::exceptions::CliParseError);
}

