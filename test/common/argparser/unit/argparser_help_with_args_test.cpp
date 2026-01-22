/**
 * One-test file: Help flag with other args; parse ok and help true.
 */
#include "common/ArgParser/ArgParser.h"
#include "helpers.h"
#include <gtest/gtest.h>
#include <span>

using crsce::common::ArgParser;

/**

 * @name ArgParserTest.HelpFlagWithOtherArgs

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(ArgParserTest, HelpFlagWithOtherArgs) {
    // ReSharper disable once CppUseStructuredBinding
    auto a = make_argv({"prog", "-h", "-in", "a.bin", "-out", "b.bin"});
    ArgParser p("prog");
    const bool ok = p.parse(std::span<char *>{a.argv.data(), a.argv.size()});
    EXPECT_TRUE(ok);
    EXPECT_TRUE(p.options().help);
}
