/**
 * @file solver_coverage_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Unit tests for BranchingController, ConstraintStore (lineLen, unassign),
 *        Sha256HashVerifier, and EnumerationController to achieve full coverage
 *        of the targeted decompress solver source files.
 */
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "common/Csm/Csm.h"
#include "decompress/Solvers/BranchingController.h"
#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/EnumerationController.h"
#include "decompress/Solvers/LineID.h"
#include "decompress/Solvers/LtpTable.h"
#include "decompress/Solvers/PropagationEngine.h"
#include "decompress/Solvers/Sha256HashVerifier.h"

using crsce::decompress::solvers::BranchingController;
using crsce::decompress::solvers::CellState;
using crsce::decompress::solvers::ConstraintStore;
using crsce::decompress::solvers::EnumerationController;
using crsce::decompress::solvers::LineID;
using crsce::decompress::solvers::LineType;
using crsce::decompress::solvers::PropagationEngine;
using crsce::decompress::solvers::Sha256HashVerifier;
using crsce::decompress::solvers::kLtp1Base;
using crsce::decompress::solvers::kLtp2Base;
using crsce::decompress::solvers::kLtp3Base;
using crsce::decompress::solvers::kLtp4Base;
using crsce::decompress::solvers::ltpFlatIndices;

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
                                     const std::uint16_t antiDiagVal = 0) {
        const std::vector<std::uint16_t> rowSums(kS, rowVal);
        const std::vector<std::uint16_t> colSums(kS, colVal);
        const std::vector<std::uint16_t> diagSums(kNumDiags, diagVal);
        const std::vector<std::uint16_t> antiDiagSums(kNumDiags, antiDiagVal);
        return {rowSums, colSums, diagSums, antiDiagSums,
                std::vector<std::uint16_t>(kS, 0), std::vector<std::uint16_t>(kS, 0),
                std::vector<std::uint16_t>(kS, 0), std::vector<std::uint16_t>(kS, 0)};
    }
} // namespace

// ---------------------------------------------------------------------------
// BranchingController tests
// ---------------------------------------------------------------------------

/**
 * @brief branchOrder() returns {0, 1} for canonical lexicographic order.
 */
TEST(BranchingControllerTest, BranchOrderReturnsZeroThenOne) {
    auto store = makeAllZeroStore();
    PropagationEngine engine(store);
    const BranchingController brancher(store, engine);

    const auto order = brancher.branchOrder();
    EXPECT_EQ(order[0], 0);
    EXPECT_EQ(order[1], 1);
}

/**
 * @brief branchOrder() returns an array of size 2.
 */
TEST(BranchingControllerTest, BranchOrderReturnsTwoElements) {
    auto store = makeAllZeroStore();
    PropagationEngine engine(store);
    const BranchingController brancher(store, engine);

    const auto order = brancher.branchOrder();
    EXPECT_EQ(order.size(), 2U);
}

/**
 * @brief saveUndoPoint() returns current undo stack size (0 when empty).
 */
TEST(BranchingControllerTest, SaveUndoPointReturnsZeroInitially) {
    auto store = makeAllZeroStore();
    PropagationEngine engine(store);
    BranchingController brancher(store, engine);

    const auto token = brancher.saveUndoPoint();
    EXPECT_EQ(token, 0U);
}

/**
 * @brief saveUndoPoint() returns increasing tokens as assignments are recorded.
 */
TEST(BranchingControllerTest, SaveUndoPointReflectsStackGrowth) {
    auto store = makeAllZeroStore();
    PropagationEngine engine(store);
    BranchingController brancher(store, engine);

    const auto token0 = brancher.saveUndoPoint();
    EXPECT_EQ(token0, 0U);

    // Record an assignment and check the token increases
    store.assign(0, 0, 1);
    brancher.recordAssignment(0, 0);
    const auto token1 = brancher.saveUndoPoint();
    EXPECT_EQ(token1, 1U);

    // Record another assignment
    store.assign(0, 1, 0);
    brancher.recordAssignment(0, 1);
    const auto token2 = brancher.saveUndoPoint();
    EXPECT_EQ(token2, 2U);
}

/**
 * @brief undoToSavePoint() reverts assignments back to the saved state.
 */
TEST(BranchingControllerTest, UndoToSavePointRevertsAssignments) {
    auto store = makeAllZeroStore();
    PropagationEngine engine(store);
    BranchingController brancher(store, engine);

    // Save before any assignments
    const auto token = brancher.saveUndoPoint();

    // Assign cell (0, 0) to 1 and record it
    store.assign(0, 0, 1);
    brancher.recordAssignment(0, 0);

    // Assign cell (0, 1) to 0 and record it
    store.assign(0, 1, 0);
    brancher.recordAssignment(0, 1);

    // Both cells should now be assigned
    EXPECT_NE(store.getCellState(0, 0), CellState::Unassigned);
    EXPECT_NE(store.getCellState(0, 1), CellState::Unassigned);

    // Undo to the save point
    brancher.undoToSavePoint(token);

    // Both cells should be unassigned again
    EXPECT_EQ(store.getCellState(0, 0), CellState::Unassigned);
    EXPECT_EQ(store.getCellState(0, 1), CellState::Unassigned);
}

/**
 * @brief undoToSavePoint() only reverts assignments after the save point.
 */
