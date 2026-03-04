/**
 * @file row_decomposed_controller_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Unit tests for RowDecomposedController: all-zeros, structured patterns, and infeasible cases.
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "common/Csm/Csm.h"
#include "decompress/Solvers/BranchingController.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/PropagationEngine.h"
#include "decompress/Solvers/RowDecomposedController.h"
#include "decompress/Solvers/Sha256HashVerifier.h"

using crsce::decompress::solvers::BranchingController;
using crsce::decompress::solvers::ConstraintStore;
using crsce::decompress::solvers::PropagationEngine;
using crsce::decompress::solvers::RowDecomposedController;
using crsce::decompress::solvers::Sha256HashVerifier;

namespace {
    constexpr std::uint16_t kS = 511;
    constexpr std::uint16_t kNumDiags = (2 * kS) - 1;
} // namespace

// ---------------------------------------------------------------------------
// Basic operations
// ---------------------------------------------------------------------------

/**
 * @brief reset() can be called without causing errors (documented no-op).
 */
TEST(RowDecomposedControllerTest, ResetIsNoOp) {
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

    RowDecomposedController controller(
        std::move(store), std::move(propagator),
        std::move(brancher), std::move(hasher)
    );

    controller.reset();
    controller.reset();
}

// ---------------------------------------------------------------------------
// All-zeros test
// ---------------------------------------------------------------------------

/**
 * @brief All-zero sums should yield exactly one solution: the all-zero matrix.
 *
 * Initial propagation forces all cells to 0 (every row/col target is 0).
 * The row-decomposed solver should recognize this and yield immediately.
 */
TEST(RowDecomposedControllerTest, AllZerosYieldsSingleSolution) {
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

    // Set expected hashes for the all-zero row
    const std::array<std::uint64_t, 8> zeroRow{};
    const auto zeroDigest = hasher->computeHash(zeroRow);
    for (std::uint16_t r = 0; r < kS; ++r) {
        hasher->setExpected(r, zeroDigest);
    }

    RowDecomposedController controller(
        std::move(store), std::move(propagator),
        std::move(brancher), std::move(hasher)
    );

    std::vector<crsce::common::Csm> solutions;
    controller.enumerate([&solutions](const crsce::common::Csm &csm) -> bool {
        solutions.push_back(csm);
        return true;
    });

    ASSERT_EQ(solutions.size(), 1U);

    // Verify the solution is all zeros
    for (std::uint16_t r = 0; r < kS; ++r) {
        for (std::uint16_t c = 0; c < kS; ++c) {
            EXPECT_EQ(solutions.at(0).get(r, c), 0)
                << "Cell (" << r << ", " << c << ") should be 0";
            if (solutions.at(0).get(r, c) != 0) {
                return;
            }
        }
    }
}

/**
 * @brief enumerateSolutionsLex with all-zero sums yields the same result.
 */
TEST(RowDecomposedControllerTest, EnumerateSolutionsLexAllZeros) {
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

    RowDecomposedController controller(
        std::move(store), std::move(propagator),
        std::move(brancher), std::move(hasher)
    );

    int solutionCount = 0;
    for (const auto &csm : controller.enumerateSolutionsLex()) {
        ++solutionCount;
        EXPECT_EQ(csm.get(0, 0), 0);
        EXPECT_EQ(csm.get(kS - 1, kS - 1), 0);
    }

    EXPECT_EQ(solutionCount, 1);
}

// ---------------------------------------------------------------------------
// Infeasible test
// ---------------------------------------------------------------------------

/**
 * @brief Infeasible constraints (contradictory row/col targets) yield no solutions.
 *
 * Row 0 target = kS (all ones), but column 0 target = 0 (no ones).
 * Cell (0, 0) must be both 1 and 0 -- contradiction detected during initial propagation.
 */
