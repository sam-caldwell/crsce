/**
 * One-test file: Happy path -in then -out order.
 */
#include "common/ArgParser/ArgParser.h"
#include "helpers.h"
#include <gtest/gtest.h>
#include <span>

using crsce::common::ArgParser;

TEST(ArgParserTest, HappyPathInThenOut) {
    // ReSharper disable once CppUseStructuredBinding
    auto a = make_argv({"prog", "-in", "a.bin", "-out", "b.bin"});
    ArgParser p("prog");
    const bool ok = p.parse(std::span<char *>{a.argv.data(), a.argv.size()});
    ASSERT_TRUE(ok);
    // ReSharper disable once CppUseStructuredBinding
    const auto &o = p.options();
    EXPECT_FALSE(o.help);
    EXPECT_EQ(o.input, "a.bin");
    EXPECT_EQ(o.output, "b.bin");
}
