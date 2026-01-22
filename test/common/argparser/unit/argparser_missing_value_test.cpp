/**
 * One-test file: Missing value for -in/-out fails parse.
 */
#include "common/ArgParser/ArgParser.h"
#include "helpers.h"
#include <gtest/gtest.h>
#include <span>

using crsce::common::ArgParser;

/**

 * @name ArgParserTest.MissingValueFails

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(ArgParserTest, MissingValueFails) {
    {
        // ReSharper disable once CppUseStructuredBinding
        auto a = make_argv({"prog", "-in"});
        ArgParser p("prog");
        EXPECT_FALSE(p.parse(std::span<char *>{a.argv.data(), a.argv.size()}));
    }
    {
        // ReSharper disable once CppUseStructuredBinding
        auto a = make_argv({"prog", "-out"});
        ArgParser p("prog");
        EXPECT_FALSE(p.parse(std::span<char *>{a.argv.data(), a.argv.size()}));
    }
}
