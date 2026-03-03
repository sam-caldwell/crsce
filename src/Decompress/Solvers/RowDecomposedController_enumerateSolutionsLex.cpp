/**
 * @file RowDecomposedController_enumerateSolutionsLex.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief RowDecomposedController::enumerateSolutionsLex -- global probability-guided DFS.
 */
#include "decompress/Solvers/RowDecomposedController.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "common/Csm/Csm.h"
#include "common/Generator/Generator.h"
#include "common/O11y/O11y.h"
#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/IBranchingController.h"
#include "decompress/Solvers/IPropagationEngine.h"
#include "decompress/Solvers/LineID.h"
#include "decompress/Solvers/ProbabilityEstimator.h"
#include "decompress/Solvers/PropagationEngine.h"

namespace crsce::decompress::solvers {

    namespace {

        /**
         * @struct ProbDfsFrame
         * @name ProbDfsFrame
         * @brief One level of the explicit global DFS stack.
         */
        struct ProbDfsFrame {
            /**
             * @name orderIdx
             * @brief Index into the global cellOrder vector.
             */
            std::uint32_t orderIdx;

            /**
             * @name nextValue
             * @brief Next value to try: 0 (first), 1 (second), or 2 (exhausted).
             */
            std::uint8_t nextValue;

            /**
             * @name token
             * @brief Undo save point before this frame's assignment.
             */
            IBranchingController::UndoToken token;

            /**
             * @name row
             * @brief Row index of the cell at orderIdx (cached for hash checks).
             */
            std::uint16_t row;

            /**
             * @name col
             * @brief Column index of the cell at orderIdx (cached for assignment).
             */
            std::uint16_t col;

            /**
             * @name preferred
             * @brief Preferred branch value from probability estimate.
             */
            std::uint8_t preferred;
        };

    } // anonymous namespace

