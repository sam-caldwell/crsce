/**
 * @file constraint_store_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Unit tests for the ConstraintStore class.
 */
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/LineID.h"

using crsce::decompress::solvers::CellState;
using crsce::decompress::solvers::ConstraintStore;
using crsce::decompress::solvers::LineID;
using crsce::decompress::solvers::LineType;

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
     * @brief Helper to build a ConstraintStore with specific row/col sums and zero diag sums.
     * @param rowSumVal Value for every row sum.
     * @param colSumVal Value for every column sum.
     * @return A ConstraintStore with uniform row/col sums.
     */
    ConstraintStore makeUniformStore(const std::uint16_t rowSumVal, const std::uint16_t colSumVal) {
        const std::vector<std::uint16_t> rowSums(kS, rowSumVal);
        const std::vector<std::uint16_t> colSums(kS, colSumVal);
        const std::vector<std::uint16_t> diagSums(kNumDiags, 0);
        const std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
        return {rowSums, colSums, diagSums, antiDiagSums,
                std::vector<std::uint16_t>(kS, 0), std::vector<std::uint16_t>(kS, 0),
                std::vector<std::uint16_t>(kS, 0), std::vector<std::uint16_t>(kS, 0)};
    }
} // namespace

/**
 * @brief After construction with all-zero sums, every cell starts as Unassigned.
 */
TEST(ConstraintStoreTest, AllCellsStartUnassigned) {
    auto store = makeAllZeroStore();

    // Spot-check a few cells
    EXPECT_EQ(store.getCellState(0, 0), CellState::Unassigned);
    EXPECT_EQ(store.getCellState(0, 510), CellState::Unassigned);
    EXPECT_EQ(store.getCellState(255, 255), CellState::Unassigned);
    EXPECT_EQ(store.getCellState(510, 510), CellState::Unassigned);
}

/**
 * @brief After construction, row unknown counts equal kS for all rows.
 */
TEST(ConstraintStoreTest, InitialRowUnknownCountIsS) {
    auto store = makeAllZeroStore();

    EXPECT_EQ(store.getRowUnknownCount(0), kS);
    EXPECT_EQ(store.getRowUnknownCount(255), kS);
    EXPECT_EQ(store.getRowUnknownCount(510), kS);
}

/**
 * @brief Assigning a cell to 1 changes its state to One.
 */
TEST(ConstraintStoreTest, AssignOneSetsCellToOne) {
    auto store = makeAllZeroStore();
    store.assign(0, 0, 1);
    EXPECT_EQ(store.getCellState(0, 0), CellState::One);
}

/**
 * @brief Assigning a cell to 0 changes its state to Zero.
 */
TEST(ConstraintStoreTest, AssignZeroSetsCellToZero) {
    auto store = makeAllZeroStore();
    store.assign(0, 0, 0);
    EXPECT_EQ(store.getCellState(0, 0), CellState::Zero);
}

/**
 * @brief After assigning (r,c)=1, the residual on the affected row decreases by 1
 *        (relative to target, because a(L) increases by 1 and rho = target - a).
 */
TEST(ConstraintStoreTest, AssignOneDecreasesResidual) {
    // Use row target = 5 so residual starts at 5
    auto store = makeUniformStore(5, 0);

    const LineID rowLine{.type = LineType::Row, .index = 0};
    EXPECT_EQ(store.getResidual(rowLine), 5);

    store.assign(0, 0, 1);
    EXPECT_EQ(store.getResidual(rowLine), 4);
}

/**
 * @brief After assigning (r,c)=0, the residual on the affected row does not change
 *        (because a(L) is not incremented for zero assignments).
 */
TEST(ConstraintStoreTest, AssignZeroDoesNotChangeResidual) {
    auto store = makeUniformStore(5, 0);

    const LineID rowLine{.type = LineType::Row, .index = 0};
    EXPECT_EQ(store.getResidual(rowLine), 5);

    store.assign(0, 0, 0);
    EXPECT_EQ(store.getResidual(rowLine), 5);
}

