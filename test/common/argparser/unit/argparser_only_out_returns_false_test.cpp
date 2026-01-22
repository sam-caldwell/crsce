/**
 * @file: argparser_only_out_returns_false_test.cpp
 */
#include "common/ArgParser/ArgParser.h"
#include "helpers.h"
#include <gtest/gtest.h>
#include <span>

using crsce::common::ArgParser;

TEST(ArgParserTest, OnlyOutFlagReturnsFalse) {
    ArgParser p("prog");
    // ReSharper disable once CppUseStructuredBinding
    auto a = make_argv({"prog", "-out", "output.bin"});
    const bool ok = p.parse(std::span<char *>{a.argv.data(), a.argv.size()});
    EXPECT_FALSE(ok);
}
