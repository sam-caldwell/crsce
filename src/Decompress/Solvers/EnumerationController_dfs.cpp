/**
 * @file EnumerationController_dfs.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief EnumerationController::dfs implementation - iterative DFS for solution enumeration.
 */
#include "decompress/Solvers/EnumerationController.h"

#include <array>
#include <cstdint>
#include <vector>

#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/IBranchingController.h"
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
    } // anonymous namespace

    /**
     * @name dfs
     * @brief Iterative DFS traversal for canonical lex-order solution enumeration.
     *
     * Uses an explicit heap-allocated stack instead of call-stack recursion to
     * avoid stack overflow on deep search trees (up to 261,121 cells).
     * Hash verification is offloaded to a background thread via AsyncHashPipeline.
     *
     * @param callback Called for each verified solution found.
     * @param stop Set to true when callback returns false to halt enumeration.
     * @throws None
     */
    void EnumerationController::dfs(const SolutionCallback &callback, bool &stop) {
        // Devirtualize: cast to concrete final type for all hot-path calls
        auto &cs = static_cast<ConstraintStore &>(*store_); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        // Find the first unassigned cell
        const auto firstCell = brancher_->nextCell();
        if (!firstCell.has_value()) {
            // All cells already assigned by initial propagation -- yield directly
            // (cross-sums are satisfied; no DFS enumeration needed)
            const auto csm = buildCsm();
            if (!callback(csm)) {
                stop = true;
            }
            return;
        }

        const auto [firstR, firstC] = firstCell.value();
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
            cs.assign(frame.r, frame.c, v);
            brancher_->recordAssignment(frame.r, frame.c);

            // Propagate constraints from affected lines
            const auto lines = cs.getLinesForCell(frame.r, frame.c);
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

            // Find the next unassigned cell (no hash checks -- offloaded to pipeline)
            const auto nextCell = brancher_->nextCell();
            if (!nextCell.has_value()) {
                // All cells assigned -- cross-sum-valid candidate, submit for hash verification
                pipeline_->submit(buildCsm());

                // Deliver any verified solutions that are ready (non-blocking)
                while (auto verified = pipeline_->tryNextVerified()) {
                    if (!callback(verified.value())) {
                        stop = true;
                        break;
                    }
                }
                continue; // undo happens at the top of the next iteration
            }

            const auto [nr, nc] = nextCell.value();

            // Push child frame (no row-hash check -- offloaded to pipeline)
            stack.push_back({nr, nc, 0, 0});
        }

        // DFS exhausted or stopped: signal pipeline that no more candidates will come
        pipeline_->shutdown();

        // Drain remaining verified solutions (blocking)
        if (!stop) {
            while (auto verified = pipeline_->nextVerified()) {
                if (!callback(verified.value())) {
                    stop = true;
                    break;
                }
            }
        }

        // Cleanup: undo any remaining state if we stopped early
        if (!stack.empty()) {
            brancher_->undoToSavePoint(stack.front().token);
        }
    }
} // namespace crsce::decompress::solvers
