/**
 * @file unit_failed_literal_prober_deep_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Unit tests for FailedLiteralProber::probeAlternateDeep.
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
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
using crsce::decompress::solvers::kLtpNumLines;
using crsce::decompress::solvers::ltpLineLen;
using crsce::decompress::solvers::PropagationEngine;
using crsce::decompress::solvers::Sha256HashVerifier;

namespace {
    constexpr std::uint16_t kS = 127;
    constexpr std::uint16_t kNumDiags = (2 * kS) - 1;

    /**
     * @brief Build a ConstraintStore with customizable sum vectors.
     */
    ConstraintStore makeStore(const std::vector<std::uint16_t> &rowSums,
                              const std::vector<std::uint16_t> &colSums,
                              const std::vector<std::uint16_t> &diagSums,
                              const std::vector<std::uint16_t> &antiDiagSums) {
        return {rowSums, colSums, diagSums, antiDiagSums,
                std::vector<std::uint16_t>(kS, 0), std::vector<std::uint16_t>(kS, 0),
                std::vector<std::uint16_t>(kS, 0), std::vector<std::uint16_t>(kS, 0),
                std::vector<std::uint16_t>(kS, 0), std::vector<std::uint16_t>(kS, 0)};
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
        return {std::vector<std::uint16_t>(kS, static_cast<std::uint16_t>(kS / 2)),
                std::vector<std::uint16_t>(kS, static_cast<std::uint16_t>(kS / 2)),
                diagSums,
                antiDiagSums,
                ltpSums, ltpSums, ltpSums, ltpSums, ltpSums, ltpSums};
    }

    /**
     * @brief Set expected hashes for all-zero rows on a hasher.
     */
    void setAllZeroExpectedHashes(Sha256HashVerifier &hasher) {
        const std::array<std::uint64_t, 2> zeroRow{};
        const auto zeroDigest = hasher.computeHash(zeroRow);
        for (std::uint16_t r = 0; r < kS; ++r) {
            hasher.setExpected(r, zeroDigest);
        }
    }
} // namespace

// ---------------------------------------------------------------------------
// probeAlternateDeep at depth 1 (equivalent to probeAlternate)
// ---------------------------------------------------------------------------

/**
 * @brief At depth=1, probeAlternateDeep returns true for a feasible value (same as probeAlternate).
 */
TEST(FailedLiteralProberDeepTest, Depth1FeasibleMatchesProbeAlternate) {
    auto store = makeMidRangeStore();
    PropagationEngine propagator(store);
    BranchingController brancher(store, propagator);
    Sha256HashVerifier hasher(kS);
    FailedLiteralProber prober(store, propagator, brancher, hasher);

    EXPECT_TRUE(prober.probeAlternateDeep(63, 63, 0, 1));
}

/**
 * @brief At depth=1, probeAlternateDeep returns false for an infeasible value.
 */
TEST(FailedLiteralProberDeepTest, Depth1InfeasibleMatchesProbeAlternate) {
    auto store = makeAllZeroStore();
    PropagationEngine propagator(store);
    BranchingController brancher(store, propagator);
    Sha256HashVerifier hasher(kS);
    FailedLiteralProber prober(store, propagator, brancher, hasher);

    // All sums = 0: assigning 1 is infeasible
    EXPECT_FALSE(prober.probeAlternateDeep(0, 0, 1, 1));
}

// ---------------------------------------------------------------------------
// probeAlternateDeep at depth 2
// ---------------------------------------------------------------------------

/**
 * @brief At depth=2, probeAlternateDeep detects infeasibility for value 1 in all-zeros store.
 *
 * The all-zeros constraint makes any assignment of 1 immediately infeasible at depth 1,
 * so depth 2 also detects it.
 */
TEST(FailedLiteralProberDeepTest, Depth2InfeasibleAllZeros) {
    auto store = makeAllZeroStore();
    PropagationEngine propagator(store);
    BranchingController brancher(store, propagator);
    Sha256HashVerifier hasher(kS);
    FailedLiteralProber prober(store, propagator, brancher, hasher);

    EXPECT_FALSE(prober.probeAlternateDeep(0, 0, 1, 2));
}

