/**
 * @file ReasonGraph.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Flat per-cell antecedent table for CDCL reason-graph tracking.
 *
 * ReasonGraph maintains a 511×511 array of CellAntecedent records — one per cell
 * in the CSM.  The DFS loop records every assignment (decision or propagated) and
 * removes the record on undo (via BranchingController::undoToSavePoint).
 *
 * ConflictAnalyzer reads the graph to find the responsible decision frame when a
 * hash failure occurs, enabling non-chronological backjumping.
 */
#pragma once

#include <cstdint>
#include <vector>

#include "decompress/Solvers/CellAntecedent.h"

namespace crsce::decompress::solvers {

    /**
     * @class ReasonGraph
     * @brief Manages a flat 511×511 array of CellAntecedent records.
     *
     * All operations are O(1) per cell.  The backing store is heap-allocated to
     * avoid placing ~3 MB on the coroutine frame.
     */
    class ReasonGraph final {
    public:
        /**
         * @name kS
         * @brief Matrix dimension.
         */
        static constexpr std::uint16_t kS = 511;

        /**
         * @name ReasonGraph
         * @brief Default-construct and zero-initialise all 511×511 entries.
         * @throws std::bad_alloc if heap allocation fails.
         */
        ReasonGraph();

        /**
         * @name recordDecision
         * @brief Mark cell (r, c) as a DFS branching decision at the given stack depth.
         * @param r          Row index in [0, kS).
         * @param c          Column index in [0, kS).
         * @param stackDepth DFS frame index (stack.size() - 1) when assigned.
         * @throws None
         */
        void recordDecision(std::uint16_t r, std::uint16_t c, std::uint32_t stackDepth);

        /**
         * @name recordPropagated
         * @brief Mark cell (r, c) as forced by a constraint line at the given stack depth.
         * @param r             Row index in [0, kS).
         * @param c             Column index in [0, kS).
         * @param stackDepth    DFS frame index active when propagation was triggered.
         * @param flatLineIdx   Flat ConstraintStore line index of the forcing line.
         * @throws None
         */
        void recordPropagated(std::uint16_t r, std::uint16_t c,
                              std::uint32_t stackDepth, std::uint32_t flatLineIdx);

        /**
         * @name unrecord
         * @brief Clear the antecedent entry for cell (r, c) on undo.
         * @param r Row index in [0, kS).
         * @param c Column index in [0, kS).
         * @throws None
         */
        void unrecord(std::uint16_t r, std::uint16_t c);

        /**
         * @name getAntecedent
         * @brief Read the antecedent record for cell (r, c).
         * @param r Row index in [0, kS).
         * @param c Column index in [0, kS).
         * @return Const reference to the CellAntecedent entry.
         * @throws None
         */
        [[nodiscard]] const CellAntecedent &getAntecedent(std::uint16_t r, std::uint16_t c) const;

    private:
        /**
         * @name table_
         * @brief Flat row-major antecedent table.  Entry at (r, c) is table_[r * kS + c].
         */
        std::vector<CellAntecedent> table_;
    };

} // namespace crsce::decompress::solvers
