/**
 * @file EnumerationController_enumerateSolutionsLex.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief EnumerationController::enumerateSolutionsLex -- coroutine-based generator for DFS solution enumeration.
 */
#include "decompress/Solvers/EnumerationController.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "common/Csm/Csm.h"
#include "common/Generator/Generator.h"
#include "common/O11y/O11y.h"
#include "decompress/Solvers/AsyncHashPipeline.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/IBranchingController.h"
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
    } // anonymous namespace

    /**
     * @name enumerateSolutionsLex
     * @brief Coroutine-based generator yielding feasible CSM solutions in lex order.
     *
     * Performs Algorithm 1 (EnumerateSolutionsLex) using only cross-sum constraints
     * in the DFS hot loop. Hash verification is offloaded to a background thread
     * via AsyncHashPipeline. Verified solutions are yielded to the consumer as they
     * become available, allowing early termination (e.g., for decompression).
     *
     * @return A Generator<Csm> that yields solutions one at a time.
     * @throws None
     */
    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    auto EnumerationController::enumerateSolutionsLex() -> crsce::common::Generator<crsce::common::Csm> {
        // Devirtualize: cast to concrete final type for all hot-path calls
        auto &cs = static_cast<ConstraintStore &>(*store_); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        // Create async hash pipeline for this enumeration run
        pipeline_ = std::make_unique<AsyncHashPipeline>(*hasher_, 8);

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
            pipeline_->shutdown();
            pipeline_.reset();
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
            // All cells already assigned by initial propagation -- yield directly
            // (cross-sums are satisfied; no DFS enumeration needed)
            ::crsce::o11y::O11y::instance().event("solver_fully_determined");
            pipeline_->shutdown();
            pipeline_.reset();
            co_yield buildCsm();
            co_return;
        }

        const auto [firstR, firstC] = firstCell.value();
        const std::array<std::uint8_t, 2> order = brancher_->branchOrder();

        std::vector<CoFrame> stack;
        stack.push_back({firstR, firstC, 0, 0});
        std::uint64_t dfsIterations = 0;
        std::uint64_t failedNotFeasible = 0;
        std::uint64_t candidatesSubmitted = 0;

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
                     {"candidates_submitted", std::to_string(candidatesSubmitted)}});
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
            cs.assign(frame.r, frame.c, v);
            brancher_->recordAssignment(frame.r, frame.c);

            // Propagate constraints from affected lines
            const auto lines = cs.getLinesForCell(frame.r, frame.c);
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

            // Find the next unassigned cell (no hash checks -- offloaded to pipeline)
            const auto nextCell = brancher_->nextCell();
            if (!nextCell.has_value()) {
                // All cells assigned -- cross-sum-valid candidate, submit for hash verification
                ++candidatesSubmitted;
                pipeline_->submit(buildCsm());

                // Yield any verified solutions that are ready (non-blocking)
                while (auto verified = pipeline_->tryNextVerified()) {
                    co_yield verified.value();
                }
                continue; // undo happens at the top of the next iteration
            }

            const auto [nr, nc] = nextCell.value();

            // Push child frame (no row-hash check -- offloaded to pipeline)
            stack.push_back({nr, nc, 0, 0});
        }

        // DFS exhausted: signal pipeline that no more candidates will come
        pipeline_->shutdown();

        // Drain all remaining verified solutions from the pipeline (blocking)
        while (auto verified = pipeline_->nextVerified()) {
            co_yield verified.value();
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
                 {"candidates_submitted", std::to_string(candidatesSubmitted)}});
        }

        pipeline_.reset();

        // Cleanup: undo any remaining state
        if (!stack.empty()) {
            brancher_->undoToSavePoint(stack.front().token);
        }
    }

} // namespace crsce::decompress::solvers
