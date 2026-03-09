/**
 * @file metal_propagation_engine_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Unit tests for the MetalPropagationEngine class.
 */
#include <gtest/gtest.h>

#ifdef CRSCE_ENABLE_METAL

#include <algorithm>
#include <cstdint>
#include <vector>

#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/LineID.h"
#include "decompress/Solvers/LtpTable.h"
#include "decompress/Solvers/MetalContext.h"
#include "decompress/Solvers/MetalPropagationEngine.h"

using crsce::decompress::solvers::CellState;
using crsce::decompress::solvers::ConstraintStore;
using crsce::decompress::solvers::LineID;
using crsce::decompress::solvers::LineType;
using crsce::decompress::solvers::MetalContext;
using crsce::decompress::solvers::MetalPropagationEngine;
using crsce::decompress::solvers::kLtp1Base;
using crsce::decompress::solvers::kLtp2Base;
using crsce::decompress::solvers::kLtp3Base;
using crsce::decompress::solvers::kLtp4Base;
using crsce::decompress::solvers::kLtp5Base;
using crsce::decompress::solvers::kLtp6Base;
using crsce::decompress::solvers::ltpMembership;

namespace {
    constexpr std::uint16_t kS = 511;
    constexpr std::uint16_t kNumDiags = (2 * kS) - 1;

    /**
     * @brief Helper to skip if no Metal GPU is available.
     */
    void skipIfNoMetal() {
        const MetalContext probe;
        if (!probe.isAvailable()) {
            GTEST_SKIP() << "No Metal GPU device available";
        }
    }
} // namespace

/**
 * @brief When rho=0 and u>0 on a row, all unknowns on that row are forced to 0.
 *
 * Mirrors PropagationEngineTest::RhoZeroForcesAllUnknownsToZero.
 */
TEST(MetalPropagationEngineTest, RhoZeroForcesAllUnknownsToZero) {
    skipIfNoMetal();

    const std::vector<std::uint16_t> rowSums(kS, 0);
    const std::vector<std::uint16_t> colSums(kS, 0);
    const std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    const std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    const std::vector<std::uint16_t> ltp1Sums(kS, 0);
    const std::vector<std::uint16_t> ltp2Sums(kS, 0);
    const std::vector<std::uint16_t> ltp3Sums(kS, 0);
    const std::vector<std::uint16_t> ltp4Sums(kS, 0);
    const std::vector<std::uint16_t> ltp5Sums(kS, 0);
    const std::vector<std::uint16_t> ltp6Sums(kS, 0);

    ConstraintStore store(rowSums, colSums, diagSums, antiDiagSums,
                          ltp1Sums, ltp2Sums, ltp3Sums, ltp4Sums,
                      ltp5Sums, ltp6Sums);
    MetalPropagationEngine engine(store, rowSums, colSums, diagSums, antiDiagSums);

    const std::vector<LineID> queue = {{.type = LineType::Row, .index = 0}};
    const bool feasible = engine.propagate(queue);
    EXPECT_TRUE(feasible);

    for (std::uint16_t c = 0; c < kS; ++c) {
        EXPECT_EQ(store.getCellState(0, c), CellState::Zero)
            << "Cell (0, " << c << ") should be Zero";
    }

    const auto forced = engine.getForcedAssignments();
    std::uint32_t row0ForcedCount = 0;
    for (const auto &a : forced) {
        if (a.r == 0) {
            EXPECT_EQ(a.value, 0);
            ++row0ForcedCount;
        }
    }
    EXPECT_GE(row0ForcedCount, kS);
}

/**
 * @brief When rho=u and u>0, unknowns are forced to 1.
 *
 * Mirrors PropagationEngineTest::RhoEqualsUForcesAllUnknownsToOne.
 */
