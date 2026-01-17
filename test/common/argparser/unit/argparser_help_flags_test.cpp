/**
 * One-test file: Help flags set help true and parse ok.
 */
#include <gtest/gtest.h>
#include "CommandLineArgs/ArgParser.h"
#include "helpers.h"
#include <span>

using crsce::common::ArgParser;

TEST(ArgParserTest, HelpFlagsSetHelpTrue) {
  {
    auto a = make_argv({"prog", "-h"});
    ArgParser p("prog");
    const bool ok = p.parse(std::span<char*>{a.argv.data(), a.argv.size()});
    EXPECT_TRUE(ok);
    EXPECT_TRUE(p.options().help);
  }
  {
    auto a = make_argv({"prog", "--help"});
    ArgParser p("prog");
    const bool ok = p.parse(std::span<char*>{a.argv.data(), a.argv.size()});
    EXPECT_TRUE(ok);
    EXPECT_TRUE(p.options().help);
  }
}

