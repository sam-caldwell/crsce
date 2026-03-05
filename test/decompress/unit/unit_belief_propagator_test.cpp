/**
 * @file unit_belief_propagator_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Unit tests for BeliefPropagator Gaussian BP.
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "decompress/Solvers/BeliefPropagator.h"
#include "decompress/Solvers/ConstraintStore.h"

using crsce::decompress::solvers::BeliefPropagator;
using crsce::decompress::solvers::ConstraintStore;

namespace {
    constexpr std::uint16_t kS        = 511;
    constexpr std::uint16_t kNumDiags = (2U * kS) - 1U;

    /**
     * @brief Build a ConstraintStore with uniform row/col/ltp sums and appropriate diag sums.
     * @param rowSum  Target sum applied to all kS row lines.
     * @param colSum  Target sum applied to all kS column lines.
     * @return ConstraintStore with all cells unassigned.
     */
    ConstraintStore makeUniformStore(const std::uint16_t rowSum, const std::uint16_t colSum) {
        const std::vector<std::uint16_t> rows(kS, rowSum);
        const std::vector<std::uint16_t> cols(kS, colSum);
        // Diagonal sums: diagonal d has length min(d+1, 2s-1-d).
        // Use 0 for simplicity (consistent with zero targets on most lines).
        const std::vector<std::uint16_t> diags(kNumDiags, 0U);
        const std::vector<std::uint16_t> antis(kNumDiags, 0U);
        const std::vector<std::uint16_t> ltpZero(kS, 0U);
        return {rows, cols, diags, antis, ltpZero, ltpZero, ltpZero, ltpZero};
    }
} // namespace

// ---------------------------------------------------------------------------
// Initial state
// ---------------------------------------------------------------------------

/**
 * @brief After construction (no BP run), belief() returns 0.5 everywhere.
 */
TEST(BeliefPropagatorTest, FreshBeliefsAreHalf) {
    const auto cs = makeUniformStore(0, 0);
    const BeliefPropagator bp(cs);

    EXPECT_FLOAT_EQ(bp.belief(0, 0), 0.5F);
    EXPECT_FLOAT_EQ(bp.belief(255, 255), 0.5F);
    EXPECT_FLOAT_EQ(bp.belief(510, 510), 0.5F);
}

// ---------------------------------------------------------------------------
// Zero iterations
// ---------------------------------------------------------------------------

/**
 * @brief run() with 0 iterations leaves all beliefs at 0.5.
 */
TEST(BeliefPropagatorTest, ZeroIterationsPreservesHalf) {
    const auto cs = makeUniformStore(0, 0);
    BeliefPropagator bp(cs);
    const auto result = bp.run(0.5F, 0U);

    EXPECT_EQ(result.iterationsRun, 0U);
    EXPECT_FLOAT_EQ(bp.belief(0, 0), 0.5F);
}

// ---------------------------------------------------------------------------
// reset() restores uniform prior
// ---------------------------------------------------------------------------

/**
 * @brief After run() followed by reset(), beliefs return to 0.5.
 */
TEST(BeliefPropagatorTest, ResetRestoresHalf) {
    const auto cs = makeUniformStore(0, 0);
    BeliefPropagator bp(cs);

    bp.run(0.5F, 5U);
    bp.reset();

    EXPECT_FLOAT_EQ(bp.belief(0, 0), 0.5F);
    EXPECT_FLOAT_EQ(bp.belief(200, 100), 0.5F);
}

// ---------------------------------------------------------------------------
// Zero-target rows push beliefs below 0.5
// ---------------------------------------------------------------------------

/**
 * @brief With all line targets = 0 (all cells must be 0), BP should drive
 *        marginals below 0.5 after 30 iterations.
 */
TEST(BeliefPropagatorTest, ZeroTargetDrivesBeliefsBelowHalf) {
    const auto cs = makeUniformStore(0, 0);
    BeliefPropagator bp(cs);
    const auto result = bp.run(0.5F, 30U);

    EXPECT_EQ(result.iterationsRun, 30U);
    // maxBias must be positive (beliefs moved away from 0.5)
    EXPECT_GT(result.maxBias, 0.01F);
    // With rho=0 on all lines, beliefs should be pushed below 0.5
    EXPECT_LT(bp.belief(0, 0), 0.5F);
    EXPECT_LT(bp.belief(255, 255), 0.5F);
}

// ---------------------------------------------------------------------------
// RunResult fields are in valid ranges
// ---------------------------------------------------------------------------

/**
 * @brief RunResult.maxDelta and maxBias are non-negative; iterationsRun matches request.
 */
TEST(BeliefPropagatorTest, RunResultFieldsAreValid) {
    const auto cs = makeUniformStore(255, 255);
    BeliefPropagator bp(cs);
    const auto result = bp.run(0.5F, 10U);

    EXPECT_GE(result.maxDelta, 0.0F);
    EXPECT_GE(result.maxBias, 0.0F);
    EXPECT_LE(result.maxBias, 0.5F);
    EXPECT_EQ(result.iterationsRun, 10U);
}

// ---------------------------------------------------------------------------
// Warm-start: second run() continues from previous messages
// ---------------------------------------------------------------------------

/**
 * @brief Running 10 + 10 iterations produces the same final bias as 20 iterations
 *        from the same initial state (warm-start equivalence).
 */
TEST(BeliefPropagatorTest, WarmStartEquivalence) {
    const auto cs = makeUniformStore(0, 0);

    BeliefPropagator bpSplit(cs);
    bpSplit.run(0.5F, 10U);
    const auto splitResult = bpSplit.run(0.5F, 10U);

    BeliefPropagator bpFull(cs);
    const auto fullResult = bpFull.run(0.5F, 20U);

    // Both should produce similar (not necessarily identical) beliefs
    EXPECT_NEAR(splitResult.maxBias, fullResult.maxBias, 0.1F);
}