TEST(RowDecomposedControllerTest, InfeasibleYieldsNoSolutions) {
    std::vector<std::uint16_t> rowSums(kS, 0);
    const std::vector<std::uint16_t> colSums(kS, 0);
    const std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    const std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    const std::vector<std::uint16_t> slope256Sums(kS, 0);
    const std::vector<std::uint16_t> slope255Sums(kS, 0);
    const std::vector<std::uint16_t> slope2Sums(kS, 0);
    const std::vector<std::uint16_t> slope509Sums(kS, 0);
    rowSums[0] = kS;

    auto store = std::make_unique<ConstraintStore>(
        rowSums, colSums, diagSums, antiDiagSums,
        slope256Sums, slope255Sums, slope2Sums, slope509Sums
    );
    auto propagator = std::make_unique<PropagationEngine>(*store);
    auto brancher = std::make_unique<BranchingController>(*store, *propagator);
    auto hasher = std::make_unique<Sha256HashVerifier>(kS);

    RowDecomposedController controller(
        std::move(store), std::move(propagator),
        std::move(brancher), std::move(hasher)
    );

    int solutionCount = 0;
    for ([[maybe_unused]] const auto &csm : controller.enumerateSolutionsLex()) {
        ++solutionCount;
    }

    EXPECT_EQ(solutionCount, 0);
}

/**
 * @brief enumerate() with infeasible constraints calls callback zero times.
 */
TEST(RowDecomposedControllerTest, EnumerateInfeasibleNoCallback) {
    std::vector<std::uint16_t> rowSums(kS, 0);
    const std::vector<std::uint16_t> colSums(kS, 0);
    const std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    const std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    const std::vector<std::uint16_t> slope256Sums(kS, 0);
    const std::vector<std::uint16_t> slope255Sums(kS, 0);
    const std::vector<std::uint16_t> slope2Sums(kS, 0);
    const std::vector<std::uint16_t> slope509Sums(kS, 0);
    rowSums[0] = kS;

    auto store = std::make_unique<ConstraintStore>(
        rowSums, colSums, diagSums, antiDiagSums,
        slope256Sums, slope255Sums, slope2Sums, slope509Sums
    );
    auto propagator = std::make_unique<PropagationEngine>(*store);
    auto brancher = std::make_unique<BranchingController>(*store, *propagator);
    auto hasher = std::make_unique<Sha256HashVerifier>(kS);

    RowDecomposedController controller(
        std::move(store), std::move(propagator),
        std::move(brancher), std::move(hasher)
    );

    int callCount = 0;
    controller.enumerate([&callCount](const crsce::common::Csm & /*csm*/) -> bool {
        ++callCount;
        return true;
    });

    EXPECT_EQ(callCount, 0);
}

// ---------------------------------------------------------------------------
// Structured pattern: 2x2 corner (same as EnumerationController test)
// ---------------------------------------------------------------------------

/**
 * @brief 2x2 corner pattern yields the unique solution: (0,0)=1, (1,1)=1.
 *
 * Initial propagation should determine most of the matrix (all zero-sum rows/cols
 * are forced to 0). The diagonal constraint forces the remaining cells.
 */
