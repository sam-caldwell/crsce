/**
 * @file EnumerationController_enumerateSolutionsLex.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief EnumerationController::enumerateSolutionsLex -- coroutine-based generator for DFS solution enumeration.
 */
#include "decompress/Solvers/EnumerationController.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "common/Csm/Csm.h"
#include "common/Generator/Generator.h"
#include "common/O11y/O11y.h"
#include "decompress/Solvers/IBranchingController.h"
#include "decompress/Solvers/IConstraintStore.h"
#include "decompress/Solvers/IHashVerifier.h"
#include "decompress/Solvers/IPropagationEngine.h"
#include "decompress/Solvers/LineID.h"

namespace crsce::decompress::solvers {

    namespace {
        /**
         * @struct CoFrame
         * @brief One level of the explicit DFS stack for coroutine-based enumeration.
         *
         * Mirrors DfsFrame from EnumerationController_dfs.cpp but used by the
         * coroutine variant.
         */
        struct CoFrame {
            std::uint16_t r;        ///< Row index of the branching cell.
            std::uint16_t c;        ///< Column index of the branching cell.
            std::uint8_t nextValue;  ///< Next value to try: 0, 1, or 2 (exhausted).
            IBranchingController::UndoToken token; ///< Save point before the current value's assignment.
        };

        /**
         * @name checkRowHash
         * @brief Check whether a completed previous row passes hash verification.
         * @param store Constraint store.
         * @param hasher Hash verifier.
         * @param r Row being entered (check row r-1).
         * @param c Column of the cell being entered.
         * @return True if no pruning needed; false if hash mismatch detected.
         */
        auto checkRowHash(IConstraintStore &store, IHashVerifier &hasher,
                          const std::uint16_t r, const std::uint16_t c) -> bool {
            if (c == 0 && r > 0) {
                if (store.getRowUnknownCount(r - 1) == 0) {
                    const auto rowData = store.getRow(r - 1);
                    if (!hasher.verifyRow(r - 1, rowData)) {
                        return false;
                    }
                }
            }
            return true;
        }
    } // anonymous namespace