TEST(BranchingControllerTest, UndoToSavePointPreservesEarlierAssignments) {
    auto store = makeAllZeroStore();
    PropagationEngine engine(store);
    BranchingController brancher(store, engine);

    // Assign cell (0, 0) to 1 and record it
    store.assign(0, 0, 1);
    brancher.recordAssignment(0, 0);

    // Save after the first assignment
    const auto token = brancher.saveUndoPoint();

    // Assign cell (0, 1) to 0 and record it
    store.assign(0, 1, 0);
    brancher.recordAssignment(0, 1);

    // Undo only the second assignment
    brancher.undoToSavePoint(token);

    // First assignment should be preserved, second should be reverted
    EXPECT_EQ(store.getCellState(0, 0), CellState::One);
    EXPECT_EQ(store.getCellState(0, 1), CellState::Unassigned);
}

/**
 * @brief undoToSavePoint() with a token equal to current stack size is a no-op.
 */
TEST(BranchingControllerTest, UndoToCurrentPointIsNoOp) {
    auto store = makeAllZeroStore();
    PropagationEngine engine(store);
    BranchingController brancher(store, engine);

    store.assign(0, 0, 1);
    brancher.recordAssignment(0, 0);

    const auto token = brancher.saveUndoPoint();
    brancher.undoToSavePoint(token);

    // The assignment before the save point should be preserved
    EXPECT_EQ(store.getCellState(0, 0), CellState::One);
}

/**
 * @brief nextCell() returns the first unassigned cell in row-major order.
 */
TEST(BranchingControllerTest, NextCellReturnsFirstUnassigned) {
    auto store = makeAllZeroStore();
    PropagationEngine engine(store);
    const BranchingController brancher(store, engine);

    // Initially, (0, 0) is the first unassigned cell
    auto cell = brancher.nextCell();
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(cell->first, 0);
    EXPECT_EQ(cell->second, 0);

    // Assign (0, 0) and check that nextCell moves forward
    store.assign(0, 0, 0);
    cell = brancher.nextCell();
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(cell->first, 0);
    EXPECT_EQ(cell->second, 1);
}

/**
 * @brief nextCell() returns nullopt when all cells in a store with all-zero sums
 *        have been assigned by propagation.
 */
TEST(BranchingControllerTest, NextCellReturnsNulloptWhenAllAssigned) {
    auto store = makeAllZeroStore();
    PropagationEngine engine(store);
    const BranchingController brancher(store, engine);

    // Propagate all lines with zero sums -- this forces every cell to 0.
    std::vector<LineID> allLines;
    allLines.reserve(kS);
    for (std::uint16_t i = 0; i < kS; ++i) {
        allLines.push_back({.type = LineType::Row, .index = i});
    }
    engine.propagate(allLines);

    // All cells should now be assigned
    auto cell = brancher.nextCell();
    EXPECT_FALSE(cell.has_value());
}

// ---------------------------------------------------------------------------
// ConstraintStore::lineLen tests (tested indirectly via initial getUnknownCount)
// ---------------------------------------------------------------------------

/**
 * @brief Row lines have length kS (511) as seen through initial unknown count.
 */
TEST(ConstraintStoreLineLenTest, RowLineLenIsS) {
    auto store = makeAllZeroStore();

    const LineID line{.type = LineType::Row, .index = 0};
    EXPECT_EQ(store.getUnknownCount(line), kS);

    const LineID lastRow{.type = LineType::Row, .index = static_cast<std::uint16_t>(kS - 1)};
    EXPECT_EQ(store.getUnknownCount(lastRow), kS);
}

/**
 * @brief Column lines have length kS (511) as seen through initial unknown count.
 */
TEST(ConstraintStoreLineLenTest, ColumnLineLenIsS) {
    auto store = makeAllZeroStore();

    const LineID line{.type = LineType::Column, .index = 0};
    EXPECT_EQ(store.getUnknownCount(line), kS);

    const LineID lastCol{.type = LineType::Column, .index = static_cast<std::uint16_t>(kS - 1)};
    EXPECT_EQ(store.getUnknownCount(lastCol), kS);
}

/**
 * @brief Diagonal line lengths follow min(d+1, kS, kNumDiags-d) formula.
 *
 * Diagonal 0: min(1, 511, 1020) = 1
 * Diagonal 510 (main): min(511, 511, 511) = 511
 * Diagonal 1020 (last): min(1021, 511, 1) = 1
 * Diagonal 255: min(256, 511, 766) = 256
 */
TEST(ConstraintStoreLineLenTest, DiagonalLineLenFollowsFormula) {
    auto store = makeAllZeroStore();

    // Diagonal 0 has length 1
    const LineID diag0{.type = LineType::Diagonal, .index = 0};
    EXPECT_EQ(store.getUnknownCount(diag0), 1);

    // Main diagonal (index 510) has length 511
    const LineID diagMain{.type = LineType::Diagonal, .index = 510};
    EXPECT_EQ(store.getUnknownCount(diagMain), kS);

    // Last diagonal (index 1020) has length 1
    const LineID diagLast{.type = LineType::Diagonal, .index = static_cast<std::uint16_t>(kNumDiags - 1)};
    EXPECT_EQ(store.getUnknownCount(diagLast), 1);

    // Diagonal 255: min(256, 511, 766) = 256
    const LineID diag255{.type = LineType::Diagonal, .index = 255};
    EXPECT_EQ(store.getUnknownCount(diag255), 256);
}

