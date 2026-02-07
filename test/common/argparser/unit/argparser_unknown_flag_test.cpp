/**
 * @file argparser_unknown_flag_test.cpp
 * @brief One-test file: Unknown flag fails parse.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "common/ArgParser/ArgParser.h"
#include "helpers.h"
#include <gtest/gtest.h>
#include <span>

using crsce::common::ArgParser;

/**
 * @name ArgParserTest.UnknownFlagFails
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(ArgParserTest, UnknownFlagFails) {
    // ReSharper disable once CppUseStructuredBinding
    auto a = make_argv({"prog", "--bogus"});
    ArgParser p("prog");
    const bool ok = p.parse(std::span<char *>{a.argv.data(), a.argv.size()});
    EXPECT_FALSE(ok);
}