TEST(MetalPropagationEngineTest, RhoEqualsUForcesAllUnknownsToOne) {
    skipIfNoMetal();

    std::vector<std::uint16_t> rowSums(kS, 0);
    std::vector<std::uint16_t> colSums(kS, 0);
    std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    std::vector<std::uint16_t> ltp1Sums(kS, 0);
    std::vector<std::uint16_t> ltp2Sums(kS, 0);
    std::vector<std::uint16_t> ltp3Sums(kS, 0);
    std::vector<std::uint16_t> ltp4Sums(kS, 0);
    std::vector<std::uint16_t> ltp5Sums(kS, 0);
    std::vector<std::uint16_t> ltp6Sums(kS, 0);

    antiDiagSums[0] = 1;
    rowSums[0] = 1;
    colSums[0] = 1;
    diagSums[510] = 1;
    // Set LTP targets for cell (0,0) using precomputed partition tables.
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
            } else if (f < static_cast<std::uint16_t>(kLtp5Base)) {
                ltp4Sums[static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp4Base))] = 1;
            } else if (f < static_cast<std::uint16_t>(kLtp6Base)) {
                ltp5Sums[static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp5Base))] = 1;
            } else {
                ltp6Sums[static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp6Base))] = 1;
            }
        }
    }

    ConstraintStore store(rowSums, colSums, diagSums, antiDiagSums,
                          ltp1Sums, ltp2Sums, ltp3Sums, ltp4Sums,
                      ltp5Sums, ltp6Sums);
    MetalPropagationEngine engine(store, rowSums, colSums, diagSums, antiDiagSums);

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
 * @brief reset() clears the forced assignment list.
 */
TEST(MetalPropagationEngineTest, ResetClearsForcedAssignments) {
    skipIfNoMetal();

    std::vector<std::uint16_t> rowSums(kS, 0);
    std::vector<std::uint16_t> colSums(kS, 0);
    std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    std::vector<std::uint16_t> ltp1Sums(kS, 0);
    std::vector<std::uint16_t> ltp2Sums(kS, 0);
    std::vector<std::uint16_t> ltp3Sums(kS, 0);
    std::vector<std::uint16_t> ltp4Sums(kS, 0);
    std::vector<std::uint16_t> ltp5Sums(kS, 0);
    std::vector<std::uint16_t> ltp6Sums(kS, 0);

    antiDiagSums[0] = 1;
    rowSums[0] = 1;
    colSums[0] = 1;
    diagSums[510] = 1;
    // Set LTP targets for cell (0,0) using precomputed partition tables.
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
            } else if (f < static_cast<std::uint16_t>(kLtp5Base)) {
                ltp4Sums[static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp4Base))] = 1;
            } else if (f < static_cast<std::uint16_t>(kLtp6Base)) {
                ltp5Sums[static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp5Base))] = 1;
            } else {
                ltp6Sums[static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp6Base))] = 1;
            }
        }
    }

    ConstraintStore store(rowSums, colSums, diagSums, antiDiagSums,
                          ltp1Sums, ltp2Sums, ltp3Sums, ltp4Sums,
                      ltp5Sums, ltp6Sums);
    MetalPropagationEngine engine(store, rowSums, colSums, diagSums, antiDiagSums);

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
 */
TEST(MetalPropagationEngineTest, InfeasibleWhenRhoNegative) {
    skipIfNoMetal();

    const std::vector<std::uint16_t> rowSums(kS, 0);
    const std::vector<std::uint16_t> colSums(kS, 0);
    const std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    const std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    const std::vector<std::uint16_t> ltp1Sums(kS, 0);
    const std::vector<std::uint16_t> ltp2Sums(kS, 0);
    const std::vector<std::uint16_t> ltp3Sums(kS, 0);
    const std::vector<std::uint16_t> ltp4Sums(kS, 0);
    const std::vector<std::uint16_t> ltp5Sums(kS, 0);
    const std::vector<std::uint16_t> ltp6Sums(kS, 0);

    ConstraintStore store(rowSums, colSums, diagSums, antiDiagSums,
                          ltp1Sums, ltp2Sums, ltp3Sums, ltp4Sums,
                      ltp5Sums, ltp6Sums);
    store.assign(0, 0, 1);

    MetalPropagationEngine engine(store, rowSums, colSums, diagSums, antiDiagSums);

    const std::vector<LineID> queue = {{.type = LineType::Row, .index = 0}};
    const bool feasible = engine.propagate(queue);
    EXPECT_FALSE(feasible);
}

