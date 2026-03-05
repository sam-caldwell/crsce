/**
 * @file unit_slope_propagation_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Unit tests verifying that PropagationEngine correctly propagates constraints across slope lines.
 *
 * Validates that the four toroidal-slope partitions (Slope256, Slope255, Slope2, Slope509) participate
 * in constraint propagation the same way as row/col/diag/anti-diag lines: forcing unknowns to 0 when
 * rho=0, forcing unknowns to 1 when rho=u, and detecting infeasibility when rho > u.
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <utility>
#include <vector>

#include "common/CrossSum/ToroidalSlopeSum.h"
#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/LineID.h"
#include "decompress/Solvers/PropagationEngine.h"

using crsce::common::ToroidalSlopeSum;
using crsce::decompress::solvers::CellState;
using crsce::decompress::solvers::ConstraintStore;
using crsce::decompress::solvers::LineID;
using crsce::decompress::solvers::LineType;
using crsce::decompress::solvers::PropagationEngine;

namespace {
    constexpr std::uint16_t kS = 511;
    constexpr std::uint16_t kNumDiags = (2 * kS) - 1;

    /**
     * @name makeAllZeroStore
     * @brief Build a ConstraintStore where all sum targets are zero (the all-zeros matrix is the solution).
     * @return A ConstraintStore with every target set to 0.
     */
    ConstraintStore makeAllZeroStore() {
        return {
            std::vector<std::uint16_t>(kS, 0),
            std::vector<std::uint16_t>(kS, 0),
            std::vector<std::uint16_t>(kNumDiags, 0),
            std::vector<std::uint16_t>(kNumDiags, 0),
            std::vector<std::uint16_t>(kS, 0),
            std::vector<std::uint16_t>(kS, 0),
            std::vector<std::uint16_t>(kS, 0),
            std::vector<std::uint16_t>(kS, 0),
            std::vector<std::uint16_t>(kS, 0),
            std::vector<std::uint16_t>(kS, 0)
        };
    }

    /**
     * @name makeStoreWithSlopeTarget
     * @brief Build a ConstraintStore where one slope line has a specific target and all other targets are zero.
     *
     * Also sets the row, column, diagonal, anti-diagonal, and other slope targets consistently so that
     * assigning 1 to cells on the target slope line does not create infeasibility on cross-cutting lines.
     *
     * @param slopeType Which slope partition to configure (Slope256, Slope255, Slope2, or Slope509).
     * @param lineIdx The line index within that partition (0..kS-1).
     * @param target The target sum for the specified slope line.
     * @return A ConstraintStore configured for slope line forcing tests.
     */
    ConstraintStore makeStoreWithSlopeTarget(const LineType slopeType,
                                             const std::uint16_t lineIdx,
                                             const std::uint16_t target) {
        std::vector<std::uint16_t> rowSums(kS, 0);
        std::vector<std::uint16_t> colSums(kS, 0);
        std::vector<std::uint16_t> diagSums(kNumDiags, 0);
        std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
        std::vector<std::uint16_t> slope256Sums(kS, 0);
        std::vector<std::uint16_t> slope255Sums(kS, 0);
        std::vector<std::uint16_t> slope2Sums(kS, 0);
        std::vector<std::uint16_t> slope509Sums(kS, 0);

        // Determine slope value from the LineType.
        std::uint16_t slope = 0;
        switch (slopeType) {
            case LineType::Slope256: slope = 256; slope256Sums[lineIdx] = target; break;
            case LineType::Slope255: slope = 255; slope255Sums[lineIdx] = target; break;
            case LineType::Slope2:   slope = 2;   slope2Sums[lineIdx] = target;   break;
            case LineType::Slope509: slope = 509; slope509Sums[lineIdx] = target; break;
            default: break;
        }

        // Enumerate the cells on the target slope line and increment cross-cutting line targets
        // so that forcing ones does not cause infeasibility on those lines.
        for (std::uint16_t t = 0; t < target; ++t) {
            const auto c = static_cast<std::uint16_t>((static_cast<std::uint32_t>(lineIdx)
                                                       + static_cast<std::uint32_t>(slope) * t) % kS);
            const auto diagIdx = static_cast<std::uint16_t>(c - t + (kS - 1));
            const auto antiIdx = static_cast<std::uint16_t>(t + c);

            rowSums[t] += 1;
            colSums[c] += 1;
            diagSums[diagIdx] += 1;
            antiDiagSums[antiIdx] += 1;

            // Increment other slope partition targets for cell (t, c).
            if (slopeType != LineType::Slope256) {
                slope256Sums[ToroidalSlopeSum::slopeLineIndex(t, c, 256, kS)] += 1;
            }
            if (slopeType != LineType::Slope255) {
                slope255Sums[ToroidalSlopeSum::slopeLineIndex(t, c, 255, kS)] += 1;
            }
            if (slopeType != LineType::Slope2) {
                slope2Sums[ToroidalSlopeSum::slopeLineIndex(t, c, 2, kS)] += 1;
            }
            if (slopeType != LineType::Slope509) {
                slope509Sums[ToroidalSlopeSum::slopeLineIndex(t, c, 509, kS)] += 1;
            }
        }

        return {rowSums, colSums, diagSums, antiDiagSums,
                slope256Sums, slope255Sums, slope2Sums, slope509Sums,
                std::vector<std::uint16_t>(kS, 0), std::vector<std::uint16_t>(kS, 0)};
    }

    /**
     * @name cellsOnSlopeLine
     * @brief Enumerate all (r, c) cells on a given slope line.
     * @param slope The slope parameter (256, 255, 2, or 509).
     * @param lineIdx The line index within the partition.
     * @return Vector of (row, col) pairs for the kS cells on the line.
     */
    std::vector<std::pair<std::uint16_t, std::uint16_t>> cellsOnSlopeLine(
        const std::uint16_t slope, const std::uint16_t lineIdx) {
        std::vector<std::pair<std::uint16_t, std::uint16_t>> result;
        result.reserve(kS);
        for (std::uint16_t t = 0; t < kS; ++t) {
            const auto c = static_cast<std::uint16_t>((static_cast<std::uint32_t>(lineIdx)
                                                       + static_cast<std::uint32_t>(slope) * t) % kS);
            result.emplace_back(t, c);
        }
        return result;
    }
} // namespace

