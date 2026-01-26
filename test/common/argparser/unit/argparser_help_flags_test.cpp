/**
 * One-test file: Help flags set help true and parse ok.
 */
#include "common/ArgParser/ArgParser.h"
#include "helpers.h"
#include <gtest/gtest.h>
#include <span>

using crsce::common::ArgParser;

/**
 * @name ArgParserTest.HelpFlagsSetHelpTrue
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(ArgParserTest, HelpFlagsSetHelpTrue) {
    {
        // ReSharper disable once CppUseStructuredBinding
        auto a = make_argv({"prog", "-h"});
        ArgParser p("prog");
        const bool ok = p.parse(std::span<char *>{a.argv.data(), a.argv.size()});
        EXPECT_TRUE(ok);
        EXPECT_TRUE(p.options().help);
    }
    {
        // ReSharper disable once CppUseStructuredBinding
        auto a = make_argv({"prog", "--help"});
        ArgParser p("prog");
        const bool ok = p.parse(std::span<char *>{a.argv.data(), a.argv.size()});
        EXPECT_TRUE(ok);
        EXPECT_TRUE(p.options().help);
    }
}