    /**
     * @name enumerateSolutionsLex
     * @brief Coroutine-based generator yielding feasible CSM solutions in lex order.
     *
     * Performs the same Algorithm 1 (EnumerateSolutionsLex) as enumerate(), but uses
     * co_yield to lazily produce each solution instead of invoking a callback.
     * @return A Generator<Csm> that yields solutions one at a time.
     * @throws None
     */
    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    auto EnumerationController::enumerateSolutionsLex() -> crsce::common::Generator<crsce::common::Csm> {
        // Initial propagation: queue all lines
        std::vector<LineID> allLines;
        allLines.reserve(kS + kS + ((2 * kS) - 1) + ((2 * kS) - 1));
        for (std::uint16_t i = 0; i < kS; ++i) {
            allLines.push_back({.type = LineType::Row, .index = i});
        }
        for (std::uint16_t i = 0; i < kS; ++i) {
            allLines.push_back({.type = LineType::Column, .index = i});
        }
        for (std::uint16_t i = 0; i < (2 * kS) - 1; ++i) {
            allLines.push_back({.type = LineType::Diagonal, .index = i});
        }
        for (std::uint16_t i = 0; i < (2 * kS) - 1; ++i) {
            allLines.push_back({.type = LineType::AntiDiagonal, .index = i});
        }

        (*propagator_).reset();
        if (!propagator_->propagate(allLines)) {
            ::crsce::o11y::O11y::instance().event("solver_infeasible",
                {{"phase", "initial_propagation"}});
            co_return; // initial state is infeasible
        }

        // Record initial forced assignments on the undo stack
        const auto &initialForced = propagator_->getForcedAssignments();
        for (const auto &a : initialForced) {
            brancher_->recordAssignment(a.r, a.c);
        }
        ::crsce::o11y::O11y::instance().event("solver_initial_propagation",
            {{"forced_cells", std::to_string(initialForced.size())}});

        // Find the first unassigned cell
        const auto firstCell = brancher_->nextCell();
        if (!firstCell.has_value()) {
            // All cells already assigned by initial propagation
            ::crsce::o11y::O11y::instance().event("solver_fully_determined");
            co_yield buildCsm();
            co_return;
        }

        const auto [firstR, firstC] = firstCell.value();

        // Check previous row hash for the first branching cell
        if (!checkRowHash(*store_, *hasher_, firstR, firstC)) {
            co_return;
        }

        const std::array<std::uint8_t, 2> order = brancher_->branchOrder();

        std::vector<CoFrame> stack;
        stack.push_back({firstR, firstC, 0, 0});
        std::uint64_t dfsIterations = 0;
        std::uint64_t failedNotFeasible = 0;
        std::uint64_t failedHashMismatch = 0;

        const auto dfsStart = std::chrono::steady_clock::now();
        auto windowStart = dfsStart;

        while (!stack.empty()) {
            auto &frame = stack.back();

            // Emit DFS progress every 1000 iterations with rate measurement
            if (++dfsIterations % 1000 == 0) {
                const auto now = std::chrono::steady_clock::now();
                const auto windowUs = std::chrono::duration_cast<std::chrono::microseconds>(now - windowStart).count();
                const auto totalUs = std::chrono::duration_cast<std::chrono::microseconds>(now - dfsStart).count();
                const double windowRate = windowUs > 0 ? 1000.0 * 1e6 / static_cast<double>(windowUs) : 0.0;
                const double avgRate = totalUs > 0 ? static_cast<double>(dfsIterations) * 1e6 / static_cast<double>(totalUs) : 0.0;
                windowStart = now;
                ::crsce::o11y::O11y::instance().event("solver_dfs_iterations",
                    {{"count", std::to_string(dfsIterations)},
                     {"depth", std::to_string(stack.size())},
                     {"iter_per_sec", std::to_string(static_cast<std::uint64_t>(windowRate))},
                     {"avg_iter_per_sec", std::to_string(static_cast<std::uint64_t>(avgRate))},
                     {"failed_not_feasible", std::to_string(failedNotFeasible)},
                     {"failed_hash_mismatch", std::to_string(failedHashMismatch)}});
            }

            // Undo previous value's assignment before trying next value or popping
            if (frame.nextValue > 0) {
                brancher_->undoToSavePoint(frame.token);
            }

            // Both values exhausted, backtrack
            if (frame.nextValue >= 2) {
                stack.pop_back();
                continue;
            }

            const std::uint8_t v = order.at(frame.nextValue++); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            frame.token = brancher_->saveUndoPoint();

            // Assign the cell and record it on the undo stack
            store_->assign(frame.r, frame.c, v);
            brancher_->recordAssignment(frame.r, frame.c);

            // Propagate constraints from affected lines
            const auto lines = store_->getLinesForCell(frame.r, frame.c);
            (*propagator_).reset();
            const bool feasible = propagator_->propagate(lines);

            if (!feasible) {
                ++failedNotFeasible;
                continue;
            }

            // Record forced assignments on the undo stack
            const auto &forced = propagator_->getForcedAssignments();
            for (const auto &a : forced) {
                brancher_->recordAssignment(a.r, a.c);
            }

            // Check hash for any rows completed by forced assignments
            bool hashOk = true;
            for (const auto &a : forced) {
                if (store_->getRowUnknownCount(a.r) == 0) {
                    const auto rowData = store_->getRow(a.r);
                    if (!hasher_->verifyRow(a.r, rowData)) {
                        hashOk = false;
                        break;
                    }
                }
            }

            // Check hash for the branching cell's own row
            if (hashOk && store_->getRowUnknownCount(frame.r) == 0) {
                const auto rowData = store_->getRow(frame.r);
                if (!hasher_->verifyRow(frame.r, rowData)) {
                    hashOk = false;
                }
            }

            if (!hashOk) {
                ++failedHashMismatch;
                continue;
            }

            // Find the next unassigned cell
            const auto nextCell = brancher_->nextCell();
            if (!nextCell.has_value()) {
                // All cells assigned -- complete solution
                ::crsce::o11y::O11y::instance().event("solver_solution_found",
                    {{"dfs_iterations", std::to_string(dfsIterations)}});
                co_yield buildCsm();
                ::crsce::o11y::O11y::instance().event("solver_solution_built",
                    {{"dfs_iterations", std::to_string(dfsIterations)}});
                continue; // undo happens at the top of the next iteration
            }

            const auto [nr, nc] = nextCell.value();

            // Check previous row hash before descending into the child cell
            if (!checkRowHash(*store_, *hasher_, nr, nc)) {
                continue;
            }

            // Push child frame
            stack.push_back({nr, nc, 0, 0});
        }
        {
            const auto now = std::chrono::steady_clock::now();
            const auto totalUs = std::chrono::duration_cast<std::chrono::microseconds>(now - dfsStart).count();
            const double avgRate = totalUs > 0 ? static_cast<double>(dfsIterations) * 1e6 / static_cast<double>(totalUs) : 0.0;
            ::crsce::o11y::O11y::instance().event("solver_loop exited",
                {{"dfs_iterations", std::to_string(dfsIterations)},
                 {"elapsed_ms", std::to_string(totalUs / 1000)},
                 {"avg_iter_per_sec", std::to_string(static_cast<std::uint64_t>(avgRate))},
                 {"failed_not_feasible", std::to_string(failedNotFeasible)},
                 {"failed_hash_mismatch", std::to_string(failedHashMismatch)}});
        }

        // Cleanup: undo any remaining state
        if (!stack.empty()) {
            brancher_->undoToSavePoint(stack.front().token);
        }
    }

} // namespace crsce::decompress::solvers