/**
 * @brief Propagation with an empty queue is feasible and produces no forced assignments.
 */
TEST(MetalPropagationEngineTest, EmptyQueueIsFeasible) {
    skipIfNoMetal();

    const std::vector<std::uint16_t> rowSums(kS, 0);
    const std::vector<std::uint16_t> colSums(kS, 0);
    const std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    const std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    const std::vector<std::uint16_t> ltp1Sums(kS, 0);
    const std::vector<std::uint16_t> ltp2Sums(kS, 0);
    const std::vector<std::uint16_t> ltp3Sums(kS, 0);
    const std::vector<std::uint16_t> ltp4Sums(kS, 0);
    const std::vector<std::uint16_t> ltp5Sums(kS, 0);
    const std::vector<std::uint16_t> ltp6Sums(kS, 0);

    ConstraintStore store(rowSums, colSums, diagSums, antiDiagSums,
                          ltp1Sums, ltp2Sums, ltp3Sums, ltp4Sums,
                      ltp5Sums, ltp6Sums);
    MetalPropagationEngine engine(store, rowSums, colSums, diagSums, antiDiagSums);

    const std::vector<LineID> queue;
    const bool feasible = engine.propagate(queue);
    EXPECT_TRUE(feasible);
    EXPECT_TRUE(engine.getForcedAssignments().empty());
}

/**
 * @brief When neither rho=0 nor rho=u, no forcing occurs and the result is feasible.
 */
TEST(MetalPropagationEngineTest, NoForcingWhenRhoBetweenZeroAndU) {
    skipIfNoMetal();

    const std::vector<std::uint16_t> rowSums(kS, 255);
    const std::vector<std::uint16_t> colSums(kS, 255);
    std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    const std::vector<std::uint16_t> ltp1Sums(kS, static_cast<std::uint16_t>(kS / 2));
    const std::vector<std::uint16_t> ltp2Sums(kS, static_cast<std::uint16_t>(kS / 2));
    const std::vector<std::uint16_t> ltp3Sums(kS, static_cast<std::uint16_t>(kS / 2));
    const std::vector<std::uint16_t> ltp4Sums(kS, static_cast<std::uint16_t>(kS / 2));
    const std::vector<std::uint16_t> ltp5Sums(kS, static_cast<std::uint16_t>(kS / 2));
    const std::vector<std::uint16_t> ltp6Sums(kS, static_cast<std::uint16_t>(kS / 2));

    for (std::uint16_t d = 0; d < kNumDiags; ++d) {
        const auto len = std::min({static_cast<int>(d + 1),
                                   static_cast<int>(kS),
                                   static_cast<int>(kNumDiags - d)});
        diagSums[d] = static_cast<std::uint16_t>(len / 2);
        antiDiagSums[d] = static_cast<std::uint16_t>(len / 2);
    }

    ConstraintStore store(rowSums, colSums, diagSums, antiDiagSums,
                          ltp1Sums, ltp2Sums, ltp3Sums, ltp4Sums,
                      ltp5Sums, ltp6Sums);
    MetalPropagationEngine engine(store, rowSums, colSums, diagSums, antiDiagSums);

    const std::vector<LineID> queue = {{.type = LineType::Row, .index = 0}};
    const bool feasible = engine.propagate(queue);
    EXPECT_TRUE(feasible);
    EXPECT_TRUE(engine.getForcedAssignments().empty());
}

#else // !CRSCE_ENABLE_METAL

/**
 * @brief Placeholder test when Metal is not enabled.
 */
TEST(MetalPropagationEngineTest, Disabled) {
    GTEST_SKIP() << "Metal GPU acceleration is not enabled";
}

#endif // CRSCE_ENABLE_METAL
