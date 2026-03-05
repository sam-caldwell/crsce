/**
 * @file propagation_engine_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Unit tests for the PropagationEngine class.
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/IPropagationEngine.h"
#include "decompress/Solvers/LineID.h"
#include "decompress/Solvers/PropagationEngine.h"
#include "decompress/Solvers/LtpTable.h"

using crsce::decompress::solvers::CellState;
using crsce::decompress::solvers::ConstraintStore;
using crsce::decompress::solvers::LineID;
using crsce::decompress::solvers::LineType;
using crsce::decompress::solvers::PropagationEngine;
using crsce::decompress::solvers::kLtp1Base;
using crsce::decompress::solvers::kLtp2Base;
using crsce::decompress::solvers::kLtp3Base;
using crsce::decompress::solvers::kLtp4Base;
using crsce::decompress::solvers::ltpMembership;

namespace {
    constexpr std::uint16_t kS = 511;
    constexpr std::uint16_t kNumDiags = (2 * kS) - 1;

    /**
     * @brief Build a ConstraintStore. LTP sums default to zero.
     */
    ConstraintStore makeStore(const std::vector<std::uint16_t> &rowSums,
                              const std::vector<std::uint16_t> &colSums,
                              const std::vector<std::uint16_t> &diagSums,
                              const std::vector<std::uint16_t> &antiDiagSums,
                              const std::vector<std::uint16_t> &ltp1Sums = std::vector<std::uint16_t>(kS, 0),
                              const std::vector<std::uint16_t> &ltp2Sums = std::vector<std::uint16_t>(kS, 0),
                              const std::vector<std::uint16_t> &ltp3Sums = std::vector<std::uint16_t>(kS, 0),
                              const std::vector<std::uint16_t> &ltp4Sums = std::vector<std::uint16_t>(kS, 0)) {
        return {rowSums, colSums, diagSums, antiDiagSums, ltp1Sums, ltp2Sums, ltp3Sums, ltp4Sums};
    }
} // namespace

/**
 * @brief When rho=0 and u>0 on a row, all unknowns on that row are forced to 0.
 *
 * Set all target sums to 0.  Propagate row 0.
 * All kS cells on row 0 should be forced to 0, and cascading propagation will
 * force all other cells to 0 as well.
 */
TEST(PropagationEngineTest, RhoZeroForcesAllUnknownsToZero) {
    const std::vector<std::uint16_t> rowSums(kS, 0);
    const std::vector<std::uint16_t> colSums(kS, 0);
    const std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    const std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    auto store = makeStore(rowSums, colSums, diagSums, antiDiagSums);
    PropagationEngine engine(store);

    const std::vector<LineID> queue = {{.type = LineType::Row, .index = 0}};
    const bool feasible = engine.propagate(queue);
    EXPECT_TRUE(feasible);

    // All cells on row 0 should now be Zero
    for (std::uint16_t c = 0; c < kS; ++c) {
        EXPECT_EQ(store.getCellState(0, c), CellState::Zero)
            << "Cell (0, " << c << ") should be Zero";
    }

    // Forced assignments should include all kS cells on row 0
    const auto forced = engine.getForcedAssignments();
    std::uint32_t row0ForcedCount = 0;
    for (const auto &a : forced) {
        if (a.r == 0) {
            EXPECT_EQ(a.value, 0);
            ++row0ForcedCount;
        }
    }
    // At minimum, all kS cells from row 0 should have been forced
    EXPECT_GE(row0ForcedCount, kS);
}

/**
 * @brief When rho=u and u>0 on an anti-diagonal, all unknowns are forced to 1.
 *
 * Anti-diagonal 0 has exactly 1 cell: (0, 0).  Set its target sum to 1.
 * When we propagate that line, cell (0, 0) must be forced to 1.
 */
