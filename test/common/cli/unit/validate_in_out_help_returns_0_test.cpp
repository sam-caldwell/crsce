/**
 * @file validate_in_out_help_returns_0_test.cpp
 * @brief validate_in_out should return 0 and print usage when -h is present.
 */
#include "common/Cli/ValidateInOut.h"
#include "common/ArgParser/ArgParser.h"

#include <gtest/gtest.h>
#include <span>
#include <string>
#include <vector>

using crsce::common::ArgParser;

/**
 * @name ValidateInOut.HelpReturns0
 * @brief Expect return code 0 (help path) when -h is provided.
 */
TEST(ValidateInOut, HelpReturns0) { // NOLINT
    ArgParser p("dummy");
    std::vector<std::string> av = {"dummy", "-h"};
    std::vector<char*> argv;
    argv.reserve(av.size());
    for (auto &s : av) {
        argv.push_back(s.data());
    }
    const int rc = crsce::common::cli::validate_in_out(p, std::span<char*>{argv.data(), argv.size()});
    EXPECT_EQ(rc, 0);
}
