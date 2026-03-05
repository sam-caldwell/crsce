/**
 * @file probability_estimator_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Unit tests for ProbabilityEstimator: CellScore computation, sorting, and edge cases.
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/LineID.h"
#include "decompress/Solvers/ProbabilityEstimator.h"
#include "decompress/Solvers/PropagationEngine.h"

using crsce::decompress::solvers::ConstraintStore;
using crsce::decompress::solvers::LineID;
using crsce::decompress::solvers::LineType;
using crsce::decompress::solvers::ProbabilityEstimator;
using crsce::decompress::solvers::PropagationEngine;

namespace {
    constexpr std::uint16_t kS = 511;
    constexpr std::uint16_t kNumDiags = (2 * kS) - 1;

    /**
     * @brief Helper to build a ConstraintStore with all-zero sum vectors.
     * @return A ConstraintStore where every target sum is 0.
     */
    ConstraintStore makeAllZeroStore() {
        const std::vector<std::uint16_t> rowSums(kS, 0);
        const std::vector<std::uint16_t> colSums(kS, 0);
        const std::vector<std::uint16_t> diagSums(kNumDiags, 0);
        const std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
        return {rowSums, colSums, diagSums, antiDiagSums,
                std::vector<std::uint16_t>(kS, 0), std::vector<std::uint16_t>(kS, 0),
                std::vector<std::uint16_t>(kS, 0), std::vector<std::uint16_t>(kS, 0)};
    }

    /**
     * @brief Helper to build a ConstraintStore with specified uniform sums.
     * @param rowVal Uniform row sum value.
     * @param colVal Uniform column sum value.
     * @param diagVal Uniform diagonal sum value.
     * @param antiDiagVal Uniform anti-diagonal sum value.
     * @return A ConstraintStore with the given uniform sums.
     */
    ConstraintStore makeUniformStore(const std::uint16_t rowVal,
                                     const std::uint16_t colVal,
                                     const std::uint16_t diagVal = 0,
                                     const std::uint16_t antiDiagVal = 0,
                                     const std::uint16_t ltpVal = 0) {
        const std::vector<std::uint16_t> rowSums(kS, rowVal);
        const std::vector<std::uint16_t> colSums(kS, colVal);
        const std::vector<std::uint16_t> diagSums(kNumDiags, diagVal);
        const std::vector<std::uint16_t> antiDiagSums(kNumDiags, antiDiagVal);
        return {rowSums, colSums, diagSums, antiDiagSums,
                std::vector<std::uint16_t>(kS, ltpVal), std::vector<std::uint16_t>(kS, ltpVal),
                std::vector<std::uint16_t>(kS, ltpVal), std::vector<std::uint16_t>(kS, ltpVal)};
    }
} // namespace

// ---------------------------------------------------------------------------
// ProbabilityEstimator construction
// ---------------------------------------------------------------------------

/**
 * @brief ProbabilityEstimator can be constructed with a const reference to ConstraintStore.
 */
TEST(ProbabilityEstimatorTest, ConstructionDoesNotThrow) {
    auto store = makeAllZeroStore();
    EXPECT_NO_THROW((ProbabilityEstimator(store))); // NOLINT(misc-const-correctness)
}

// ---------------------------------------------------------------------------
// computeCellScores tests
// ---------------------------------------------------------------------------

/**
 * @brief All-zero sums: all cells are unassigned, all scores should be 0/0 with preferred=0.
 */
TEST(ProbabilityEstimatorTest, AllZeroSumsProducesAllZeroScores) {
    auto store = makeAllZeroStore();
    const ProbabilityEstimator estimator(store);

    const auto scores = estimator.computeCellScores(0);

    // All 511 cells in row 0 should be scored
    EXPECT_EQ(scores.size(), kS);

    // With all targets zero, rho = 0 for all lines, so score1 = 0 for all.
    // score0 = u_col * u_diag * u_anti (all > 0), so preferred = 0.
    for (const auto &s : scores) {
        EXPECT_EQ(s.score1, 0U);
        EXPECT_GT(s.score0, 0U);
        EXPECT_EQ(s.preferred, 0U);
    }
}

