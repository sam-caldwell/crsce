/**
 * @file: argparser_only_in_returns_false_test.cpp
 */
#include "common/ArgParser/ArgParser.h"
#include "helpers.h"
#include <gtest/gtest.h>
#include <span>

using crsce::common::ArgParser;

/**
 * @name ArgParserTest.OnlyInFlagReturnsFalse
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(ArgParserTest, OnlyInFlagReturnsFalse) {
    ArgParser p("prog");
    // ReSharper disable once CppUseStructuredBinding
    auto a = make_argv({"prog", "-in", "input.bin"});
    const bool ok = p.parse(std::span<char *>{a.argv.data(), a.argv.size()});
    EXPECT_FALSE(ok);
}
