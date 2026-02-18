/**
 * @file argparser_ctor_noargs_throws_test.cpp
 * @brief ArgParser ctor(parse-on-ctor) throws CliNoArgs when no args provided.
 */

#include <gtest/gtest.h>
#include <span>
#include <string>
#include <vector>

#include "common/ArgParser/ArgParser.h"
#include "common/exceptions/CliNoArgs.h"

/**
 * @name ArgParser.CtorNoArgsThrows
 * @brief Expect ArgParser(program, argv) to throw CliNoArgs when argc<=1.
 */
TEST(ArgParser, CtorNoArgsThrows) {
    std::vector<std::string> av = {"prog"};
    std::vector<char*> argv; argv.reserve(av.size());
    for (auto &s : av) { argv.push_back(s.data()); }
    EXPECT_THROW((crsce::common::ArgParser{"prog", std::span<char*>{argv.data(), argv.size()}}),
                 crsce::common::exceptions::CliNoArgs);
}

