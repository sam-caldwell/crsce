/**
 * @file evaluate_hashes_mismatch_test.cpp
 * @brief Unit: evaluate_hashes throws on mismatch.
 */
#include <gtest/gtest.h>

#include "testrunner/Cli/detail/evaluate_hashes.h"
#include "common/exceptions/PossibleCollisionException.h"

TEST(TestRunnerRandom, EvaluateHashesThrowsOnMismatch) {
    EXPECT_THROW({ crsce::testrunner::cli::evaluate_hashes("abc", "xyz"); }, crsce::common::exceptions::PossibleCollisionException);
}
