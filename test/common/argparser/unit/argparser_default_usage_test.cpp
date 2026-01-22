/**
 * @file argparser_default_usage_test.cpp
 * @brief Default program name in usage string.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "common/ArgParser/ArgParser.h"
#include <gtest/gtest.h>
#include <string>

using crsce::common::ArgParser;

/**

 * @name ArgParserTest.DefaultProgramNameUsage

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(ArgParserTest, DefaultProgramNameUsage) {
    const ArgParser p("");
    const auto u = p.usage();
    EXPECT_EQ(u.rfind("program ", 0), 0U);
}
