/**
 * @file EnumerationController_dfs.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief EnumerationController::dfs implementation - iterative DFS for solution enumeration.
 */
#include "decompress/Solvers/EnumerationController.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/IBranchingController.h"
#include "decompress/Solvers/IPropagationEngine.h"
#include "decompress/Solvers/LineID.h"
#include "decompress/Solvers/PropagationEngine.h"

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
     * Inline per-row SHA-256 verification prunes the search at every row boundary.
     * Final full-matrix verification is offloaded to AsyncHashPipeline.
     *
     * @param callback Called for each verified solution found.
     * @param stop Set to true when callback returns false to halt enumeration.
     * @throws None
     */
    void EnumerationController::dfs(const SolutionCallback &callback, bool &stop) {
        // Devirtualize: cast to concrete final type for all hot-path calls
        auto &cs = static_cast<ConstraintStore &>(*store_); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        // One-time devirtualization: use concrete PropagationEngine* in hot loop
        auto *cpuProp = dynamic_cast<PropagationEngine *>(propagator_.get());

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
        stack.reserve(261121);
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

            const std::uint8_t v = order[frame.nextValue++]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            frame.token = brancher_->saveUndoPoint();

            // Assign the cell and record it on the undo stack
            cs.assign(frame.r, frame.c, v);
            brancher_->recordAssignment(frame.r, frame.c);

            // Propagate constraints from affected lines
            bool feasible = false;
            if (cpuProp != nullptr) {
                // Fast path: inline feasibility check, skip queue machinery for no-forcing case
                feasible = cpuProp->tryPropagateCell(frame.r, frame.c);
            } else {
                // MetalPropagationEngine fallback: original virtual dispatch
                const auto lines = cs.getLinesForCell(frame.r, frame.c);
                (*propagator_).reset();
                feasible = propagator_->propagate(
                    std::span<const LineID>{lines.lines.data(), static_cast<std::size_t>(lines.count)});
            }

            if (!feasible) {
                continue;
            }

            // Record forced assignments on the undo stack
            const auto &forced = (cpuProp != nullptr)
                ? cpuProp->getForcedAssignments()
                : propagator_->getForcedAssignments();
            for (const auto &a : forced) {
                brancher_->recordAssignment(a.r, a.c);
            }

            // Inline row-hash verification: when a row becomes fully assigned,
            // immediately check its SHA-256 against the expected lateral hash.
            bool hashFailed = false;
            if (cs.getStatDirect(frame.r).unknown == 0) {
                if (!hasher_->verifyRow(frame.r, cs.getRow(frame.r))) {
                    hashFailed = true;
                }
            }
            if (!hashFailed) {
                for (const auto &a : forced) {
                    if (a.r != frame.r && cs.getStatDirect(a.r).unknown == 0) {
                        if (!hasher_->verifyRow(a.r, cs.getRow(a.r))) {
                            hashFailed = true;
                            break;
                        }
                    }
                }
            }
            if (hashFailed) {
                continue;
            }

            // Find the next unassigned cell
            const auto nextCell = brancher_->nextCell();
            if (!nextCell.has_value()) {
                // All cells assigned and all row hashes verified inline --
                // submit for final full-matrix hash verification via pipeline
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
