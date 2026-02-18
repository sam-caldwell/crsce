/**
 * @file argparser_ctor_missing_value_parse_error_test.cpp
 * @brief ArgParser ctor(parse-on-ctor) throws CliParseError on missing -in/-out value.
 */

#include <gtest/gtest.h>
#include <span>
#include <string>
#include <vector>

#include "common/ArgParser/ArgParser.h"
#include "common/exceptions/CliParseError.h"

/**
 * @name ArgParser.CtorMissingValueParseError
 * @brief Expect CliParseError when -in has no following value.
 */
TEST(ArgParser, CtorMissingValueParseError) {
    std::vector<std::string> av = {"prog", "-in"};
    std::vector<char*> argv; argv.reserve(av.size());
    for (auto &s : av) { argv.push_back(s.data()); }
    EXPECT_THROW((crsce::common::ArgParser{"prog", std::span<char*>{argv.data(), argv.size()}}),
                 crsce::common::exceptions::CliParseError);
}

