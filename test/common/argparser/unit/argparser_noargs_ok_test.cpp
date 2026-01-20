/**
 * One-test file: No args parses ok and no help flag.
 */
#include "common/ArgParser/ArgParser.h"
#include "helpers.h"
#include <gtest/gtest.h>
#include <span>

using crsce::common::ArgParser;

TEST(ArgParserTest, NoArgsParsesOkNoHelp) {
  // ReSharper disable once CppUseStructuredBinding
  auto a = make_argv({"prog"});
  ArgParser p("prog");
  const bool ok = p.parse(std::span<char *>{a.argv.data(), a.argv.size()});
  EXPECT_TRUE(ok);
  // ReSharper disable once CppUseStructuredBinding
  const auto &o = p.options();
  EXPECT_FALSE(o.help);
  EXPECT_TRUE(o.input.empty());
  EXPECT_TRUE(o.output.empty());
}
