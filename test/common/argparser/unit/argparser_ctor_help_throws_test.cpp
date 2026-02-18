/**
 * @file argparser_ctor_help_throws_test.cpp
 * @brief ArgParser ctor(parse-on-ctor) throws CliHelpRequested when -h present.
 */

#include <gtest/gtest.h>
#include <span>
#include <string>
#include <vector>

#include "common/ArgParser/ArgParser.h"
#include "common/exceptions/CliHelpRequested.h"

/**
 * @name ArgParser.CtorHelpThrows
 * @brief Expect ArgParser(program, argv) to throw CliHelpRequested for -h.
 */
TEST(ArgParser, CtorHelpThrows) {
    std::vector<std::string> av = {"prog", "-h"};
    std::vector<char*> argv; argv.reserve(av.size());
    for (auto &s : av) { argv.push_back(s.data()); }
    EXPECT_THROW((crsce::common::ArgParser{"prog", std::span<char*>{argv.data(), argv.size()}}),
                 crsce::common::exceptions::CliHelpRequested);
}

