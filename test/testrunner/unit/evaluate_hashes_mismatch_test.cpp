/**
 * @file evaluate_hashes_mismatch_test.cpp
 * @brief Unit: evaluate_hashes throws on mismatch.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include <gtest/gtest.h>

#include "testRunnerRandom/Cli/detail/evaluate_hashes.h"
#include "common/exceptions/PossibleCollisionException.h"

TEST(TestRunnerRandom, EvaluateHashesThrowsOnMismatch) {
    EXPECT_THROW({ crsce::testrunner_random::cli::evaluate_hashes("abc", "xyz"); }, crsce::common::exceptions::PossibleCollisionException);
}
