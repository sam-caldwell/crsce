/**
 * @file unit_failed_literal_prober_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Unit tests for the FailedLiteralProber class.
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "decompress/Solvers/BranchingController.h"
#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/FailedLiteralProber.h"
#include "decompress/Solvers/LtpTable.h"
#include "decompress/Solvers/PropagationEngine.h"
#include "decompress/Solvers/Sha256HashVerifier.h"

using crsce::decompress::solvers::BranchingController;
using crsce::decompress::solvers::CellState;
using crsce::decompress::solvers::ConstraintStore;
using crsce::decompress::solvers::FailedLiteralProber;
using crsce::decompress::solvers::kLtp1Base;
using crsce::decompress::solvers::kLtp2Base;
using crsce::decompress::solvers::kLtp3Base;
using crsce::decompress::solvers::kLtp4Base;
using crsce::decompress::solvers::kLtp5Base;
using crsce::decompress::solvers::kLtp6Base;
using crsce::decompress::solvers::kLtpNumLines;
using crsce::decompress::solvers::ltpLineLen;
using crsce::decompress::solvers::ltpMembership;
using crsce::decompress::solvers::PropagationEngine;
using crsce::decompress::solvers::Sha256HashVerifier;

namespace {
    constexpr std::uint16_t kS = 511;
    constexpr std::uint16_t kNumDiags = (2 * kS) - 1;

    /**
     * @brief Build a ConstraintStore with customizable sum vectors.
     *
     * LTP targets default to 0 unless overridden.
     */
    ConstraintStore makeStore(const std::vector<std::uint16_t> &rowSums,
                              const std::vector<std::uint16_t> &colSums,
                              const std::vector<std::uint16_t> &diagSums,
                              const std::vector<std::uint16_t> &antiDiagSums,
                              const std::vector<std::uint16_t> &ltp1Sums = std::vector<std::uint16_t>(kS, 0),
                              const std::vector<std::uint16_t> &ltp2Sums = std::vector<std::uint16_t>(kS, 0),
                              const std::vector<std::uint16_t> &ltp3Sums = std::vector<std::uint16_t>(kS, 0),
                              const std::vector<std::uint16_t> &ltp4Sums = std::vector<std::uint16_t>(kS, 0),
                              const std::vector<std::uint16_t> &ltp5Sums = std::vector<std::uint16_t>(kS, 0),
                              const std::vector<std::uint16_t> &ltp6Sums = std::vector<std::uint16_t>(kS, 0)) {
        return {rowSums, colSums, diagSums, antiDiagSums,
                ltp1Sums, ltp2Sums, ltp3Sums, ltp4Sums, ltp5Sums, ltp6Sums};
    }

    /**
     * @brief Build an all-zero ConstraintStore.
     */
    ConstraintStore makeAllZeroStore() {
        return makeStore(
            std::vector<std::uint16_t>(kS, 0),
            std::vector<std::uint16_t>(kS, 0),
            std::vector<std::uint16_t>(kNumDiags, 0),
            std::vector<std::uint16_t>(kNumDiags, 0)
        );
    }

    /**
     * @brief Build a mid-range ConstraintStore where no single-cell assignment triggers forcing.
     */
    ConstraintStore makeMidRangeStore() {
        std::vector<std::uint16_t> diagSums(kNumDiags, 0);
        std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
        for (std::uint16_t d = 0; d < kNumDiags; ++d) {
            const auto len = std::min({static_cast<int>(d + 1),
                                       static_cast<int>(kS),
                                       static_cast<int>(kNumDiags - d)});
            diagSums[d] = static_cast<std::uint16_t>(len / 2);
            antiDiagSums[d] = static_cast<std::uint16_t>(len / 2);
        }
        // LTP line lengths are variable: ltp_len(k) = min(k+1, 511-k), max 256.
        // Use ltp_len(k)/2 as mid-range target so no line starts infeasible.
        std::vector<std::uint16_t> ltpSums(kLtpNumLines);
        for (std::uint16_t k = 0; k < kLtpNumLines; ++k) {
            ltpSums[k] = static_cast<std::uint16_t>(ltpLineLen(k) / 2U);
        }
        return makeStore(
            std::vector<std::uint16_t>(kS, 255),
            std::vector<std::uint16_t>(kS, 255),
            diagSums,
            antiDiagSums,
            ltpSums, ltpSums, ltpSums, ltpSums, ltpSums, ltpSums
        );
    }

    /**
     * @brief Set expected hashes for all-zero rows on a hasher.
     */
    void setAllZeroExpectedHashes(Sha256HashVerifier &hasher) {
        const std::array<std::uint64_t, 8> zeroRow{};
        const auto zeroDigest = hasher.computeHash(zeroRow);
        for (std::uint16_t r = 0; r < kS; ++r) {
            hasher.setExpected(r, zeroDigest);
        }
    }
} // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

/**
 * @brief Creating a FailedLiteralProber with valid references does not throw.
 */
TEST(FailedLiteralProberTest, ConstructionDoesNotThrow) {
    auto store = makeAllZeroStore();
    PropagationEngine propagator(store);
    BranchingController brancher(store, propagator);
    Sha256HashVerifier hasher(kS);
    EXPECT_NO_THROW(FailedLiteralProber(store, propagator, brancher, hasher));
}