/**
 * @name SlopePropagationTest.SlopeLineRhoZeroForcesZeros
 * @brief When a slope line has target=0 (rho=0), propagation forces all unknowns on that line to 0.
 *
 * Strategy: create a store where all targets are zero. Manually assign 510 of the 511 cells on
 * slope256 line 0 to 0, leaving one unknown. Then propagate the slope line. The remaining unknown
 * must be forced to 0 since rho = target - assigned_ones = 0 - 0 = 0.
 */
TEST(SlopePropagationTest, SlopeLineRhoZeroForcesZeros) {
    auto store = makeAllZeroStore();
    const auto cells = cellsOnSlopeLine(256, 0);

    // Assign 510 cells on slope256 line 0 to 0, leaving the last cell unknown.
    for (std::uint16_t i = 0; i < kS - 1; ++i) {
        store.assign(cells[i].first, cells[i].second, 0);
    }

    // The last cell on slope256 line 0 is still unknown.
    const auto &lastCell = cells[kS - 1];
    ASSERT_EQ(store.getCellState(lastCell.first, lastCell.second), CellState::Unassigned);

    PropagationEngine engine(store);

    // Propagate the slope256 line 0. rho = 0 - 0 = 0, u = 1 => force the last unknown to 0.
    const std::vector<LineID> queue = {{.type = LineType::Slope256, .index = 0}};
    engine.reset();
    const bool feasible = engine.propagate(queue);
    EXPECT_TRUE(feasible);

    // Verify the last cell was forced to 0.
    EXPECT_EQ(store.getCellState(lastCell.first, lastCell.second), CellState::Zero);

    // Verify the forced assignment list includes this cell.
    const auto &forced = engine.getForcedAssignments();
    bool foundLastCell = false;
    for (const auto &a : forced) {
        if (a.r == lastCell.first && a.c == lastCell.second) {
            EXPECT_EQ(a.value, 0);
            foundLastCell = true;
        }
    }
    EXPECT_TRUE(foundLastCell) << "Last cell on slope256 line 0 should appear in forced assignments";
}

