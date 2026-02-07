/**
 * @file evaluate_hashes_match_test.cpp
 * @brief Unit: evaluate_hashes does not throw on match.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include <gtest/gtest.h>

#include "testRunnerRandom/Cli/detail/evaluate_hashes.h"

TEST(TestRunnerRandom, EvaluateHashesNoThrowOnMatch) {
    EXPECT_NO_THROW({ crsce::testrunner_random::cli::evaluate_hashes("abc", "abc"); });
}