// ---------------------------------------------------------------------------
// probeCell
// ---------------------------------------------------------------------------

/**
 * @brief When both values are feasible, probeCell returns forcedValue=255 and bothInfeasible=false.
 *
 * Uses an interior cell (255, 255) where all lines have mid-range targets and sufficient
 * length, so neither value triggers forcing or infeasibility.
 */
TEST(FailedLiteralProberTest, ProbeCellBothFeasible) {
    auto store = makeMidRangeStore();
    PropagationEngine propagator(store);
    BranchingController brancher(store, propagator);
    Sha256HashVerifier hasher(kS);
    FailedLiteralProber prober(store, propagator, brancher, hasher);

    // Use interior cell (255, 255) — all 8 lines have length >= 256 and mid-range targets
    const auto result = prober.probeCell(255, 255);
    EXPECT_EQ(result.row, 255);
    EXPECT_EQ(result.col, 255);
    EXPECT_EQ(result.forcedValue, 255);
    EXPECT_FALSE(result.bothInfeasible);
}

/**
 * @brief When row target=0, assigning 1 makes rho<0 — probeCell forces 0.
 *
 * All sums are 0. With expected hashes set for all-zero rows, probing (0,0)=0 is
 * feasible (cascading propagation fills entire matrix to 0, hash check passes).
 * Probing (0,0)=1 is infeasible (rho < 0). So value 0 is forced.
 */
TEST(FailedLiteralProberTest, ProbeCellForcesZero) {
    auto store = makeAllZeroStore();
    PropagationEngine propagator(store);
    BranchingController brancher(store, propagator);
    Sha256HashVerifier hasher(kS);
    setAllZeroExpectedHashes(hasher);
    FailedLiteralProber prober(store, propagator, brancher, hasher);

    const auto result = prober.probeCell(0, 0);
    EXPECT_EQ(result.row, 0);
    EXPECT_EQ(result.col, 0);
    EXPECT_EQ(result.forcedValue, 0);
    EXPECT_FALSE(result.bothInfeasible);
}

/**
 * @brief When anti-diagonal target=1 with len=1, assigning 0 forces rho>u — probeCell forces 1.
 *
 * Anti-diagonal 0 has length 1, containing only cell (0,0). Target=1 means the cell
 * must be 1. Assigning 0 makes rho = 1 > u = 0 after that assignment, which is infeasible.
 * Assigning 1 satisfies the constraint. So value 1 is forced.
 */
TEST(FailedLiteralProberTest, ProbeCellForcesOne) {
    std::vector<std::uint16_t> rowSums(kS, 0);
    std::vector<std::uint16_t> colSums(kS, 0);
    std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    // Anti-diagonal 0 = cell (0,0) only, target = 1
    antiDiagSums[0] = 1;
    rowSums[0] = 1;
    colSums[0] = 1;
    diagSums[510] = 1; // diagonal for (0,0)
    // Set LTP targets for cell (0,0): each LTP line containing (0,0) needs target 1.
    std::vector<std::uint16_t> ltp1Sums(kS, 0);
    std::vector<std::uint16_t> ltp2Sums(kS, 0);
    std::vector<std::uint16_t> ltp3Sums(kS, 0);
    std::vector<std::uint16_t> ltp4Sums(kS, 0);
    std::vector<std::uint16_t> ltp5Sums(kS, 0);
    std::vector<std::uint16_t> ltp6Sums(kS, 0);
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

    auto store = makeStore(rowSums, colSums, diagSums, antiDiagSums,
                           ltp1Sums, ltp2Sums, ltp3Sums, ltp4Sums,
                      ltp5Sums, ltp6Sums);
    PropagationEngine propagator(store);
    BranchingController brancher(store, propagator);
    Sha256HashVerifier hasher(kS);

    // Set expected hashes: row 0 has cell (0,0) = 1, rest = 0.
    // All other rows are all-zero.
    const std::array<std::uint64_t, 8> zeroRow{};
    const auto zeroDigest = hasher.computeHash(zeroRow);
    for (std::uint16_t r = 0; r < kS; ++r) {
        hasher.setExpected(r, zeroDigest);
    }
    // Row 0 with cell (0,0) = 1: bit 0 is at word 0, bit 63 (MSB-first)
    std::array<std::uint64_t, 8> row0WithOne{};
    row0WithOne[0] = std::uint64_t{1} << 63; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    hasher.setExpected(0, hasher.computeHash(row0WithOne));

    FailedLiteralProber prober(store, propagator, brancher, hasher);

    const auto result = prober.probeCell(0, 0);
    EXPECT_EQ(result.row, 0);
    EXPECT_EQ(result.col, 0);
    EXPECT_EQ(result.forcedValue, 1);
    EXPECT_FALSE(result.bothInfeasible);
}

/**
 * @brief When the constraint store is contradictory for both values, bothInfeasible is true.
 *
 * Row 0 target=0 makes assigning 1 infeasible (rho < 0).
 * Anti-diagonal 0 target=1 with len=1 makes assigning 0 infeasible (rho > u).
 * Both values fail, so bothInfeasible is true.
 */
