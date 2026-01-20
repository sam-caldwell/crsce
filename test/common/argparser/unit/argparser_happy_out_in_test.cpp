/**
 * One-test file: Happy path -out then -in order.
 */
#include "common/ArgParser/ArgParser.h"
#include "helpers.h"
#include <gtest/gtest.h>
#include <span>

using crsce::common::ArgParser;

TEST(ArgParserTest, HappyPathOutThenIn) {
  // ReSharper disable once CppUseStructuredBinding
  auto a = make_argv({"prog", "-out", "b.bin", "-in", "a.bin"});
  ArgParser p("prog");
  const bool ok = p.parse(std::span<char *>{a.argv.data(), a.argv.size()});
  ASSERT_TRUE(ok);
  const auto &o = p.options();
  EXPECT_FALSE(o.help);
  EXPECT_EQ(o.input, "a.bin");
  EXPECT_EQ(o.output, "b.bin");
}
