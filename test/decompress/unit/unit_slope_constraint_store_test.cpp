/**
 * @file unit_slope_constraint_store_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Unit tests for slope-related functionality in the ConstraintStore class.
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/LineID.h"

using crsce::decompress::solvers::ConstraintStore;
using crsce::decompress::solvers::LineType;

namespace {
    constexpr std::uint16_t kS = 511;
    constexpr std::uint16_t kNumDiags = (2 * kS) - 1;

    constexpr std::uint32_t kSlope256Base = 3064;
    constexpr std::uint32_t kSlope255Base = 3575;
    constexpr std::uint32_t kSlope2Base = 4086;
    constexpr std::uint32_t kSlope509Base = 4597;

    /**
     * @name makeAllZeroStore
     * @brief Helper to build a ConstraintStore with all-zero sum vectors.
     * @return A ConstraintStore where every target sum is 0.
     */
    ConstraintStore makeAllZeroStore() {
        const std::vector<std::uint16_t> rowSums(kS, 0);
        const std::vector<std::uint16_t> colSums(kS, 0);
        const std::vector<std::uint16_t> diagSums(kNumDiags, 0);
        const std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
        const std::vector<std::uint16_t> slope256Sums(kS, 0);
        const std::vector<std::uint16_t> slope255Sums(kS, 0);
        const std::vector<std::uint16_t> slope2Sums(kS, 0);
        const std::vector<std::uint16_t> slope509Sums(kS, 0);
        return {rowSums, colSums, diagSums, antiDiagSums,
                slope256Sums, slope255Sums, slope2Sums, slope509Sums};
    }
} // namespace

/**
 * @name AssignUpdatesSlopeStats
 * @brief Assigning cell (0,0,1) decrements unknown and increments assigned on all 4 slope stats.
 *
 * For cell (0,0), all four slope line indices are 0:
 *   slope 256: ((0 - 256*0) % 511 + 511) % 511 = 0
 *   slope 255: ((0 - 255*0) % 511 + 511) % 511 = 0
 *   slope 2:   ((0 - 2*0)   % 511 + 511) % 511 = 0
 *   slope 509: ((0 - 509*0) % 511 + 511) % 511 = 0
 */
TEST(SlopeConstraintStoreTest, AssignUpdatesSlopeStats) {
    auto store = makeAllZeroStore();
    store.assign(0, 0, 1);

    const auto &stat256 = store.getStatDirect(kSlope256Base + 0);
    EXPECT_EQ(stat256.unknown, 510);
    EXPECT_EQ(stat256.assigned, 1);

    const auto &stat255 = store.getStatDirect(kSlope255Base + 0);
    EXPECT_EQ(stat255.unknown, 510);
    EXPECT_EQ(stat255.assigned, 1);

    const auto &stat2 = store.getStatDirect(kSlope2Base + 0);
    EXPECT_EQ(stat2.unknown, 510);
    EXPECT_EQ(stat2.assigned, 1);

    const auto &stat509 = store.getStatDirect(kSlope509Base + 0);
    EXPECT_EQ(stat509.unknown, 510);
    EXPECT_EQ(stat509.assigned, 1);
}

/**
 * @name UnassignRestoresSlopeStats
 * @brief Assigning then unassigning cell (0,0) restores all 4 slope stats to initial values.
 */
TEST(SlopeConstraintStoreTest, UnassignRestoresSlopeStats) {
    auto store = makeAllZeroStore();
    store.assign(0, 0, 1);
    store.unassign(0, 0);

    const auto &stat256 = store.getStatDirect(kSlope256Base + 0);
    EXPECT_EQ(stat256.unknown, kS);
    EXPECT_EQ(stat256.assigned, 0);

    const auto &stat255 = store.getStatDirect(kSlope255Base + 0);
    EXPECT_EQ(stat255.unknown, kS);
    EXPECT_EQ(stat255.assigned, 0);

    const auto &stat2 = store.getStatDirect(kSlope2Base + 0);
    EXPECT_EQ(stat2.unknown, kS);
    EXPECT_EQ(stat2.assigned, 0);

    const auto &stat509 = store.getStatDirect(kSlope509Base + 0);
    EXPECT_EQ(stat509.unknown, kS);
    EXPECT_EQ(stat509.assigned, 0);
}

/**
 * @name GetLinesForCellReturnsCorrectSlopeTypes
 * @brief getLinesForCell(0,0) returns Slope256, Slope255, Slope2, Slope509 in positions [4..7].
 */
TEST(SlopeConstraintStoreTest, GetLinesForCellReturnsCorrectSlopeTypes) {
    auto store = makeAllZeroStore();
    const auto lines = store.getLinesForCell(0, 0);

    EXPECT_EQ(lines[4].type, LineType::Slope256);
    EXPECT_EQ(lines[5].type, LineType::Slope255);
    EXPECT_EQ(lines[6].type, LineType::Slope2);
    EXPECT_EQ(lines[7].type, LineType::Slope509);
}

/**
 * @name SlopeLineIndicesAreCorrect
 * @brief For cell (1,2), verify getLinesForCell returns the correct slope line indices.
 *
 * Expected slope line indices for (r=1, c=2):
 *   slope 256: ((2 - 256*1) % 511 + 511) % 511 = ((-254) % 511 + 511) % 511 = 257
 *   slope 255: ((2 - 255*1) % 511 + 511) % 511 = ((-253) % 511 + 511) % 511 = 258
 *   slope 2:   ((2 - 2*1)   % 511 + 511) % 511 = ((0)    % 511 + 511) % 511 = 0
 *   slope 509: ((2 - 509*1) % 511 + 511) % 511 = ((-507) % 511 + 511) % 511 = 4
 */
TEST(SlopeConstraintStoreTest, SlopeLineIndicesAreCorrect) {
    auto store = makeAllZeroStore();
    const auto lines = store.getLinesForCell(1, 2);

    EXPECT_EQ(lines[4].type, LineType::Slope256);
    EXPECT_EQ(lines[4].index, 257);

    EXPECT_EQ(lines[5].type, LineType::Slope255);
    EXPECT_EQ(lines[5].index, 258);

    EXPECT_EQ(lines[6].type, LineType::Slope2);
    EXPECT_EQ(lines[6].index, 0);

    EXPECT_EQ(lines[7].type, LineType::Slope509);
    EXPECT_EQ(lines[7].index, 4);
}

/**
 * @name AssignMultipleCellsSameSlopeLine
 * @brief Assigning two cells on the same slope-256 line accumulates stats correctly.
 *
 * Cells (0,0) and (1,256) both have slope-256 line index 0:
 *   (0,0):   ((0 - 256*0) % 511 + 511) % 511 = 0
 *   (1,256): ((256 - 256*1) % 511 + 511) % 511 = 0
 *
 * After assigning both to 1, the slope-256 line 0 should have unknown=509, assigned=2.
 */
TEST(SlopeConstraintStoreTest, AssignMultipleCellsSameSlopeLine) {
    auto store = makeAllZeroStore();
    store.assign(0, 0, 1);
    store.assign(1, 256, 1);

    const auto &stat256 = store.getStatDirect(kSlope256Base + 0);
    EXPECT_EQ(stat256.unknown, 509);
    EXPECT_EQ(stat256.assigned, 2);
}
