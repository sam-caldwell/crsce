/**
 * One-test file: Duplicate -in/-out flags; last occurrence wins.
 */
#include "common/ArgParser/ArgParser.h"
#include "helpers.h"
#include <gtest/gtest.h>
#include <span>

using crsce::common::ArgParser;

/**
 * @name ArgParserTest.DuplicateFlagsLastWins
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(ArgParserTest, DuplicateFlagsLastWins) {
    // ReSharper disable once CppUseStructuredBinding
    auto a = make_argv({
        "prog", "-in", "a.bin", "-in", "b.bin", "-out", "x.bin",
        "-out", "y.bin"
    });
    ArgParser p("prog");
    const bool ok = p.parse(std::span<char *>{a.argv.data(), a.argv.size()});
    ASSERT_TRUE(ok);
    const auto &o = p.options();
    EXPECT_EQ(o.input, "b.bin");
    EXPECT_EQ(o.output, "y.bin");
    EXPECT_FALSE(o.help);
}
