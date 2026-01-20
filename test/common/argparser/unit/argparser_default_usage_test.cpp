/**
 * @file: argparser_default_usage_test.cpp
 * @brief Default program name in usage string.
 */
#include "common/ArgParser/ArgParser.h"
#include <gtest/gtest.h>
#include <string>

using crsce::common::ArgParser;

TEST(ArgParserTest, DefaultProgramNameUsage) {
  const ArgParser p("");
  const auto u = p.usage();
  EXPECT_EQ(u.rfind("program ", 0), 0U);
}