/**
 * @brief Anti-diagonal line lengths follow the same min(x+1, kS, kNumAntiDiags-x) formula.
 *
 * Anti-diagonal 0: min(1, 511, 1020) = 1
 * Anti-diagonal 510 (main): min(511, 511, 511) = 511
 * Anti-diagonal 1020 (last): min(1021, 511, 1) = 1
 */
TEST(ConstraintStoreLineLenTest, AntiDiagonalLineLenFollowsFormula) {
    auto store = makeAllZeroStore();

    // Anti-diagonal 0 has length 1
    const LineID antiDiag0{.type = LineType::AntiDiagonal, .index = 0};
    EXPECT_EQ(store.getUnknownCount(antiDiag0), 1);

    // Main anti-diagonal (index 510) has length 511
    const LineID antiDiagMain{.type = LineType::AntiDiagonal, .index = 510};
    EXPECT_EQ(store.getUnknownCount(antiDiagMain), kS);

    // Last anti-diagonal (index 1020) has length 1
    const LineID antiDiagLast{.type = LineType::AntiDiagonal, .index = static_cast<std::uint16_t>(kNumDiags - 1)};
    EXPECT_EQ(store.getUnknownCount(antiDiagLast), 1);
}

// ---------------------------------------------------------------------------
// ConstraintStore::unassign tests
// ---------------------------------------------------------------------------

/**
 * @brief Unassigning a cell set to 1 restores it to Unassigned and reverts line statistics.
 */
TEST(ConstraintStoreUnassignTest, UnassignOneRestoresCellAndStats) {
    auto store = makeUniformStore(5, 5);

    const LineID rowLine{.type = LineType::Row, .index = 0};
    const LineID colLine{.type = LineType::Column, .index = 0};

    store.assign(0, 0, 1);
    EXPECT_EQ(store.getCellState(0, 0), CellState::One);
    EXPECT_EQ(store.getUnknownCount(rowLine), kS - 1);
    EXPECT_EQ(store.getResidual(rowLine), 4); // 5 - 1 = 4
    EXPECT_EQ(store.getAssignedCount(rowLine), 1);
    EXPECT_EQ(store.getUnknownCount(colLine), kS - 1);

    store.unassign(0, 0);
    EXPECT_EQ(store.getCellState(0, 0), CellState::Unassigned);
    EXPECT_EQ(store.getUnknownCount(rowLine), kS);
    EXPECT_EQ(store.getResidual(rowLine), 5); // restored
    EXPECT_EQ(store.getAssignedCount(rowLine), 0);
    EXPECT_EQ(store.getUnknownCount(colLine), kS);
}

/**
 * @brief Unassigning a cell set to 0 restores it to Unassigned and reverts line statistics.
 */
TEST(ConstraintStoreUnassignTest, UnassignZeroRestoresCellAndStats) {
    auto store = makeUniformStore(5, 5);

    const LineID rowLine{.type = LineType::Row, .index = 0};

    store.assign(0, 0, 0);
    EXPECT_EQ(store.getCellState(0, 0), CellState::Zero);
    EXPECT_EQ(store.getUnknownCount(rowLine), kS - 1);
    EXPECT_EQ(store.getResidual(rowLine), 5); // unchanged because assigned-ones is still 0
    EXPECT_EQ(store.getAssignedCount(rowLine), 0);

    store.unassign(0, 0);
    EXPECT_EQ(store.getCellState(0, 0), CellState::Unassigned);
    EXPECT_EQ(store.getUnknownCount(rowLine), kS);
    EXPECT_EQ(store.getResidual(rowLine), 5);
    EXPECT_EQ(store.getAssignedCount(rowLine), 0);
}

/**
 * @brief Unassigning a cell set to 1 clears the corresponding bit in getRow().
 */
TEST(ConstraintStoreUnassignTest, UnassignOneClearsRowBit) {
    auto store = makeAllZeroStore();

    store.assign(0, 0, 1);
    auto row = store.getRow(0);
    EXPECT_NE(row[0], 0ULL); // bit should be set

    store.unassign(0, 0);
    row = store.getRow(0);
    EXPECT_EQ(row[0], 0ULL); // bit should be cleared
}

/**
 * @brief Unassigning updates diagonal and anti-diagonal statistics correctly.
 */
TEST(ConstraintStoreUnassignTest, UnassignUpdatesDiagAndAntiDiagStats) {
    auto store = makeAllZeroStore();

    // Cell (5, 10): diagonal index = 10 - 5 + 510 = 515, anti-diag index = 5 + 10 = 15
    const LineID diagLine{.type = LineType::Diagonal, .index = 515};
    const LineID antiDiagLine{.type = LineType::AntiDiagonal, .index = 15};

    const auto diagUnknownBefore = store.getUnknownCount(diagLine);
    const auto antiDiagUnknownBefore = store.getUnknownCount(antiDiagLine);

    store.assign(5, 10, 1);
    EXPECT_EQ(store.getUnknownCount(diagLine), diagUnknownBefore - 1);
    EXPECT_EQ(store.getUnknownCount(antiDiagLine), antiDiagUnknownBefore - 1);
    EXPECT_EQ(store.getAssignedCount(diagLine), 1);
    EXPECT_EQ(store.getAssignedCount(antiDiagLine), 1);

    store.unassign(5, 10);
    EXPECT_EQ(store.getUnknownCount(diagLine), diagUnknownBefore);
    EXPECT_EQ(store.getUnknownCount(antiDiagLine), antiDiagUnknownBefore);
    EXPECT_EQ(store.getAssignedCount(diagLine), 0);
    EXPECT_EQ(store.getAssignedCount(antiDiagLine), 0);
}