/**
 * @brief After assigning a cell, the unknown count on the affected row decreases by 1.
 */
TEST(ConstraintStoreTest, AssignDecreasesUnknownCount) {
    auto store = makeAllZeroStore();

    const LineID rowLine{.type = LineType::Row, .index = 0};
    EXPECT_EQ(store.getUnknownCount(rowLine), kS);

    store.assign(0, 0, 1);
    EXPECT_EQ(store.getUnknownCount(rowLine), kS - 1);

    store.assign(0, 1, 0);
    EXPECT_EQ(store.getUnknownCount(rowLine), kS - 2);
}

/**
 * @brief getRowUnknownCount agrees with getUnknownCount for the same row.
 */
TEST(ConstraintStoreTest, GetRowUnknownCountConsistentWithGetUnknownCount) {
    auto store = makeAllZeroStore();

    store.assign(3, 0, 1);
    store.assign(3, 1, 0);

    const LineID rowLine{.type = LineType::Row, .index = 3};
    EXPECT_EQ(store.getRowUnknownCount(3), store.getUnknownCount(rowLine));
    EXPECT_EQ(store.getRowUnknownCount(3), kS - 2);
}

/**
 * @brief getCellValue returns the correct value after assignment.
 */
TEST(ConstraintStoreTest, GetCellValueReturnsAssignedValue) {
    auto store = makeAllZeroStore();

    store.assign(10, 20, 1);
    EXPECT_EQ(store.getCellValue(10, 20), 1);

    store.assign(10, 21, 0);
    EXPECT_EQ(store.getCellValue(10, 21), 0);
}

/**
 * @brief getRow returns zero words when no bits are assigned to 1.
 */
TEST(ConstraintStoreTest, GetRowAllZeroes) {
    auto store = makeAllZeroStore();

    // Assign some cells to 0 (not 1); row bits should remain zero.
    store.assign(0, 0, 0);
    store.assign(0, 63, 0);

    const auto row = store.getRow(0);
    for (const auto word : row) {
        EXPECT_EQ(word, 0ULL);
    }
}

/**
 * @brief getRow correctly sets bits when cells are assigned to 1.
 *
 * Column c maps to word c/64, bit position 63-(c%64) within that word.
 */
TEST(ConstraintStoreTest, GetRowSetsBitsCorrectly) {
    auto store = makeAllZeroStore();

    // Assign column 0 to 1: word 0, bit 63
    store.assign(0, 0, 1);
    auto row = store.getRow(0);
    EXPECT_EQ(row[0], static_cast<std::uint64_t>(1) << 63);

    // Assign column 63 to 1: word 0, bit 0
    store.assign(0, 63, 1);
    row = store.getRow(0);
    EXPECT_EQ(row[0], (static_cast<std::uint64_t>(1) << 63) | 1ULL);

    // Assign column 64 to 1: word 1, bit 63
    store.assign(0, 64, 1);
    row = store.getRow(0);
    EXPECT_EQ(row[1], static_cast<std::uint64_t>(1) << 63);
}

/**
 * @brief getLinesForCell returns exactly 8 lines (row, col, diag, anti-diag, LTP1-4).
 */
TEST(ConstraintStoreTest, GetLinesForCellReturnsEightLines) {
    auto store = makeAllZeroStore();
    const auto lines = store.getLinesForCell(0, 0);
    EXPECT_EQ(lines.size(), 8U);
}

/**
 * @brief getLinesForCell returns the correct line types and indices for cell (0, 0).
 *
 * For cell (0,0):
 *   Row index = 0
 *   Column index = 0
 *   Diagonal index = c - r + (kS-1) = 0 - 0 + 510 = 510
 *   Anti-diagonal index = r + c = 0
 */
