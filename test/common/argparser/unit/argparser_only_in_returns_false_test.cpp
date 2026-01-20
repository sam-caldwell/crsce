/**
 * @file: argparser_only_in_returns_false_test.cpp
 */
#include "common/ArgParser/ArgParser.h"
#include "helpers.h"
#include <gtest/gtest.h>
#include <span>

using crsce::common::ArgParser;

TEST(ArgParserTest, OnlyInFlagReturnsFalse) {
  ArgParser p("prog");
  auto a = make_argv({"prog", "-in", "input.bin"});
  const bool ok = p.parse(std::span<char *>{a.argv.data(), a.argv.size()});
  EXPECT_FALSE(ok);
}