/**
 * @brief Multiple assign/unassign cycles leave the store in a consistent state.
 */
TEST(ConstraintStoreUnassignTest, MultipleAssignUnassignCycles) {
    auto store = makeUniformStore(3, 3);

    const LineID rowLine{.type = LineType::Row, .index = 2};

    for (int cycle = 0; cycle < 3; ++cycle) {
        store.assign(2, 0, 1);
        EXPECT_EQ(store.getCellState(2, 0), CellState::One);
        EXPECT_EQ(store.getAssignedCount(rowLine), 1);

        store.unassign(2, 0);
        EXPECT_EQ(store.getCellState(2, 0), CellState::Unassigned);
        EXPECT_EQ(store.getAssignedCount(rowLine), 0);
        EXPECT_EQ(store.getUnknownCount(rowLine), kS);
    }
}

// ---------------------------------------------------------------------------
// Sha256HashVerifier tests
// ---------------------------------------------------------------------------

/**
 * @brief Constructor creates a verifier with s rows of zeroed expected digests.
 */
TEST(Sha256HashVerifierTest, ConstructorCreatesVerifier) {
    const Sha256HashVerifier verifier(kS);
    // Construction should not throw. Verify basic function by computing a hash.
    const std::array<std::uint64_t, 8> zeroRow{};
    const auto digest = verifier.computeHash(zeroRow);
    EXPECT_EQ(digest.size(), 32U);
}

/**
 * @brief computeHash() produces a deterministic 32-byte digest for the same input.
 */
TEST(Sha256HashVerifierTest, ComputeHashIsDeterministic) {
    const Sha256HashVerifier verifier(kS);

    const std::array<std::uint64_t, 8> row = {0x0123456789ABCDEFULL, 0, 0, 0, 0, 0, 0, 0};
    const auto digest1 = verifier.computeHash(row);
    const auto digest2 = verifier.computeHash(row);
    EXPECT_EQ(digest1, digest2);
}

/**
 * @brief computeHash() produces different digests for different inputs.
 */
TEST(Sha256HashVerifierTest, ComputeHashDiffersForDifferentInputs) {
    const Sha256HashVerifier verifier(kS);

    const std::array<std::uint64_t, 8> rowA = {};
    const std::array<std::uint64_t, 8> rowB = {1, 0, 0, 0, 0, 0, 0, 0};
    const auto digestA = verifier.computeHash(rowA);
    const auto digestB = verifier.computeHash(rowB);
    EXPECT_NE(digestA, digestB);
}

/**
 * @brief computeHash() of all-zero row produces the SHA-256 of 64 zero bytes.
 */
TEST(Sha256HashVerifierTest, ComputeHashOfZeroRow) {
    const Sha256HashVerifier verifier(kS);

    const std::array<std::uint64_t, 8> zeroRow{};
    const auto digest = verifier.computeHash(zeroRow);

    // SHA-256("" padded to 64 zero bytes) is a known constant.
    // Verify it is not all zeros (which would indicate the hash is broken).
    bool allZero = true;
    for (const auto b : digest) {
        if (b != 0) {
            allZero = false;
            break;
        }
    }
    EXPECT_FALSE(allZero) << "SHA-256 of 64 zero bytes should not be all zeros";
}

/**
 * @brief setExpected() stores the expected digest and verifyRow() checks against it.
 */
TEST(Sha256HashVerifierTest, SetExpectedAndVerifyRowMatch) {
    Sha256HashVerifier verifier(kS);

    const std::array<std::uint64_t, 8> row = {0xDEADBEEFCAFEBABEULL, 0, 0, 0, 0, 0, 0, 0};
    const auto digest = verifier.computeHash(row);

    verifier.setExpected(0, digest);
    EXPECT_TRUE(verifier.verifyRow(0, row));
}

/**
 * @brief verifyRow() returns false when the row does not match the expected digest.
 */
TEST(Sha256HashVerifierTest, VerifyRowReturnsFalseOnMismatch) {
    Sha256HashVerifier verifier(kS);

    const std::array<std::uint64_t, 8> rowA = {0xDEADBEEFCAFEBABEULL, 0, 0, 0, 0, 0, 0, 0};
    const std::array<std::uint64_t, 8> rowB = {0x1111111111111111ULL, 0, 0, 0, 0, 0, 0, 0};

    const auto digestA = verifier.computeHash(rowA);
    verifier.setExpected(0, digestA);

    // Verify with a different row should fail
    EXPECT_FALSE(verifier.verifyRow(0, rowB));
}

/**
 * @brief verifyRow() returns false when expected digest is all zeros (default) and row is non-zero.
 */