TEST(ConstraintStoreTest, GetLinesForCellCorrectForOrigin) {
    auto store = makeAllZeroStore();
    const auto lines = store.getLinesForCell(0, 0);

    EXPECT_EQ(lines[0].type, LineType::Row);
    EXPECT_EQ(lines[0].index, 0);

    EXPECT_EQ(lines[1].type, LineType::Column);
    EXPECT_EQ(lines[1].index, 0);

    EXPECT_EQ(lines[2].type, LineType::Diagonal);
    EXPECT_EQ(lines[2].index, 510);

    EXPECT_EQ(lines[3].type, LineType::AntiDiagonal);
    EXPECT_EQ(lines[3].index, 0);
}

/**
 * @brief getLinesForCell returns the correct indices for cell (5, 10).
 *
 * For cell (5,10):
 *   Row = 5, Column = 10
 *   Diagonal = 10 - 5 + 510 = 515
 *   Anti-diagonal = 5 + 10 = 15
 */
TEST(ConstraintStoreTest, GetLinesForCellCorrectForArbitraryCell) {
    auto store = makeAllZeroStore();
    const auto lines = store.getLinesForCell(5, 10);

    EXPECT_EQ(lines[0].type, LineType::Row);
    EXPECT_EQ(lines[0].index, 5);

    EXPECT_EQ(lines[1].type, LineType::Column);
    EXPECT_EQ(lines[1].index, 10);

    EXPECT_EQ(lines[2].type, LineType::Diagonal);
    EXPECT_EQ(lines[2].index, 515);

    EXPECT_EQ(lines[3].type, LineType::AntiDiagonal);
    EXPECT_EQ(lines[3].index, 15);
}

/**
 * @brief Column unknown count decreases when cells are assigned in the same column.
 */
TEST(ConstraintStoreTest, ColumnUnknownCountDecreases) {
    auto store = makeAllZeroStore();

    const LineID colLine{.type = LineType::Column, .index = 5};
    EXPECT_EQ(store.getUnknownCount(colLine), kS);

    store.assign(0, 5, 1);
    EXPECT_EQ(store.getUnknownCount(colLine), kS - 1);

    store.assign(1, 5, 0);
    EXPECT_EQ(store.getUnknownCount(colLine), kS - 2);
}

/**
 * @brief Diagonal unknown count decreases when a cell on that diagonal is assigned.
 */
TEST(ConstraintStoreTest, DiagonalUnknownCountDecreases) {
    auto store = makeAllZeroStore();

    // Cell (0, 0) is on diagonal index 510 (c - r + 510 = 510).
    // The main diagonal (index 510) has length kS = 511.
    const LineID diagLine{.type = LineType::Diagonal, .index = 510};
    EXPECT_EQ(store.getUnknownCount(diagLine), kS);

    store.assign(0, 0, 1);
    EXPECT_EQ(store.getUnknownCount(diagLine), kS - 1);
}

/**
 * @brief Anti-diagonal unknown count decreases when a cell on that anti-diagonal is assigned.
 */
TEST(ConstraintStoreTest, AntiDiagonalUnknownCountDecreases) {
    auto store = makeAllZeroStore();

    // Cell (0, 0) is on anti-diagonal index 0. That anti-diagonal has length 1.
    const LineID antiDiagLine{.type = LineType::AntiDiagonal, .index = 0};
    EXPECT_EQ(store.getUnknownCount(antiDiagLine), 1);

    store.assign(0, 0, 1);
    EXPECT_EQ(store.getUnknownCount(antiDiagLine), 0);
}

/**
 * @brief getAssignedCount returns the number of cells assigned to 1 on a line.
 */
TEST(ConstraintStoreTest, GetAssignedCountTracksOnes) {
    auto store = makeAllZeroStore();

    const LineID rowLine{.type = LineType::Row, .index = 0};
    EXPECT_EQ(store.getAssignedCount(rowLine), 0);

    store.assign(0, 0, 1);
    EXPECT_EQ(store.getAssignedCount(rowLine), 1);

    store.assign(0, 1, 0);
    EXPECT_EQ(store.getAssignedCount(rowLine), 1); // zero assignment does not increment

    store.assign(0, 2, 1);
    EXPECT_EQ(store.getAssignedCount(rowLine), 2);
}
