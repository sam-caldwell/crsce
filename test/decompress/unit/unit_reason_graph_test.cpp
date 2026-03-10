/**
 * @file unit_reason_graph_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Unit tests for the ReasonGraph class.
 */
#include <gtest/gtest.h>

#include <cstdint>

#include "decompress/Solvers/CellAntecedent.h"
#include "decompress/Solvers/ReasonGraph.h"

using crsce::decompress::solvers::CellAntecedent;
using crsce::decompress::solvers::ReasonGraph;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

/**
 * @brief A freshly constructed ReasonGraph has all entries in the default
 *        (unassigned) state.
 */
TEST(ReasonGraphTest, DefaultStateAfterConstruction) {
    const ReasonGraph rg;
    const auto &e = rg.getAntecedent(0, 0);
    EXPECT_FALSE(e.isAssigned);
    EXPECT_FALSE(e.isDecision);
    EXPECT_EQ(e.antecedentLine, CellAntecedent::kNoAntecedent);
    EXPECT_EQ(e.stackDepth, 0U);
}

// ---------------------------------------------------------------------------
// recordDecision
// ---------------------------------------------------------------------------

/**
 * @brief recordDecision sets isAssigned, isDecision, stackDepth, and
 *        antecedentLine = kNoAntecedent.
 */
TEST(ReasonGraphTest, RecordDecision) {
    ReasonGraph rg;
    rg.recordDecision(3, 7, 42);
    const auto &e = rg.getAntecedent(3, 7);
    EXPECT_TRUE(e.isAssigned);
    EXPECT_TRUE(e.isDecision);
    EXPECT_EQ(e.stackDepth, 42U);
    EXPECT_EQ(e.antecedentLine, CellAntecedent::kNoAntecedent);
}

/**
 * @brief recordDecision at a corner cell (0,0) is stored correctly.
 */
TEST(ReasonGraphTest, RecordDecisionCorner) {
    ReasonGraph rg;
    rg.recordDecision(0, 0, 0);
    const auto &e = rg.getAntecedent(0, 0);
    EXPECT_TRUE(e.isAssigned);
    EXPECT_TRUE(e.isDecision);
    EXPECT_EQ(e.stackDepth, 0U);
}

/**
 * @brief recordDecision at the last cell (510,510) is stored correctly.
 */
TEST(ReasonGraphTest, RecordDecisionLastCell) {
    ReasonGraph rg;
    constexpr std::uint16_t kLast = 510;
    rg.recordDecision(kLast, kLast, 999);
    const auto &e = rg.getAntecedent(kLast, kLast);
    EXPECT_TRUE(e.isAssigned);
    EXPECT_TRUE(e.isDecision);
    EXPECT_EQ(e.stackDepth, 999U);
}

// ---------------------------------------------------------------------------
// recordPropagated
// ---------------------------------------------------------------------------

/**
 * @brief recordPropagated sets isAssigned=true, isDecision=false, stackDepth,
 *        and antecedentLine.
 */
TEST(ReasonGraphTest, RecordPropagated) {
    ReasonGraph rg;
    rg.recordPropagated(10, 20, 77, 1234);
    const auto &e = rg.getAntecedent(10, 20);
    EXPECT_TRUE(e.isAssigned);
    EXPECT_FALSE(e.isDecision);
    EXPECT_EQ(e.stackDepth, 77U);
    EXPECT_EQ(e.antecedentLine, 1234U);
}

// ---------------------------------------------------------------------------
// unrecord
// ---------------------------------------------------------------------------

/**
 * @brief unrecord clears a previously recorded decision entry back to default.
 */
TEST(ReasonGraphTest, UnrecordClearsDecision) {
    ReasonGraph rg;
    rg.recordDecision(5, 5, 100);
    rg.unrecord(5, 5);
    const auto &e = rg.getAntecedent(5, 5);
    EXPECT_FALSE(e.isAssigned);
    EXPECT_FALSE(e.isDecision);
    EXPECT_EQ(e.antecedentLine, CellAntecedent::kNoAntecedent);
}

/**
 * @brief unrecord clears a previously recorded propagated entry back to default.
 */
TEST(ReasonGraphTest, UnrecordClearsPropagated) {
    ReasonGraph rg;
    rg.recordPropagated(2, 3, 50, 999);
    rg.unrecord(2, 3);
    const auto &e = rg.getAntecedent(2, 3);
    EXPECT_FALSE(e.isAssigned);
    EXPECT_EQ(e.antecedentLine, CellAntecedent::kNoAntecedent);
}

/**
 * @brief unrecord on an already-default cell is a safe no-op.
 */
TEST(ReasonGraphTest, UnrecordOnDefaultIsNoOp) {
    ReasonGraph rg;
    rg.unrecord(0, 0); // should not crash
    const auto &e = rg.getAntecedent(0, 0);
    EXPECT_FALSE(e.isAssigned);
}

// ---------------------------------------------------------------------------
// Independence of cells
// ---------------------------------------------------------------------------

/**
 * @brief Recording in one cell does not affect adjacent cells.
 */
TEST(ReasonGraphTest, CellsAreIndependent) {
    ReasonGraph rg;
    rg.recordDecision(1, 1, 10);
    rg.recordPropagated(1, 2, 11, 5);

    const auto &e1 = rg.getAntecedent(1, 1);
    const auto &e2 = rg.getAntecedent(1, 2);
    const auto &e3 = rg.getAntecedent(1, 0); // untouched

    EXPECT_TRUE(e1.isDecision);
    EXPECT_FALSE(e2.isDecision);
    EXPECT_FALSE(e3.isAssigned);
}