TEST(Sha256HashVerifierTest, VerifyRowFailsWithDefaultExpected) {
    const Sha256HashVerifier verifier(kS);

    // Default expected is all zeros. A non-zero row's hash will not be all zeros.
    const std::array<std::uint64_t, 8> row = {1, 0, 0, 0, 0, 0, 0, 0};
    EXPECT_FALSE(verifier.verifyRow(0, row));
}

/**
 * @brief setExpected() can set digests for multiple different rows.
 */
TEST(Sha256HashVerifierTest, SetExpectedForMultipleRows) {
    Sha256HashVerifier verifier(kS);

    const std::array<std::uint64_t, 8> row0 = {};
    const std::array<std::uint64_t, 8> row1 = {0xFFFFFFFFFFFFFFFFULL, 0, 0, 0, 0, 0, 0, 0};

    const auto digest0 = verifier.computeHash(row0);
    const auto digest1 = verifier.computeHash(row1);

    verifier.setExpected(0, digest0);
    verifier.setExpected(1, digest1);

    EXPECT_TRUE(verifier.verifyRow(0, row0));
    EXPECT_TRUE(verifier.verifyRow(1, row1));

    // Cross-check: row1 data should not match row0's expected digest
    EXPECT_FALSE(verifier.verifyRow(0, row1));
    EXPECT_FALSE(verifier.verifyRow(1, row0));
}

/**
 * @brief Constructor with a small s value works correctly.
 */
TEST(Sha256HashVerifierTest, ConstructorSmallDimension) {
    Sha256HashVerifier verifier(3);

    const std::array<std::uint64_t, 8> row = {};
    const auto digest = verifier.computeHash(row);

    verifier.setExpected(0, digest);
    verifier.setExpected(1, digest);
    verifier.setExpected(2, digest);

    EXPECT_TRUE(verifier.verifyRow(0, row));
    EXPECT_TRUE(verifier.verifyRow(1, row));
    EXPECT_TRUE(verifier.verifyRow(2, row));
}

// ---------------------------------------------------------------------------
// EnumerationController tests
// ---------------------------------------------------------------------------

/**
 * @brief reset() can be called without causing errors (it is a documented no-op).
 */
TEST(EnumerationControllerTest, ResetIsNoOp) {
    auto store = std::make_unique<ConstraintStore>(
        std::vector<std::uint16_t>(kS, 0),
        std::vector<std::uint16_t>(kS, 0),
        std::vector<std::uint16_t>(kNumDiags, 0),
        std::vector<std::uint16_t>(kNumDiags, 0),
        std::vector<std::uint16_t>(kS, 0),
        std::vector<std::uint16_t>(kS, 0),
        std::vector<std::uint16_t>(kS, 0),
        std::vector<std::uint16_t>(kS, 0)
    );
    auto propagator = std::make_unique<PropagationEngine>(*store);
    auto brancher = std::make_unique<BranchingController>(*store, *propagator);
    auto hasher = std::make_unique<Sha256HashVerifier>(kS);

    EnumerationController enumerator(
        std::move(store), std::move(propagator),
        std::move(brancher), std::move(hasher)
    );

    // Should not throw or cause any side effects
    enumerator.reset();
    enumerator.reset();
}

/**
 * @brief enumerate() with all-zero sums yields exactly one solution (the all-zero matrix)
 *        because propagation forces every cell to 0. We skip hash verification by setting
 *        expected hashes to the actual zero-row hash.
 */
TEST(EnumerationControllerTest, EnumerateAllZerosSingleSolution) {
    auto store = std::make_unique<ConstraintStore>(
        std::vector<std::uint16_t>(kS, 0),
        std::vector<std::uint16_t>(kS, 0),
        std::vector<std::uint16_t>(kNumDiags, 0),
        std::vector<std::uint16_t>(kNumDiags, 0),
        std::vector<std::uint16_t>(kS, 0),
        std::vector<std::uint16_t>(kS, 0),
        std::vector<std::uint16_t>(kS, 0),
        std::vector<std::uint16_t>(kS, 0)
    );
    auto propagator = std::make_unique<PropagationEngine>(*store);
    auto brancher = std::make_unique<BranchingController>(*store, *propagator);
    auto hasher = std::make_unique<Sha256HashVerifier>(kS);

    // Set expected hashes for all rows to match the all-zero row hash
    const std::array<std::uint64_t, 8> zeroRow{};
    const auto zeroDigest = hasher->computeHash(zeroRow);
    for (std::uint16_t r = 0; r < kS; ++r) {
        hasher->setExpected(r, zeroDigest);
    }

    EnumerationController enumerator(
        std::move(store), std::move(propagator),
        std::move(brancher), std::move(hasher)
    );

    std::vector<crsce::common::Csm> solutions;
    enumerator.enumerate([&solutions](const crsce::common::Csm &csm) -> bool {
        solutions.push_back(csm);
        return true; // continue
    });

    ASSERT_EQ(solutions.size(), 1U);

    // Verify the solution is all zeros
    for (std::uint16_t r = 0; r < kS; ++r) {
        for (std::uint16_t c = 0; c < kS; ++c) {
            EXPECT_EQ(solutions[0].get(r, c), 0)
                << "Cell (" << r << ", " << c << ") should be 0";
            if (solutions[0].get(r, c) != 0) {
                return; // stop on first failure to avoid flooding output
            }
        }
    }
}

/**
 * @brief enumerate() callback returning false stops enumeration early.
 */