TEST(FailedLiteralProberTest, ProbeCellBothInfeasible) {
    const std::vector<std::uint16_t> rowSums(kS, 0);
    const std::vector<std::uint16_t> colSums(kS, 0);
    const std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    // Row 0 target = 0 contradicts anti-diagonal 0 target = 1 for cell (0,0)
    antiDiagSums[0] = 1;

    auto store = makeStore(rowSums, colSums, diagSums, antiDiagSums);
    PropagationEngine propagator(store);
    BranchingController brancher(store, propagator);
    Sha256HashVerifier hasher(kS);
    FailedLiteralProber prober(store, propagator, brancher, hasher);

    const auto result = prober.probeCell(0, 0);
    EXPECT_EQ(result.row, 0);
    EXPECT_EQ(result.col, 0);
    EXPECT_TRUE(result.bothInfeasible);
}

// ---------------------------------------------------------------------------
// probeToFixpoint
// ---------------------------------------------------------------------------

/**
 * @brief All-zero sums: probeToFixpoint forces all cells to 0.
 */
TEST(FailedLiteralProberTest, ProbeToFixpointAllZeros) {
    auto store = makeAllZeroStore();
    PropagationEngine propagator(store);
    BranchingController brancher(store, propagator);
    Sha256HashVerifier hasher(kS);
    setAllZeroExpectedHashes(hasher);
    FailedLiteralProber prober(store, propagator, brancher, hasher);

    const auto result = prober.probeToFixpoint();
    EXPECT_GE(result, 0);

    // All cells should be assigned
    for (std::uint16_t r = 0; r < kS; ++r) {
        EXPECT_EQ(store.getStatDirect(r).unknown, 0)
            << "Row " << r << " should have no unassigned cells";
        if (store.getStatDirect(r).unknown != 0) {
            return;
        }
    }
}

/**
 * @brief Contradictory store: probeToFixpoint returns -1.
 */
TEST(FailedLiteralProberTest, ProbeToFixpointContradiction) {
    const std::vector<std::uint16_t> rowSums(kS, 0);
    const std::vector<std::uint16_t> colSums(kS, 0);
    const std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    // Row 0 target = 0 contradicts anti-diagonal 0 target = 1
    antiDiagSums[0] = 1;

    auto store = makeStore(rowSums, colSums, diagSums, antiDiagSums);
    PropagationEngine propagator(store);
    BranchingController brancher(store, propagator);
    Sha256HashVerifier hasher(kS);
    FailedLiteralProber prober(store, propagator, brancher, hasher);

    const auto result = prober.probeToFixpoint();
    EXPECT_EQ(result, -1);
}

// ---------------------------------------------------------------------------
// probeAlternate
// ---------------------------------------------------------------------------

/**
 * @brief probeAlternate returns true for a feasible value.
 */
TEST(FailedLiteralProberTest, ProbeAlternateFeasible) {
    auto store = makeMidRangeStore();
    PropagationEngine propagator(store);
    BranchingController brancher(store, propagator);
    Sha256HashVerifier hasher(kS);
    FailedLiteralProber prober(store, propagator, brancher, hasher);

    // With mid-range sums, assigning 0 to interior cell (255,255) should be feasible
    EXPECT_TRUE(prober.probeAlternate(255, 255, 0));
}

/**
 * @brief probeAlternate returns false for an infeasible value.
 */
TEST(FailedLiteralProberTest, ProbeAlternateInfeasible) {
    auto store = makeAllZeroStore();
    PropagationEngine propagator(store);
    BranchingController brancher(store, propagator);
    Sha256HashVerifier hasher(kS);
    FailedLiteralProber prober(store, propagator, brancher, hasher);

    // All sums = 0: assigning 1 to any cell is infeasible (rho goes negative)
    EXPECT_FALSE(prober.probeAlternate(0, 0, 1));
}

// ---------------------------------------------------------------------------
// State invariance
// ---------------------------------------------------------------------------

/**
 * @brief probeCell does not mutate the constraint store state.
 *
 * After probing, all cells should still be unassigned and line statistics unchanged.
 */
TEST(FailedLiteralProberTest, ProbeDoesNotMutateState) {
    auto store = makeMidRangeStore();
    PropagationEngine propagator(store);
    BranchingController brancher(store, propagator);
    Sha256HashVerifier hasher(kS);
    FailedLiteralProber prober(store, propagator, brancher, hasher);

    // Record state before probing
    const auto stateBefore = store.getCellState(255, 255);
    const auto rowUnknownBefore = store.getStatDirect(255).unknown;

    static_cast<void>(prober.probeCell(255, 255));

    // State should be identical
    EXPECT_EQ(store.getCellState(255, 255), stateBefore);
    EXPECT_EQ(store.getStatDirect(255).unknown, rowUnknownBefore);

    // Check a few more cells to be sure
    EXPECT_EQ(store.getCellState(0, 0), CellState::Unassigned);
    EXPECT_EQ(store.getCellState(1, 0), CellState::Unassigned);
}
