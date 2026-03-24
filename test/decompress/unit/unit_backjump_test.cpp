/**
 * @file unit_backjump_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Unit tests for B.1.7 lightweight backjump behaviour in RowDecomposedController.
 *
 * Tests focus on two observable invariants of the backjump mechanism:
 *
 *   1. Correctness: the solver still finds the correct solution (or no solution) when
 *      the backjump code path is exercised.  Backjumping must never skip a valid
 *      solution or leave the constraint store in a dirty state.
 *
 *   2. Termination: when every row hash is wrong (no solution exists), the solver must
 *      terminate and yield zero solutions rather than looping indefinitely.  The
 *      backjump mechanism must exhaust the search space correctly even when all
 *      hash checks fail.
 *
 * Correctness of valid-solution cases (all-zeros, structured patterns) is already
 * covered by row_decomposed_controller_test.cpp; those tests remain the canonical
 * regression suite for solution quality.
 */
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>
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
    constexpr std::uint16_t kS       = 511;
    constexpr std::uint16_t kNumDiags = (2 * kS) - 1;

    /**
     * @brief Build a heap-allocated all-zero ConstraintStore (every row/col/diag target = 0).
     */
    std::unique_ptr<ConstraintStore> makeAllZeroStore() {
        return std::make_unique<ConstraintStore>(
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
        );
    }

    /**
     * @brief Compute and set the correct SHA-256 digest for the all-zero row on every row.
     */
    void setCorrectAllZeroHashes(Sha256HashVerifier &hasher) {
        const std::array<std::uint64_t, 2> zeroRow{};
        const auto digest = hasher.computeHash(zeroRow);
        for (std::uint16_t r = 0; r < kS; ++r) {
            hasher.setExpected(r, digest);
        }
    }

    /**
     * @brief Set deliberately wrong hashes on every row (SHA-256 of a nonzero row).
     *
     * This guarantees that any completed row fails its hash check, exercising the
     * backjump code path on every hash verification.
     */
    void setWrongHashes(Sha256HashVerifier &hasher) {
        // Hash a row with bit 0 set — guaranteed ≠ all-zero row hash.
        std::array<std::uint64_t, 2> nonzeroRow{};
        nonzeroRow[0] = std::uint64_t{1} << 63; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        const auto wrongDigest = hasher.computeHash(nonzeroRow);
        for (std::uint16_t r = 0; r < kS; ++r) {
            hasher.setExpected(r, wrongDigest);
        }
    }

    /**
     * @brief Build a RowDecomposedController from a heap-allocated store and hasher.
     *
     * Propagator and brancher are constructed referencing the store before ownership
     * is transferred to the controller, so no dangling references occur.
     */
    RowDecomposedController makeController(std::unique_ptr<ConstraintStore> store,
                                           std::unique_ptr<Sha256HashVerifier> hasher) {
        auto &storeRef  = *store;
        auto propagator = std::make_unique<PropagationEngine>(storeRef);
        auto brancher   = std::make_unique<BranchingController>(storeRef, *propagator);
        return RowDecomposedController(
            std::move(store), std::move(propagator),
            std::move(brancher), std::move(hasher)
        );
    }

} // namespace

// ---------------------------------------------------------------------------
// Correctness: correct hashes → solution found
// ---------------------------------------------------------------------------

/**
 * @brief All-zero constraints with correct hashes yield exactly one solution.
 *
 * The backjump path is not triggered here (no hash mismatches), but the test
 * confirms the solver still works correctly after the backjump code was added.
 */
TEST(BackjumpTest, CorrectHashesYieldSolution) {
    auto store  = makeAllZeroStore();
    auto hasher = std::make_unique<Sha256HashVerifier>(kS);
    setCorrectAllZeroHashes(*hasher);

    auto ctrl = makeController(std::move(store), std::move(hasher));

    int count = 0;
    for ([[maybe_unused]] const auto &csm : ctrl.enumerateSolutionsLex()) {
        ++count;
        if (count > 1) { break; } // guard against runaway enumeration in test
    }
    EXPECT_EQ(count, 1);
}

// ---------------------------------------------------------------------------
// Termination: wrong hashes → no solutions, must terminate
// ---------------------------------------------------------------------------

/**
 * @brief All-zero constraints with wrong hashes on every row yield no solutions.
 *
 * The all-zero constraint forces the solver to fill the matrix with zeros via
 * initial propagation.  Every row is then completed and hash-checked immediately.
 * All checks fail (wrong digests).  The backjump code path is exercised on every
 * failure.  The solver must terminate and yield zero solutions rather than
 * looping.
 *
 * This test will hang if the backjump or chronological-backtrack termination is
 * broken.  A timeout in the test runner catches that case.
 */
TEST(BackjumpTest, WrongHashesYieldNoSolutions) {
    auto store  = makeAllZeroStore();
    auto hasher = std::make_unique<Sha256HashVerifier>(kS);
    setWrongHashes(*hasher);

    auto ctrl = makeController(std::move(store), std::move(hasher));

    int count = 0;
    for ([[maybe_unused]] const auto &csm : ctrl.enumerateSolutionsLex()) {
        ++count;
    }
    EXPECT_EQ(count, 0);
}