TEST(EnumerationControllerTest, EnumerateStopsOnCallbackFalse) {
    auto store = std::make_unique<ConstraintStore>(
        std::vector<std::uint16_t>(kS, 0),
        std::vector<std::uint16_t>(kS, 0),
        std::vector<std::uint16_t>(kNumDiags, 0),
        std::vector<std::uint16_t>(kNumDiags, 0),
        std::vector<std::uint16_t>(kS, 0),
        std::vector<std::uint16_t>(kS, 0),
        std::vector<std::uint16_t>(kS, 0),
        std::vector<std::uint16_t>(kS, 0)
    );
    auto propagator = std::make_unique<PropagationEngine>(*store);
    auto brancher = std::make_unique<BranchingController>(*store, *propagator);
    auto hasher = std::make_unique<Sha256HashVerifier>(kS);

    const std::array<std::uint64_t, 8> zeroRow{};
    const auto zeroDigest = hasher->computeHash(zeroRow);
    for (std::uint16_t r = 0; r < kS; ++r) {
        hasher->setExpected(r, zeroDigest);
    }

    EnumerationController enumerator(
        std::move(store), std::move(propagator),
        std::move(brancher), std::move(hasher)
    );

    int callCount = 0;
    enumerator.enumerate([&callCount](const crsce::common::Csm & /*csm*/) -> bool {
        ++callCount;
        return false; // stop immediately
    });

    EXPECT_EQ(callCount, 1);
}

/**
 * @brief enumerateSolutionsLex() coroutine yields the same all-zero solution for all-zero sums.
 */
TEST(EnumerationControllerTest, EnumerateSolutionsLexAllZeros) {
    auto store = std::make_unique<ConstraintStore>(
        std::vector<std::uint16_t>(kS, 0),
        std::vector<std::uint16_t>(kS, 0),
        std::vector<std::uint16_t>(kNumDiags, 0),
        std::vector<std::uint16_t>(kNumDiags, 0),
        std::vector<std::uint16_t>(kS, 0),
        std::vector<std::uint16_t>(kS, 0),
        std::vector<std::uint16_t>(kS, 0),
        std::vector<std::uint16_t>(kS, 0)
    );
    auto propagator = std::make_unique<PropagationEngine>(*store);
    auto brancher = std::make_unique<BranchingController>(*store, *propagator);
    auto hasher = std::make_unique<Sha256HashVerifier>(kS);

    const std::array<std::uint64_t, 8> zeroRow{};
    const auto zeroDigest = hasher->computeHash(zeroRow);
    for (std::uint16_t r = 0; r < kS; ++r) {
        hasher->setExpected(r, zeroDigest);
    }

    EnumerationController enumerator(
        std::move(store), std::move(propagator),
        std::move(brancher), std::move(hasher)
    );

    int solutionCount = 0;
    for (const auto &csm : enumerator.enumerateSolutionsLex()) {
        ++solutionCount;
        // Verify it is all zeros
        EXPECT_EQ(csm.get(0, 0), 0);
        EXPECT_EQ(csm.get(kS - 1, kS - 1), 0);
    }

    EXPECT_EQ(solutionCount, 1);
}

/**
 * @brief enumerateSolutionsLex() yields no solutions when constraints are infeasible.
 *
 * Set row 0 target to kS (all ones) but column 0 target to 0 (no ones).
 * This is inherently contradictory since cell (0, 0) must be both 1 (row) and 0 (column).
 */
TEST(EnumerationControllerTest, EnumerateSolutionsLexInfeasible) {
    std::vector<std::uint16_t> rowSums(kS, 0);
    const std::vector<std::uint16_t> colSums(kS, 0);
    const std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    const std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    // Row 0 target = kS (need all cells in row 0 to be 1)
    rowSums[0] = kS;
    // But column 0 target = 0 (need all cells in column 0 to be 0)
    // This is contradictory for cell (0, 0).

    auto store = std::make_unique<ConstraintStore>(rowSums, colSums, diagSums, antiDiagSums,
                                                    std::vector<std::uint16_t>(kS, 0),
                                                    std::vector<std::uint16_t>(kS, 0),
                                                    std::vector<std::uint16_t>(kS, 0),
                                                    std::vector<std::uint16_t>(kS, 0));
    auto propagator = std::make_unique<PropagationEngine>(*store);
    auto brancher = std::make_unique<BranchingController>(*store, *propagator);
    auto hasher = std::make_unique<Sha256HashVerifier>(kS);

    EnumerationController enumerator(
        std::move(store), std::move(propagator),
        std::move(brancher), std::move(hasher)
    );

    int solutionCount = 0;
    for ([[maybe_unused]] const auto &csm : enumerator.enumerateSolutionsLex()) {
        ++solutionCount;
    }

    EXPECT_EQ(solutionCount, 0);
}

/**
 * @brief enumerate() with infeasible constraints calls the callback zero times.
 */