TEST(PropagationEngineTest, RhoEqualsUForcesAllUnknownsToOne) {
    std::vector<std::uint16_t> rowSums(kS, 0);
    std::vector<std::uint16_t> colSums(kS, 0);
    std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    // Anti-diagonal 0 contains only cell (0, 0). Set target = 1.
    antiDiagSums[0] = 1;

    // Row 0 and Col 0 targets: set to 1 so the forced assignment of 1 at (0,0)
    // does not cause rho < 0 on those lines.
    rowSums[0] = 1;
    colSums[0] = 1;

    // Diagonal for (0,0) is index 510. Set its target to 1.
    diagSums[510] = 1;
    // LTP line targets for (0,0): must equal 1 to allow assigning (0,0)=1 without infeasibility.
    std::vector<std::uint16_t> ltp1Sums(kS, 0);
    std::vector<std::uint16_t> ltp2Sums(kS, 0);
    std::vector<std::uint16_t> ltp3Sums(kS, 0);
    std::vector<std::uint16_t> ltp4Sums(kS, 0);
    {
        const auto &mem = ltpMembership(0, 0);
        for (std::uint8_t j = 0; j < mem.count; ++j) {
            const auto f = mem.flat[j]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            if (f < static_cast<std::uint16_t>(kLtp2Base)) {
                ltp1Sums[static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp1Base))] = 1;
            } else if (f < static_cast<std::uint16_t>(kLtp3Base)) {
                ltp2Sums[static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp2Base))] = 1;
            } else if (f < static_cast<std::uint16_t>(kLtp4Base)) {
                ltp3Sums[static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp3Base))] = 1;
            } else {
                ltp4Sums[static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp4Base))] = 1;
            }
        }
    }
    auto store = makeStore(rowSums, colSums, diagSums, antiDiagSums,
                           ltp1Sums, ltp2Sums, ltp3Sums, ltp4Sums);
    PropagationEngine engine(store);

    const std::vector<LineID> queue = {
        {.type = LineType::AntiDiagonal, .index = 0}
    };
    const bool feasible = engine.propagate(queue);
    EXPECT_TRUE(feasible);

    EXPECT_EQ(store.getCellState(0, 0), CellState::One);

    const auto forced = engine.getForcedAssignments();
    bool foundCell00 = false;
    for (const auto &a : forced) {
        if (a.r == 0 && a.c == 0) {
            EXPECT_EQ(a.value, 1);
            foundCell00 = true;
        }
    }
    EXPECT_TRUE(foundCell00) << "Cell (0, 0) should appear in forced assignments";
}

/**
 * @brief getForcedAssignments() returns the list of forced cells after propagation.
 */
TEST(PropagationEngineTest, GetForcedAssignmentsReturnsList) {
    std::vector<std::uint16_t> rowSums(kS, 0);
    std::vector<std::uint16_t> colSums(kS, 0);
    std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    // Anti-diagonal 0 has length 1, target = 1 => forces cell (0,0) to 1
    antiDiagSums[0] = 1;
    rowSums[0] = 1;
    colSums[0] = 1;
    diagSums[510] = 1;
    // LTP line targets for (0,0): must equal 1 to allow assigning (0,0)=1 without infeasibility.
    std::vector<std::uint16_t> ltp1Sums(kS, 0);
    std::vector<std::uint16_t> ltp2Sums(kS, 0);
    std::vector<std::uint16_t> ltp3Sums(kS, 0);
    std::vector<std::uint16_t> ltp4Sums(kS, 0);
    {
        const auto &mem = ltpMembership(0, 0);
        for (std::uint8_t j = 0; j < mem.count; ++j) {
            const auto f = mem.flat[j]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            if (f < static_cast<std::uint16_t>(kLtp2Base)) {
                ltp1Sums[static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp1Base))] = 1;
            } else if (f < static_cast<std::uint16_t>(kLtp3Base)) {
                ltp2Sums[static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp2Base))] = 1;
            } else if (f < static_cast<std::uint16_t>(kLtp4Base)) {
                ltp3Sums[static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp3Base))] = 1;
            } else {
                ltp4Sums[static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp4Base))] = 1;
            }
        }
    }
    auto store = makeStore(rowSums, colSums, diagSums, antiDiagSums,
                           ltp1Sums, ltp2Sums, ltp3Sums, ltp4Sums);
    PropagationEngine engine(store);

    // Before propagation, forced list should be empty
    EXPECT_TRUE(engine.getForcedAssignments().empty());

    const std::vector<LineID> queue = {
        {.type = LineType::AntiDiagonal, .index = 0}
    };
    engine.propagate(queue);

    const auto forced = engine.getForcedAssignments();
    EXPECT_FALSE(forced.empty());
}

/**
 * @brief reset() clears the forced assignment list.
 */
TEST(PropagationEngineTest, ResetClearsForcedAssignments) {
    std::vector<std::uint16_t> rowSums(kS, 0);
    std::vector<std::uint16_t> colSums(kS, 0);
    std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    antiDiagSums[0] = 1;
    rowSums[0] = 1;
    colSums[0] = 1;
    diagSums[510] = 1;
    // LTP line targets for (0,0): must equal 1 to allow assigning (0,0)=1 without infeasibility.
    std::vector<std::uint16_t> ltp1Sums(kS, 0);
    std::vector<std::uint16_t> ltp2Sums(kS, 0);
    std::vector<std::uint16_t> ltp3Sums(kS, 0);
    std::vector<std::uint16_t> ltp4Sums(kS, 0);
    {
        const auto &mem = ltpMembership(0, 0);
        for (std::uint8_t j = 0; j < mem.count; ++j) {
            const auto f = mem.flat[j]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            if (f < static_cast<std::uint16_t>(kLtp2Base)) {
                ltp1Sums[static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp1Base))] = 1;
            } else if (f < static_cast<std::uint16_t>(kLtp3Base)) {
                ltp2Sums[static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp2Base))] = 1;
            } else if (f < static_cast<std::uint16_t>(kLtp4Base)) {
                ltp3Sums[static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp3Base))] = 1;
            } else {
                ltp4Sums[static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp4Base))] = 1;
            }
        }
    }
    auto store = makeStore(rowSums, colSums, diagSums, antiDiagSums,
                           ltp1Sums, ltp2Sums, ltp3Sums, ltp4Sums);
    PropagationEngine engine(store);

    const std::vector<LineID> queue = {
        {.type = LineType::AntiDiagonal, .index = 0}
    };
    engine.propagate(queue);
    EXPECT_FALSE(engine.getForcedAssignments().empty());

    engine.reset();
    EXPECT_TRUE(engine.getForcedAssignments().empty());
}