/**
 * @brief After propagation forces all cells to 0, no unassigned cells remain.
 */
TEST(ProbabilityEstimatorTest, AllCellsAssignedReturnsEmpty) {
    auto store = makeAllZeroStore();
    PropagationEngine engine(store);

    // Propagate row 0 with zero target -- forces all cells in row 0 to 0
    std::vector<LineID> lines;
    lines.push_back({.type = LineType::Row, .index = 0});
    engine.propagate(lines);

    const ProbabilityEstimator estimator(store);
    const auto scores = estimator.computeCellScores(0);
    EXPECT_TRUE(scores.empty());
}

/**
 * @brief Scores are sorted by confidence descending.
 */
TEST(ProbabilityEstimatorTest, ScoresSortedByConfidenceDescending) {
    // Use non-uniform sums to produce different confidence values
    const std::vector<std::uint16_t> rowSums(kS, 255);
    std::vector<std::uint16_t> colSums(kS, 255);
    const std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    const std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    // Make column 0 different to create varied confidence values
    colSums[0] = 1;
    const ConstraintStore store(rowSums, colSums, diagSums, antiDiagSums,
                                std::vector<std::uint16_t>(kS, 0), std::vector<std::uint16_t>(kS, 0),
                                std::vector<std::uint16_t>(kS, 0), std::vector<std::uint16_t>(kS, 0));

    const ProbabilityEstimator estimator(store);
    const auto scores = estimator.computeCellScores(0);

    ASSERT_FALSE(scores.empty());
    for (std::size_t i = 1; i < scores.size(); ++i) {
        EXPECT_GE(scores.at(i - 1).confidence, scores.at(i).confidence)
            << "Scores not sorted at index " << i;
    }
}

/**
 * @brief With one cell already assigned, computeCellScores returns kS-1 scores.
 */
TEST(ProbabilityEstimatorTest, SkipsAssignedCells) {
    auto store = makeUniformStore(255, 255);
    store.assign(0, 0, 1);

    const ProbabilityEstimator estimator(store);
    const auto scores = estimator.computeCellScores(0);
    EXPECT_EQ(scores.size(), kS - 1);

    // Column 0 should not appear in scores
    for (const auto &s : scores) {
        EXPECT_NE(s.col, 0);
    }
}

/**
 * @brief When all column targets are kS (all ones needed), score1 should be large
 *        and preferred should be 1.
 */
TEST(ProbabilityEstimatorTest, HighTargetsPreferOne) {
    auto store = makeUniformStore(kS, kS, 1, 1, 1);
    const ProbabilityEstimator estimator(store);

    const auto scores = estimator.computeCellScores(0);
    ASSERT_FALSE(scores.empty());

    // All columns need all unknowns to be 1, so rho_col = kS and u_col = kS
    // This means (u_col - rho_col) = 0, so score0 = 0, preferred = 1
    for (const auto &s : scores) {
        EXPECT_EQ(s.score0, 0U);
        EXPECT_GT(s.score1, 0U);
        EXPECT_EQ(s.preferred, 1U);
    }
}

/**
 * @brief Each CellScore has a valid column index within [0, kS).
 */
TEST(ProbabilityEstimatorTest, ColumnIndicesAreValid) {
    auto store = makeUniformStore(255, 255);
    const ProbabilityEstimator estimator(store);

    const auto scores = estimator.computeCellScores(100);
    for (const auto &s : scores) {
        EXPECT_LT(s.col, kS);
    }
}

/**
 * @brief Works correctly for the last row (510).
 */
TEST(ProbabilityEstimatorTest, LastRowWorks) {
    auto store = makeUniformStore(255, 255);
    const ProbabilityEstimator estimator(store);

    const auto scores = estimator.computeCellScores(kS - 1);
    EXPECT_EQ(scores.size(), kS);
}