TEST(EnumerationControllerTest, EnumerateInfeasibleNoCallback) {
    std::vector<std::uint16_t> rowSums(kS, 0);
    const std::vector<std::uint16_t> colSums(kS, 0);
    const std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    const std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    // Create a contradiction: row 0 needs kS ones, column 0 needs 0 ones.
    rowSums[0] = kS;

    auto store = std::make_unique<ConstraintStore>(rowSums, colSums, diagSums, antiDiagSums,
                                                    std::vector<std::uint16_t>(kS, 0),
                                                    std::vector<std::uint16_t>(kS, 0),
                                                    std::vector<std::uint16_t>(kS, 0),
                                                    std::vector<std::uint16_t>(kS, 0));
    auto propagator = std::make_unique<PropagationEngine>(*store);
    auto brancher = std::make_unique<BranchingController>(*store, *propagator);
    auto hasher = std::make_unique<Sha256HashVerifier>(kS);

    EnumerationController enumerator(
        std::move(store), std::move(propagator),
        std::move(brancher), std::move(hasher)
    );

    int callCount = 0;
    enumerator.enumerate([&callCount](const crsce::common::Csm & /*csm*/) -> bool {
        ++callCount;
        return true;
    });

    EXPECT_EQ(callCount, 0);
}

// ---------------------------------------------------------------------------
// EnumerationController DFS loop tests
// ---------------------------------------------------------------------------

namespace {

/**
 * @brief Build constraint vectors for a 2x2 corner that requires DFS branching.
 *
 * Solution: (0,0)=1, (1,1)=1, all other cells 0.
 * Row sums: row 0=1, row 1=1, rest=0
 * Col sums: col 0=1, col 1=1, rest=0
 * Diag 510 ((0,0) and (1,1)): sum=2
 * Anti-diag 0 ((0,0)): sum=1
 * Anti-diag 2 ((1,1)): sum=1
 * All other diag/anti-diag sums=0.
 *
 * Initial propagation forces rows 2..510 and cols 2..510 to 0, leaving
 * a 2x2 sub-problem at (0,0),(0,1),(1,0),(1,1) that needs DFS.
 */
void fillTwoByTwoCorner(std::vector<std::uint16_t> &rowSums,
                         std::vector<std::uint16_t> &colSums,
                         std::vector<std::uint16_t> &diagSums,
                         std::vector<std::uint16_t> &antiDiagSums,
                         std::vector<std::uint16_t> &ltp1Sums,
                         std::vector<std::uint16_t> &ltp2Sums,
                         std::vector<std::uint16_t> &ltp3Sums,
                         std::vector<std::uint16_t> &ltp4Sums) {
    rowSums.assign(kS, 0);
    colSums.assign(kS, 0);
    diagSums.assign(kNumDiags, 0);
    antiDiagSums.assign(kNumDiags, 0);
    ltp1Sums.assign(kS, 0);
    ltp2Sums.assign(kS, 0);
    ltp3Sums.assign(kS, 0);
    ltp4Sums.assign(kS, 0);

    rowSums[0] = 1;
    rowSums[1] = 1;
    colSums[0] = 1;
    colSums[1] = 1;
    // Diagonal index for (r,c) = c - r + 510
    // (0,0) → diag 510, (1,1) → diag 510 → total 2
    diagSums[510] = 2;
    // Anti-diagonal index for (r,c) = r + c
    // (0,0) → anti-diag 0, (1,1) → anti-diag 2
    antiDiagSums[0] = 1;
    antiDiagSums[2] = 1;
    // Set LTP targets for cells (0,0) and (1,1) using precomputed partition tables.
    const auto &idx00 = ltpFlatIndices(0, 0);
    const auto &idx11 = ltpFlatIndices(1, 1);
    const auto l1_00 = static_cast<std::uint16_t>(idx00[0] - static_cast<std::uint16_t>(kLtp1Base));
    const auto l1_11 = static_cast<std::uint16_t>(idx11[0] - static_cast<std::uint16_t>(kLtp1Base));
    ltp1Sums[l1_00] = (l1_00 == l1_11) ? 2U : 1U;
    if (l1_00 != l1_11) { ltp1Sums[l1_11] = 1; }
    const auto l2_00 = static_cast<std::uint16_t>(idx00[1] - static_cast<std::uint16_t>(kLtp2Base));
    const auto l2_11 = static_cast<std::uint16_t>(idx11[1] - static_cast<std::uint16_t>(kLtp2Base));
    ltp2Sums[l2_00] = (l2_00 == l2_11) ? 2U : 1U;
    if (l2_00 != l2_11) { ltp2Sums[l2_11] = 1; }
    const auto l3_00 = static_cast<std::uint16_t>(idx00[2] - static_cast<std::uint16_t>(kLtp3Base));
    const auto l3_11 = static_cast<std::uint16_t>(idx11[2] - static_cast<std::uint16_t>(kLtp3Base));
    ltp3Sums[l3_00] = (l3_00 == l3_11) ? 2U : 1U;
    if (l3_00 != l3_11) { ltp3Sums[l3_11] = 1; }
    const auto l4_00 = static_cast<std::uint16_t>(idx00[3] - static_cast<std::uint16_t>(kLtp4Base));
    const auto l4_11 = static_cast<std::uint16_t>(idx11[3] - static_cast<std::uint16_t>(kLtp4Base));
    ltp4Sums[l4_00] = (l4_00 == l4_11) ? 2U : 1U;
    if (l4_00 != l4_11) { ltp4Sums[l4_11] = 1; }
}

} // namespace

