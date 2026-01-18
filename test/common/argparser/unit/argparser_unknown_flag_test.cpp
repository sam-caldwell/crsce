/**
 * One-test file: Unknown flag fails parse.
 */
#include <gtest/gtest.h>
#include "CommandLineArgs/ArgParser.h"
#include "helpers.h"
#include <span>

using crsce::common::ArgParser;

TEST(ArgParserTest, UnknownFlagFails) {
    // ReSharper disable once CppUseStructuredBinding
    auto a = make_argv({"prog", "--bogus"});
    ArgParser p("prog");
    const bool ok = p.parse(std::span<char *>{a.argv.data(), a.argv.size()});
    EXPECT_FALSE(ok);
}