TEST(RowDecomposedControllerTest, TwoByTwoCornerFindsUniqueSolution) {
    std::vector<std::uint16_t> rowSums(kS, 0);
    std::vector<std::uint16_t> colSums(kS, 0);
    std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    std::vector<std::uint16_t> slope256Sums(kS, 0);
    std::vector<std::uint16_t> slope255Sums(kS, 0);
    std::vector<std::uint16_t> slope2Sums(kS, 0);
    std::vector<std::uint16_t> slope509Sums(kS, 0);

    rowSums[0] = 1;
    rowSums[1] = 1;
    colSums[0] = 1;
    colSums[1] = 1;
    diagSums[510] = 2;
    antiDiagSums[0] = 1;
    antiDiagSums[2] = 1;

    // Cell (0,0): all slope indices = 0. Cell (1,1): slope256[256]=1, slope255[257]=1,
    // slope2[510]=1, slope509[3]=1.
    slope256Sums[0] = 1;
    slope256Sums[256] = 1;
    slope255Sums[0] = 1;
    slope255Sums[257] = 1;
    slope2Sums[0] = 1;
    slope2Sums[510] = 1;
    slope509Sums[0] = 1;
    slope509Sums[3] = 1;

    auto store = std::make_unique<ConstraintStore>(
        rowSums, colSums, diagSums, antiDiagSums,
        slope256Sums, slope255Sums, slope2Sums, slope509Sums
    );
    auto propagator = std::make_unique<PropagationEngine>(*store);
    auto brancher = std::make_unique<BranchingController>(*store, *propagator);
    auto hasher = std::make_unique<Sha256HashVerifier>(kS);

    // Compute expected hashes for each row
    {
        std::array<std::uint64_t, 8> row0{};
        row0[0] = std::uint64_t{1} << 63U; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        hasher->setExpected(0, hasher->computeHash(row0));

        std::array<std::uint64_t, 8> row1{};
        row1[0] = std::uint64_t{1} << 62U; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        hasher->setExpected(1, hasher->computeHash(row1));

        const std::array<std::uint64_t, 8> zeroRow{};
        const auto zeroDigest = hasher->computeHash(zeroRow);
        for (std::uint16_t r = 2; r < kS; ++r) {
            hasher->setExpected(r, zeroDigest);
        }
    }

    RowDecomposedController controller(
        std::move(store), std::move(propagator),
        std::move(brancher), std::move(hasher)
    );

    int solutionCount = 0;
    crsce::common::Csm result;
    for (const auto &csm : controller.enumerateSolutionsLex()) {
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
 * @brief enumerate() callback returning false stops enumeration early.
 */
TEST(RowDecomposedControllerTest, EnumerateStopsOnCallbackFalse) {
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

    RowDecomposedController controller(
        std::move(store), std::move(propagator),
        std::move(brancher), std::move(hasher)
    );

    int callCount = 0;
    controller.enumerate([&callCount](const crsce::common::Csm & /*csm*/) -> bool {
        ++callCount;
        return false;
    });

    EXPECT_EQ(callCount, 1);
}

/**
 * @brief All-ones matrix: all sums are kS, all cells should be 1.
 */
TEST(RowDecomposedControllerTest, AllOnesYieldsSingleSolution) {
    // All-ones: each row sum = kS, each col sum = kS
    // Diagonal sums vary by diagonal length, but for all-ones every diagonal
    // sum equals the diagonal length. We need correct diagonal sums.
    const std::vector<std::uint16_t> rowSums(kS, kS);
    const std::vector<std::uint16_t> colSums(kS, kS);
    std::vector<std::uint16_t> diagSums(kNumDiags);
    std::vector<std::uint16_t> antiDiagSums(kNumDiags);

    // Diagonal d has length min(d+1, kS, kNumDiags-d)
    for (std::uint16_t d = 0; d < kNumDiags; ++d) {
        const auto len = std::min({
            static_cast<std::uint16_t>(d + 1),
            kS,
            static_cast<std::uint16_t>(kNumDiags - d)
        });
        diagSums.at(d) = len;
        antiDiagSums.at(d) = len;
    }

    const std::vector<std::uint16_t> slope256Sums(kS, kS);
    const std::vector<std::uint16_t> slope255Sums(kS, kS);
    const std::vector<std::uint16_t> slope2Sums(kS, kS);
    const std::vector<std::uint16_t> slope509Sums(kS, kS);

    auto store = std::make_unique<ConstraintStore>(
        rowSums, colSums, diagSums, antiDiagSums,
        slope256Sums, slope255Sums, slope2Sums, slope509Sums
    );
    auto propagator = std::make_unique<PropagationEngine>(*store);
    auto brancher = std::make_unique<BranchingController>(*store, *propagator);
    auto hasher = std::make_unique<Sha256HashVerifier>(kS);

    // Compute expected hashes for all-ones rows
    std::array<std::uint64_t, 8> onesRow{};
    for (std::size_t w = 0; w < 7; ++w) {
        onesRow.at(w) = 0xFFFFFFFFFFFFFFFFULL;
    }
    // Last word: 511 - 7*64 = 511 - 448 = 63 bits set (MSB-first)
    onesRow.at(7) = 0xFFFFFFFFFFFFFFFEULL; // top 63 bits set, bottom bit 0
    const auto onesDigest = hasher->computeHash(onesRow);
    for (std::uint16_t r = 0; r < kS; ++r) {
        hasher->setExpected(r, onesDigest);
    }

    RowDecomposedController controller(
        std::move(store), std::move(propagator),
        std::move(brancher), std::move(hasher)
    );

    int solutionCount = 0;
    for (const auto &csm : controller.enumerateSolutionsLex()) {
        ++solutionCount;
        // Verify corners are all ones
        EXPECT_EQ(csm.get(0, 0), 1);
        EXPECT_EQ(csm.get(0, kS - 1), 1);
        EXPECT_EQ(csm.get(kS - 1, 0), 1);
        EXPECT_EQ(csm.get(kS - 1, kS - 1), 1);
    }

    EXPECT_EQ(solutionCount, 1);
}