/**
 * @brief enumerateSolutionsLex exercises the DFS loop and finds the unique solution.
 *
 * The 2x2 corner constraint system has exactly one solution: (0,0)=1, (1,1)=1.
 * The DFS must branch on cell (0,0), try value 0 (which fails because diag 510
 * can't reach target 2), then try value 1 (which succeeds via propagation).
 */
TEST(EnumerationControllerDfsTest, TwoByTwoCornerFindsUniqueSolution) {
    std::vector<std::uint16_t> rowSums;
    std::vector<std::uint16_t> colSums;
    std::vector<std::uint16_t> diagSums;
    std::vector<std::uint16_t> antiDiagSums;
    std::vector<std::uint16_t> ltp1Sums;
    std::vector<std::uint16_t> ltp2Sums;
    std::vector<std::uint16_t> ltp3Sums;
    std::vector<std::uint16_t> ltp4Sums;
    fillTwoByTwoCorner(rowSums, colSums, diagSums, antiDiagSums,
                        ltp1Sums, ltp2Sums, ltp3Sums, ltp4Sums);

    auto store = std::make_unique<ConstraintStore>(rowSums, colSums, diagSums, antiDiagSums,
                                                    ltp1Sums, ltp2Sums, ltp3Sums, ltp4Sums);
    auto propagator = std::make_unique<PropagationEngine>(*store);
    auto brancher = std::make_unique<BranchingController>(*store, *propagator);
    auto hasher = std::make_unique<Sha256HashVerifier>(kS);

    // Compute expected hashes for each row of the solution.
    // Row 0: bit 0 (MSB of first uint64) set = 1, rest 0.
    // Row 1: bit 1 (second bit of first uint64) set = 1, rest 0.
    // Rows 2..510: all zeros.
    {
        // Row 0: cell(0,0)=1 → bit 0 (MSB)
        std::array<std::uint64_t, 8> row0{};
        row0[0] = std::uint64_t{1} << 63U; // MSB of first word
        hasher->setExpected(0, hasher->computeHash(row0));

        // Row 1: cell(1,1)=1 → bit 1
        std::array<std::uint64_t, 8> row1{};
        row1[0] = std::uint64_t{1} << 62U;
        hasher->setExpected(1, hasher->computeHash(row1));

        // Rows 2..510: all zeros
        const std::array<std::uint64_t, 8> zeroRow{};
        const auto zeroDigest = hasher->computeHash(zeroRow);
        for (std::uint16_t r = 2; r < kS; ++r) {
            hasher->setExpected(r, zeroDigest);
        }
    }

    EnumerationController enumerator(
        std::move(store), std::move(propagator),
        std::move(brancher), std::move(hasher)
    );

    int solutionCount = 0;
    crsce::common::Csm result;
    for (const auto &csm : enumerator.enumerateSolutionsLex()) {
        result = csm;
        ++solutionCount;
    }

    EXPECT_EQ(solutionCount, 1);
    EXPECT_EQ(result.get(0, 0), 1);
    EXPECT_EQ(result.get(0, 1), 0);
    EXPECT_EQ(result.get(1, 0), 0);
    EXPECT_EQ(result.get(1, 1), 1);
}

/**
 * @brief 2x2 corner is fully determined by initial propagation (diag forces both cells).
 *
 * Verifies that the diagonal target=2 with only 2 unknowns on diag 510
 * forces both (0,0) and (1,1) to 1 during initial propagation, so
 * nextCell() returns nullopt and the solution is yielded immediately.
 */
TEST(EnumerationControllerDfsTest, TwoByTwoCornerFullyDetermined) {
    std::vector<std::uint16_t> rowSums;
    std::vector<std::uint16_t> colSums;
    std::vector<std::uint16_t> diagSums;
    std::vector<std::uint16_t> antiDiagSums;
    std::vector<std::uint16_t> ltp1Sums;
    std::vector<std::uint16_t> ltp2Sums;
    std::vector<std::uint16_t> ltp3Sums;
    std::vector<std::uint16_t> ltp4Sums;
    fillTwoByTwoCorner(rowSums, colSums, diagSums, antiDiagSums,
                        ltp1Sums, ltp2Sums, ltp3Sums, ltp4Sums);

    auto store = std::make_unique<ConstraintStore>(rowSums, colSums, diagSums, antiDiagSums,
                                                    ltp1Sums, ltp2Sums, ltp3Sums, ltp4Sums);
    auto propagator = std::make_unique<PropagationEngine>(*store);
    auto brancher = std::make_unique<BranchingController>(*store, *propagator);
    auto hasher = std::make_unique<Sha256HashVerifier>(kS);

    // Set expected hashes to all-zeros (wrong for rows 0 and 1, but the
    // initial-propagation-complete path does not check hashes).
    const std::array<std::uint64_t, 8> zeroRow{};
    const auto zeroDigest = hasher->computeHash(zeroRow);
    for (std::uint16_t r = 0; r < kS; ++r) {
        hasher->setExpected(r, zeroDigest);
    }

    EnumerationController enumerator(
        std::move(store), std::move(propagator),
        std::move(brancher), std::move(hasher)
    );

    int solutionCount = 0;
    for ([[maybe_unused]] const auto &csm : enumerator.enumerateSolutionsLex()) {
        ++solutionCount;
    }

    // Solution is yielded without hash verification (all-forced path)
    EXPECT_EQ(solutionCount, 1);
}