    /**
     * @name enumerateSolutionsLex
     * @brief Global probability-guided DFS yielding CSM solutions.
     *
     * After initial propagation, computes probability scores for all unassigned
     * cells globally and sorts by confidence descending. Performs DFS through the
     * cell ordering, trying the preferred value first. Cross-row hash verification
     * prunes subtrees as soon as any row becomes fully assigned.
     *
     * @return A Generator<Csm> that yields solutions one at a time.
     * @throws None
     */
    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    auto RowDecomposedController::enumerateSolutionsLex() -> crsce::common::Generator<crsce::common::Csm> {
        auto &cs = static_cast<ConstraintStore &>(*store_); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        auto *cpuProp = dynamic_cast<PropagationEngine *>(propagator_.get());

        // --- Initial propagation: queue all 3064 lines ---
        std::vector<LineID> allLines;
        constexpr std::uint16_t kNumDiags = (2 * kS) - 1;
        allLines.reserve(kS + kS + kNumDiags + kNumDiags);
        for (std::uint16_t i = 0; i < kS; ++i) {
            allLines.push_back({.type = LineType::Row, .index = i});
        }
        for (std::uint16_t i = 0; i < kS; ++i) {
            allLines.push_back({.type = LineType::Column, .index = i});
        }
        for (std::uint16_t i = 0; i < kNumDiags; ++i) {
            allLines.push_back({.type = LineType::Diagonal, .index = i});
        }
        for (std::uint16_t i = 0; i < kNumDiags; ++i) {
            allLines.push_back({.type = LineType::AntiDiagonal, .index = i});
        }

        (*propagator_).reset();
        if (!propagator_->propagate(allLines)) {
            ::crsce::o11y::O11y::instance().event("solver_dfs_infeasible",
                {{"phase", "initial_propagation"}});
            co_return;
        }

        // Record initial forced assignments on the undo stack
        const auto &initialForced = propagator_->getForcedAssignments();
        for (const auto &a : initialForced) {
            brancher_->recordAssignment(a.r, a.c);
        }
        ::crsce::o11y::O11y::instance().event("solver_dfs_initial_propagation",
            {{"forced_cells", std::to_string(initialForced.size())}});

        // --- Check if fully determined by initial propagation ---
        bool allAssigned = true;
        for (std::uint16_t r = 0; r < kS; ++r) {
            if (cs.getStatDirect(r).unknown != 0) {
                allAssigned = false;
                break;
            }
        }
        if (allAssigned) {
            // Verify all row hashes
            for (std::uint16_t r = 0; r < kS; ++r) {
                if (!hasher_->verifyRow(r, cs.getRow(r))) {
                    ::crsce::o11y::O11y::instance().event("solver_dfs_infeasible",
                        {{"phase", "initial_hash_check"}, {"row", std::to_string(r)}});
                    co_return;
                }
            }
            ::crsce::o11y::O11y::instance().event("solver_dfs_fully_determined");
            co_yield buildCsm();
            co_return;
        }

        // --- Compute global cell ordering by probability confidence ---
        const ProbabilityEstimator estimator(cs);
        auto cellOrder = estimator.computeGlobalCellScores();

        ::crsce::o11y::O11y::instance().event("solver_dfs_cell_ordering",
            {{"unassigned_cells", std::to_string(cellOrder.size())}});

        // Find the first unassigned cell in the ordering
        std::uint32_t startIdx = 0;
        while (startIdx < cellOrder.size() &&
               cs.getCellState(cellOrder[startIdx].row, cellOrder[startIdx].col) != CellState::Unassigned) {
            ++startIdx;
        }
        if (startIdx >= cellOrder.size()) {
            co_yield buildCsm();
            co_return;
        }

        // --- Global DFS loop ---
        std::vector<ProbDfsFrame> stack;
        stack.reserve(cellOrder.size());
        stack.push_back({startIdx, 0, 0,
                         cellOrder[startIdx].row,    // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                         cellOrder[startIdx].col,    // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                         cellOrder[startIdx].preferred}); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)

        std::uint64_t iterations = 0;
        std::uint64_t hashMismatches = 0;
        const auto dfsStart = std::chrono::steady_clock::now();
        auto windowStart = dfsStart;

        while (!stack.empty()) {
            auto &frame = stack.back();

            // Undo previous value's assignment before trying the next
            if (frame.nextValue > 0) {
                brancher_->undoToSavePoint(frame.token);
            }

            // Both values exhausted -- backtrack
            if (frame.nextValue >= 2) {
                stack.pop_back();
                continue;
            }

            // Determine branch value: preferred first, then alternate
            const std::uint8_t v = (frame.nextValue == 0)
                ? frame.preferred
                : static_cast<std::uint8_t>(1 - frame.preferred);
            frame.nextValue++;
            frame.token = brancher_->saveUndoPoint();

            // Assign and record
            cs.assign(frame.row, frame.col, v);
            brancher_->recordAssignment(frame.row, frame.col);

            // Propagate constraints
            bool feasible = false;
            if (cpuProp != nullptr) {
                feasible = cpuProp->tryPropagateCell(frame.row, frame.col);
            } else {
                const auto lines = cs.getLinesForCell(frame.row, frame.col);
                (*propagator_).reset();
                feasible = propagator_->propagate(lines);
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

            // --- Cross-row hash verification ---
            bool hashFailed = false;

            // Check the directly-assigned cell's row
            if (cs.getStatDirect(frame.row).unknown == 0) {
                if (!hasher_->verifyRow(frame.row, cs.getRow(frame.row))) {
                    hashFailed = true;
                    ++hashMismatches;
                }
            }

            // Check rows completed by forced propagation
            if (!hashFailed) {
                for (const auto &a : forced) {
                    if (a.r != frame.row && cs.getStatDirect(a.r).unknown == 0) {
                        if (!hasher_->verifyRow(a.r, cs.getRow(a.r))) {
                            hashFailed = true;
                            ++hashMismatches;
                            break;
                        }
                    }
                }
            }

            if (hashFailed) {
                continue; // prune this subtree
            }

            // O11y rate logging every 1M iterations
            ++iterations;
            if ((iterations & 0xFFFFF) == 0) { // every ~1M
                const auto now = std::chrono::steady_clock::now();
                const auto windowUs = std::chrono::duration_cast<std::chrono::microseconds>(
                    now - windowStart).count();
                const auto totalUs = std::chrono::duration_cast<std::chrono::microseconds>(
                    now - dfsStart).count();
                const double windowRate = windowUs > 0
                    ? 1048576.0 * 1e6 / static_cast<double>(windowUs) : 0.0;
                const double avgRate = totalUs > 0
                    ? static_cast<double>(iterations) * 1e6 / static_cast<double>(totalUs) : 0.0;
                windowStart = now;
                ::crsce::o11y::O11y::instance().event("solver_dfs_iterations",
                    {{"iterations", std::to_string(iterations)},
                     {"depth", std::to_string(stack.size())},
                     {"iter_per_sec", std::to_string(static_cast<std::uint64_t>(windowRate))},
                     {"avg_iter_per_sec", std::to_string(static_cast<std::uint64_t>(avgRate))},
                     {"hash_mismatches", std::to_string(hashMismatches)}});
            }

            // Find the next unassigned cell in the ordering
            auto nextIdx = frame.orderIdx + 1;
            while (nextIdx < cellOrder.size() &&
                   cs.getCellState(cellOrder[nextIdx].row, cellOrder[nextIdx].col) != CellState::Unassigned) {
                ++nextIdx;
            }

            if (nextIdx >= static_cast<std::uint32_t>(cellOrder.size())) {
                // All cells assigned -- yield the solution
                co_yield buildCsm();
                // Continue DFS for additional solutions (enumeration)
            } else {
                stack.push_back({nextIdx, 0, 0,
                                 cellOrder[nextIdx].row,    // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                                 cellOrder[nextIdx].col,    // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                                 cellOrder[nextIdx].preferred}); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            }
        }

        // Summary event on exit
        {
            const auto now = std::chrono::steady_clock::now();
            const auto totalUs = std::chrono::duration_cast<std::chrono::microseconds>(
                now - dfsStart).count();
            const double avgRate = totalUs > 0
                ? static_cast<double>(iterations) * 1e6 / static_cast<double>(totalUs) : 0.0;
            ::crsce::o11y::O11y::instance().event("solver_dfs_complete",
                {{"total_iterations", std::to_string(iterations)},
                 {"avg_iter_per_sec", std::to_string(static_cast<std::uint64_t>(avgRate))},
                 {"total_hash_mismatches", std::to_string(hashMismatches)}});
        }
    }

} // namespace crsce::decompress::solvers
