/**
 * One-test file: Duplicate -in/-out flags; last occurrence wins.
 */
#include <gtest/gtest.h>
#include "CommandLineArgs/ArgParser.h"
#include "helpers.h"
#include <span>

using crsce::common::ArgParser;

TEST(ArgParserTest, DuplicateFlagsLastWins) {
  auto a = make_argv({"prog", "-in", "a.bin", "-in", "b.bin", "-out", "x.bin", "-out", "y.bin"});
  ArgParser p("prog");
  const bool ok = p.parse(std::span<char*>{a.argv.data(), a.argv.size()});
  ASSERT_TRUE(ok);
  const auto& o = p.options();
  EXPECT_EQ(o.input, "b.bin");
  EXPECT_EQ(o.output, "y.bin");
  EXPECT_FALSE(o.help);
}

