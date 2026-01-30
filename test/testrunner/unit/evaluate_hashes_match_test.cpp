/**
 * @file evaluate_hashes_match_test.cpp
 * @brief Unit: evaluate_hashes does not throw on match.
 */
#include <gtest/gtest.h>

#include "testrunner/Cli/detail/evaluate_hashes.h"

TEST(TestRunnerRandom, EvaluateHashesNoThrowOnMatch) {
    EXPECT_NO_THROW({ crsce::testrunner::cli::evaluate_hashes("abc", "abc"); });
}