/**
 * @brief When rho < 0 on a line, propagate returns false (infeasible).
 *
 * Set row 0 target = 0 but manually assign cell (0, 0) to 1 beforehand.
 * Then rho = 0 - 1 = -1 < 0. Propagation should detect infeasibility.
 */
TEST(PropagationEngineTest, InfeasibleWhenRhoNegative) {
    const std::vector<std::uint16_t> rowSums(kS, 0);
    const std::vector<std::uint16_t> colSums(kS, 0);
    const std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    const std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    auto store = makeStore(rowSums, colSums, diagSums, antiDiagSums);

    // Manually assign cell (0, 0) to 1.  Row 0 target is 0, so rho = 0 - 1 = -1.
    store.assign(0, 0, 1);

    PropagationEngine engine(store);

    const std::vector<LineID> queue = {{.type = LineType::Row, .index = 0}};
    const bool feasible = engine.propagate(queue);
    EXPECT_FALSE(feasible);
}

/**
 * @brief When rho > u on a line, propagate returns false (infeasible).
 *
 * Set row 0 target = kS (all must be 1) and then assign all cells to 0 except one.
 * The remaining unknown count u = 1 but rho = kS - 0 = kS > 1 = u => infeasible.
 */
TEST(PropagationEngineTest, InfeasibleWhenRhoGreaterThanU) {
    std::vector<std::uint16_t> rowSums(kS, 0);
    const std::vector<std::uint16_t> colSums(kS, 0);
    const std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    const std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    // Row 0 target = kS (need all kS cells to be 1)
    rowSums[0] = kS;

    auto store = makeStore(rowSums, colSums, diagSums, antiDiagSums);

    // Assign all but the last cell on row 0 to 0
    for (std::uint16_t c = 0; c < kS - 1; ++c) {
        store.assign(0, c, 0);
    }
    // Now u = 1, rho = kS - 0 = 511 > 1 => infeasible

    PropagationEngine engine(store);

    const std::vector<LineID> queue = {{.type = LineType::Row, .index = 0}};
    const bool feasible = engine.propagate(queue);
    EXPECT_FALSE(feasible);
}

/**
 * @brief Propagation with an empty queue is feasible and produces no forced assignments.
 */
TEST(PropagationEngineTest, EmptyQueueIsFeasible) {
    auto store = makeStore(
        std::vector<std::uint16_t>(kS, 0),
        std::vector<std::uint16_t>(kS, 0),
        std::vector<std::uint16_t>(kNumDiags, 0),
        std::vector<std::uint16_t>(kNumDiags, 0)
    );
    PropagationEngine engine(store);

    const std::vector<LineID> queue;
    const bool feasible = engine.propagate(queue);
    EXPECT_TRUE(feasible);
    EXPECT_TRUE(engine.getForcedAssignments().empty());
}

/**
 * @brief When neither rho=0 nor rho=u, no forcing occurs and the result is feasible.
 */
TEST(PropagationEngineTest, NoForcingWhenRhoBetweenZeroAndU) {
    const std::vector<std::uint16_t> rowSums(kS, 255);
    const std::vector<std::uint16_t> colSums(kS, 255);
    std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);

    // Set diag/anti-diag targets to reasonable values so no line is infeasible.
    // The main diagonal (index 510) has length 511, so target 255 is fine.
    for (std::uint16_t d = 0; d < kNumDiags; ++d) {
        // Compute diagonal length: min(d+1, kS, kNumDiags - d)
        const auto len = std::min({static_cast<int>(d + 1),
                                   static_cast<int>(kS),
                                   static_cast<int>(kNumDiags - d)});
        diagSums[d] = static_cast<std::uint16_t>(len / 2);
        antiDiagSums[d] = static_cast<std::uint16_t>(len / 2);
    }

    // LTP sums default to 0 (no forcing triggered since no cells are assigned).
    auto store = makeStore(rowSums, colSums, diagSums, antiDiagSums);
    PropagationEngine engine(store);

    // Propagate row 0: rho = 255, u = 511. 0 < 255 < 511, so no forcing.
    const std::vector<LineID> queue = {{.type = LineType::Row, .index = 0}};
    const bool feasible = engine.propagate(queue);
    EXPECT_TRUE(feasible);
    EXPECT_TRUE(engine.getForcedAssignments().empty());
}