/**
 * @brief At depth=2, probeAlternateDeep returns true for a feasible value in mid-range store.
 */
TEST(FailedLiteralProberDeepTest, Depth2FeasibleMidRange) {
    auto store = makeMidRangeStore();
    PropagationEngine propagator(store);
    BranchingController brancher(store, propagator);
    Sha256HashVerifier hasher(kS);
    FailedLiteralProber prober(store, propagator, brancher, hasher);

    EXPECT_TRUE(prober.probeAlternateDeep(63, 63, 0, 2));
}

// ---------------------------------------------------------------------------
// probeAlternateDeep at depth 4 (max)
// ---------------------------------------------------------------------------

/**
 * @brief At depth=4, probeAlternateDeep correctly detects infeasibility in all-zeros store.
 */
TEST(FailedLiteralProberDeepTest, Depth4InfeasibleAllZeros) {
    auto store = makeAllZeroStore();
    PropagationEngine propagator(store);
    BranchingController brancher(store, propagator);
    Sha256HashVerifier hasher(kS);
    FailedLiteralProber prober(store, propagator, brancher, hasher);

    EXPECT_FALSE(prober.probeAlternateDeep(0, 0, 1, 4));
}

/**
 * @brief At depth=4, probeAlternateDeep returns true for feasible value in mid-range store.
 */
TEST(FailedLiteralProberDeepTest, Depth4FeasibleMidRange) {
    auto store = makeMidRangeStore();
    PropagationEngine propagator(store);
    BranchingController brancher(store, propagator);
    Sha256HashVerifier hasher(kS);
    FailedLiteralProber prober(store, propagator, brancher, hasher);

    EXPECT_TRUE(prober.probeAlternateDeep(63, 63, 0, 4));
}

// ---------------------------------------------------------------------------
// State invariance
// ---------------------------------------------------------------------------

/**
 * @brief probeAlternateDeep does not mutate the constraint store state.
 */
TEST(FailedLiteralProberDeepTest, DeepProbeDoesNotMutateState) {
    auto store = makeMidRangeStore();
    PropagationEngine propagator(store);
    BranchingController brancher(store, propagator);
    Sha256HashVerifier hasher(kS);
    FailedLiteralProber prober(store, propagator, brancher, hasher);

    const auto stateBefore = store.getCellState(63, 63);
    const auto rowUnknownBefore = store.getStatDirect(63).unknown;

    static_cast<void>(prober.probeAlternateDeep(63, 63, 0, 3));

    EXPECT_EQ(store.getCellState(63, 63), stateBefore);
    EXPECT_EQ(store.getStatDirect(63).unknown, rowUnknownBefore);
    EXPECT_EQ(store.getCellState(0, 0), CellState::Unassigned);
}

// ---------------------------------------------------------------------------
// Hash verification at depth
// ---------------------------------------------------------------------------

/**
 * @brief probeAlternateDeep at depth=1 catches hash mismatches on completed rows.
 *
 * All-zeros constraints with correct zero-row hashes: probing (0,0)=0 is feasible
 * because the entire matrix forces to 0 and all hashes match.
 */
TEST(FailedLiteralProberDeepTest, HashCheckAtDepth1) {
    auto store = makeAllZeroStore();
    PropagationEngine propagator(store);
    BranchingController brancher(store, propagator);
    Sha256HashVerifier hasher(kS);
    setAllZeroExpectedHashes(hasher);
    FailedLiteralProber prober(store, propagator, brancher, hasher);

    // Probing value 0 should be feasible (all zeros match hashes)
    EXPECT_TRUE(prober.probeAlternateDeep(0, 0, 0, 1));
    // Probing value 1 should be infeasible (rho goes negative)
    EXPECT_FALSE(prober.probeAlternateDeep(0, 0, 1, 1));
}
