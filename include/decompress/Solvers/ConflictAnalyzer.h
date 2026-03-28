/**
 * @file ConflictAnalyzer.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief BFS-based conflict analyzer for CDCL non-chronological backjumping.
 *
 * ConflictAnalyzer traverses the ReasonGraph via BFS starting from the cells
 * of the failed row.  It follows propagation antecedent chains to identify the
 * highest-depth decision frame (strictly below the conflict level) that
 * contributed to the hash failure.  That frame is the 1-UIP backjump target.
 *
 * Memory: the visited-epoch table (~2 MB) is heap-allocated in the constructor
 * to avoid placing it on the coroutine frame.
 */
#pragma once

#include <cstdint>
#include <vector>

#include "decompress/Solvers/BackjumpTarget.h"
#include "decompress/Solvers/ReasonGraph.h"

namespace crsce::decompress::solvers {

    /**
     * @class ConflictAnalyzer
     * @brief Performs BFS on the ReasonGraph to find the 1-UIP backjump target.
     *
     * One instance is constructed per DFS invocation and reused across all
     * conflicts.  The epoch-based visited table amortises the reset cost.
     */
    class ConflictAnalyzer final {
    public:
        /**
         * @name kS
         * @brief Matrix dimension.
         */
        static constexpr std::uint16_t kS = 127;

        /**
         * @name ConflictAnalyzer
         * @brief Construct a ConflictAnalyzer bound to the given ReasonGraph.
         * @param reasonGraph The reason graph to query during conflict analysis.
         * @throws std::bad_alloc if heap allocation fails.
         */
        explicit ConflictAnalyzer(const ReasonGraph &reasonGraph);

        /**
         * @name analyzeHashFailure
         * @brief Find the 1-UIP backjump target for a hash failure at conflictDepth.
         *
         * Performs BFS from all cells in failedRow, following propagation
         * antecedent chains.  Finds the maximum stackDepth strictly less than
         * conflictDepth that participates in the reason chain.
         *
         * @param failedRow     Row index whose SHA-1 verification failed.
         * @param conflictDepth DFS stack depth (stack.size() - 1) at time of failure.
         * @return BackjumpTarget with valid=true and targetDepth set to the jump
         *         frame index, or valid=false if no valid target exists.
         * @throws None
         */
        [[nodiscard]] BackjumpTarget analyzeHashFailure(std::uint16_t failedRow,
                                                        std::uint32_t conflictDepth) const;

    private:
        /**
         * @name reasonGraph_
         * @brief The reason graph to query.
         */
        const ReasonGraph &reasonGraph_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

        /**
         * @name visitedEpoch_
         * @brief Per-cell epoch counter for O(1) visited-set reset.
         *        visitedEpoch_[r * kS + c] == epoch_ means cell (r,c) is visited.
         */
        mutable std::vector<std::uint64_t> visitedEpoch_;

        /**
         * @name epoch_
         * @brief Monotonically increasing call counter.  Incremented each
         *        analyzeHashFailure() call to implicitly reset the visited set.
         */
        mutable std::uint64_t epoch_{0};
    };

} // namespace crsce::decompress::solvers
