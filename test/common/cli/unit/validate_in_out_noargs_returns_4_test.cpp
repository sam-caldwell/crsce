/**
 * @file validate_in_out_noargs_returns_4_test.cpp
 * @brief validate_in_out should return 4 when no -in/-out are provided and no help flag.
 */
#include "common/Cli/ValidateInOut.h"
#include "common/ArgParser/ArgParser.h"

#include <gtest/gtest.h>
#include <span>
#include <string>
#include <vector>

using crsce::common::ArgParser;

/**
 * @name ValidateInOut.NoArgsReturns4
 * @brief Expect return code 4 (insufficient arguments) when invoked with only program name.
 */
TEST(ValidateInOut, NoArgsReturns4) { // NOLINT
    ArgParser p("dummy");
    std::vector<std::string> av = {"dummy"};
    std::vector<char*> argv;
    argv.reserve(av.size());
    for (auto &s : av) {
        argv.push_back(s.data());
    }
    const int rc = crsce::common::cli::validate_in_out(p, std::span<char*>{argv.data(), argv.size()});
    EXPECT_EQ(rc, 4);
}
