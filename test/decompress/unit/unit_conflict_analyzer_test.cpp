/**
 * @file unit_conflict_analyzer_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Unit tests for ConflictAnalyzer::analyzeHashFailure.
 */
#include <gtest/gtest.h>

#include <cstdint>

#include "decompress/Solvers/ConflictAnalyzer.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/LineID.h"
#include "decompress/Solvers/ReasonGraph.h"

using crsce::decompress::solvers::ConflictAnalyzer;
using crsce::decompress::solvers::ConstraintStore;
using crsce::decompress::solvers::LineType;
using crsce::decompress::solvers::ReasonGraph;

// ---------------------------------------------------------------------------
// No valid target (empty reason graph)
// ---------------------------------------------------------------------------

/**
 * @brief When no cells are assigned, analyzeHashFailure returns valid=false.
 */
TEST(ConflictAnalyzerTest, NoAssignedCellsReturnsInvalid) {
    const ReasonGraph rg;
    const ConflictAnalyzer ca(rg);
    const auto target = ca.analyzeHashFailure(0, 10);
    EXPECT_FALSE(target.valid);
}

// ---------------------------------------------------------------------------
// All failedRow cells are decisions at various depths
// ---------------------------------------------------------------------------

/**
 * @brief Decisions in failedRow below conflictDepth are correctly identified
 *        and the maximum depth is returned.
 */
TEST(ConflictAnalyzerTest, DecisionsInFailedRowFoundDirectly) {
    ReasonGraph rg;

    constexpr std::uint16_t kFailedRow   = 3;
    constexpr std::uint32_t kConflict    = 100;

    // Mark most cells as decisions at depth 5
    for (std::uint16_t c = 0; c < 511; ++c) {
        rg.recordDecision(kFailedRow, c, 5);
    }
    // Override two cells at higher depths (still below conflict)
    rg.recordDecision(kFailedRow, 7,  42);
    rg.recordDecision(kFailedRow, 99, 77);

    const ConflictAnalyzer ca(rg);
    const auto target = ca.analyzeHashFailure(kFailedRow, kConflict);

    EXPECT_TRUE(target.valid);
    EXPECT_EQ(target.targetDepth, 77U); // max depth below 100
}

// ---------------------------------------------------------------------------
// Cells assigned at exactly conflictDepth (decisions) are skipped
// ---------------------------------------------------------------------------

/**
 * @brief Decision cells at conflictDepth are not candidates; cells below are.
 */
TEST(ConflictAnalyzerTest, DecisionAtConflictDepthIsNotCandidate) {
    ReasonGraph rg;

    constexpr std::uint16_t kFailedRow = 1;
    constexpr std::uint32_t kConflict  = 50;

    // Cell (1,0): decision at conflictDepth — NOT a candidate
    rg.recordDecision(kFailedRow, 0, kConflict);
    // Cell (1,1): decision below conflictDepth — IS a candidate
    rg.recordDecision(kFailedRow, 1, 30);

    const ConflictAnalyzer ca(rg);
    const auto target = ca.analyzeHashFailure(kFailedRow, kConflict);

    EXPECT_TRUE(target.valid);
    EXPECT_EQ(target.targetDepth, 30U);
}

// ---------------------------------------------------------------------------
// Propagation antecedent chain across rows
// ---------------------------------------------------------------------------

/**
 * @brief A propagated cell at conflictDepth triggers BFS into its antecedent
 *        line, from which lower-depth decision cells are found.
 *
 * Setup:
 *  failedRow = 5
 *  Cell (5,0): propagated at conflictDepth=50 from Row 3 (flat index 3)
 *  Cell (3,7): decision at depth 25 (found via antecedent BFS through Row 3)
 *  Expected: targetDepth = 25
 */
TEST(ConflictAnalyzerTest, FollowsAntecedentLineAcrossRows) {
    ReasonGraph rg;

    constexpr std::uint16_t kFailedRow    = 5;
    constexpr std::uint32_t kConflict     = 50;
    constexpr std::uint32_t kTargetDepth  = 25;

    // antecedentLine = flat index of Row 3 = 3 (from ConstraintStore::lineIndex)
    const std::uint32_t row3FlatIdx = ConstraintStore::lineIndex(
        {.type = LineType::Row, .index = 3});

    // (5,0): forced at conflictDepth by Row 3
    rg.recordPropagated(kFailedRow, 0, kConflict, row3FlatIdx);

    // (3,7): decision at lower depth — discoverable via antecedent chain
    rg.recordDecision(3, 7, kTargetDepth);

    const ConflictAnalyzer ca(rg);
    const auto target = ca.analyzeHashFailure(kFailedRow, kConflict);

    EXPECT_TRUE(target.valid);
    EXPECT_EQ(target.targetDepth, kTargetDepth);
}

// ---------------------------------------------------------------------------
// Reproducible across multiple calls (epoch reset)
// ---------------------------------------------------------------------------

/**
 * @brief analyzeHashFailure produces consistent results across repeated calls.
 */
TEST(ConflictAnalyzerTest, RepeatedCallsProduceConsistentResults) {
    ReasonGraph rg;

    constexpr std::uint16_t kFailedRow = 2;
    constexpr std::uint32_t kConflict  = 80;

    rg.recordDecision(kFailedRow, 10, 60);
    rg.recordDecision(kFailedRow, 20, 40);

    const ConflictAnalyzer ca(rg);

    for (int i = 0; i < 5; ++i) {
        const auto target = ca.analyzeHashFailure(kFailedRow, kConflict);
        EXPECT_TRUE(target.valid);
        EXPECT_EQ(target.targetDepth, 60U);
    }
}
