/**
 * @file EnumerationController_dfs.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief EnumerationController::dfs implementation - iterative DFS for solution enumeration.
 */
#include "decompress/Solvers/EnumerationController.h"

#include <array>
#include <cstdint>
#include <vector>

#include "decompress/Solvers/IBranchingController.h"
#include "decompress/Solvers/IConstraintStore.h"
#include "decompress/Solvers/IHashVerifier.h"
#include "decompress/Solvers/IPropagationEngine.h"

namespace crsce::decompress::solvers {

    namespace {
        /**
         * @struct DfsFrame
         * @brief One level of the explicit DFS stack.
         *
         * Each frame represents a branching decision point for a single cell.
         * The frame tracks which value (0 or 1) to try next and the undo token
         * saved before the current value's assignment.
         */
        struct DfsFrame {
            std::uint16_t r;       ///< Row index of the branching cell.
            std::uint16_t c;       ///< Column index of the branching cell.
            std::uint8_t nextValue; ///< Next value to try: 0, 1, or 2 (exhausted).
            IBranchingController::UndoToken token; ///< Save point before the current value's assignment.
        };

        /**
         * @name checkPreviousRowHash
         * @brief Check whether a completed previous row passes hash verification.
         * @param store Constraint store.
         * @param hasher Hash verifier.
         * @param r Row being entered (check row r-1).
         * @param c Column of the cell being entered.
         * @return True if no pruning needed; false if hash mismatch detected.
         */
        auto checkPreviousRowHash(IConstraintStore &store, IHashVerifier &hasher,
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
     * @name dfs
     * @brief Iterative DFS traversal for canonical lex-order solution enumeration.
     *
     * Uses an explicit heap-allocated stack instead of call-stack recursion to
     * avoid stack overflow on deep search trees (up to 261,121 cells).
     *
     * @param callback Called for each solution found.
     * @param stop Set to true when callback returns false to halt enumeration.
     * @throws None
     */
    void EnumerationController::dfs(const SolutionCallback &callback, bool &stop) {
        // Find the first unassigned cell
        const auto firstCell = brancher_->nextCell();
        if (!firstCell.has_value()) {
            // All cells already assigned by initial propagation
            const auto csm = buildCsm();
            if (!callback(csm)) {
                stop = true;
            }
            return;
        }

        const auto [firstR, firstC] = firstCell.value();

        // Check previous row hash for the first branching cell
        if (!checkPreviousRowHash(*store_, *hasher_, firstR, firstC)) {
            return;
        }

        const std::array<std::uint8_t, 2> order = brancher_->branchOrder();

        std::vector<DfsFrame> stack;
        stack.push_back({firstR, firstC, 0, 0});

        while (!stack.empty() && !stop) {
            auto &frame = stack.back();

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
                continue;
            }

            // Find the next unassigned cell
            const auto nextCell = brancher_->nextCell();
            if (!nextCell.has_value()) {
                // All cells assigned -- complete solution
                const auto csm = buildCsm();
                if (!callback(csm)) {
                    stop = true;
                }
                continue; // undo happens at the top of the next iteration
            }

            const auto [nr, nc] = nextCell.value();

            // Check previous row hash before descending into the child cell
            if (!checkPreviousRowHash(*store_, *hasher_, nr, nc)) {
                continue;
            }

            // Push child frame
            stack.push_back({nr, nc, 0, 0});
        }

        // Cleanup: undo any remaining state if we stopped early
        if (!stack.empty()) {
            brancher_->undoToSavePoint(stack.front().token);
        }
    }
} // namespace crsce::decompress::solvers