/**
 * @name SlopePropagationTest.SlopeLineRhoEqualsUnknownForcesOnes
 * @brief When a slope line has rho=u (remaining ones equals unknowns), propagation forces unknowns to 1.
 *
 * Strategy: create a store where slope255 line 0 has target=1. Assign 510 cells on that line to 0,
 * leaving one unknown. Then propagate the slope line. The remaining unknown must be forced to 1
 * since rho = target - assigned_ones = 1 - 0 = 1 = u.
 */
TEST(SlopePropagationTest, SlopeLineRhoEqualsUnknownForcesOnes) {
    auto store = makeStoreWithSlopeTarget(LineType::Slope255, 0, 1);
    const auto cells = cellsOnSlopeLine(255, 0);

    // Assign 510 cells on slope255 line 0 to 0, leaving cell 0 (the first cell) unknown.
    // makeStoreWithSlopeTarget sets cross-cutting targets (row, col, diag, anti-diag, other slopes)
    // consistently for cell 0 being 1, so cell 0 is the one that should be forced.
    for (std::uint16_t i = 1; i < kS; ++i) {
        store.assign(cells[i].first, cells[i].second, 0);
    }

    // The first cell on slope255 line 0 is still unknown.
    const auto &firstCell = cells[0];
    ASSERT_EQ(store.getCellState(firstCell.first, firstCell.second), CellState::Unassigned);

    PropagationEngine engine(store);

    // Propagate the slope255 line 0. rho = 1 - 0 = 1, u = 1 => force the unknown to 1.
    const std::vector<LineID> queue = {{.type = LineType::Slope255, .index = 0}};
    engine.reset();
    const bool feasible = engine.propagate(queue);
    EXPECT_TRUE(feasible);

    // Verify the first cell was forced to 1.
    EXPECT_EQ(store.getCellState(firstCell.first, firstCell.second), CellState::One);

    // Verify the forced assignment list includes this cell with value 1.
    const auto &forced = engine.getForcedAssignments();
    bool foundFirstCell = false;
    for (const auto &a : forced) {
        if (a.r == firstCell.first && a.c == firstCell.second) {
            EXPECT_EQ(a.value, 1);
            foundFirstCell = true;
        }
    }
    EXPECT_TRUE(foundFirstCell) << "First cell on slope255 line 0 should appear in forced assignments with value 1";
}

/**
 * @name SlopePropagationTest.SlopeInfeasibilityDetected
 * @brief When a slope line becomes infeasible (rho > u), propagation returns false.
 *
 * Strategy: create a store where all targets are zero. Then manually assign a cell on slope2 line 0
 * to 1. This causes rho = 0 - 1 = -1 < 0 on that slope line. Propagation should detect this and
 * return false.
 */
TEST(SlopePropagationTest, SlopeInfeasibilityDetected) {
    auto store = makeAllZeroStore();
    const auto cells = cellsOnSlopeLine(2, 0);

    // Assign the first cell on slope2 line 0 to 1. The slope2 line 0 target is 0,
    // so rho = 0 - 1 = -1 < 0 => infeasible.
    store.assign(cells[0].first, cells[0].second, 1);

    PropagationEngine engine(store);

    const std::vector<LineID> queue = {{.type = LineType::Slope2, .index = 0}};
    engine.reset();
    const bool feasible = engine.propagate(queue);
    EXPECT_FALSE(feasible);
}
