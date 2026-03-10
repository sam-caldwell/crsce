/**
 * @file CellAntecedent.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Per-cell antecedent record for CDCL reason tracking.
 *
 * Every cell in the 511×511 matrix has one CellAntecedent entry.  When a cell
 * is assigned by a DFS branching decision, isDecision is set to true and
 * antecedentLine is left at kNoAntecedent.  When a cell is forced by constraint
 * propagation, isDecision is false and antecedentLine holds the flat ConstraintStore
 * line index of the line that triggered the forcing.
 *
 * stackDepth is the DFS stack depth (= frame index) active at the moment of
 * assignment.  For decisions, this equals the frame index in the DFS stack.
 * For propagated cells, it equals the decision level that initiated the cascade.
 */
#pragma once

#include <cstdint>
#include <limits>

namespace crsce::decompress::solvers {

    /**
     * @struct CellAntecedent
     * @brief Records how a cell was assigned: decision or propagated consequence.
     */
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    struct CellAntecedent {
        /**
         * @name kNoAntecedent
         * @brief Sentinel value: no antecedent line (cell is a decision or unassigned).
         */
        static constexpr std::uint32_t kNoAntecedent =
            std::numeric_limits<std::uint32_t>::max();

        /**
         * @name stackDepth
         * @brief DFS frame index (decision level) when this cell was assigned.
         *        Meaningful only when isAssigned is true.
         */
        std::uint32_t stackDepth{0};

        /**
         * @name antecedentLine
         * @brief Flat ConstraintStore line index that forced this cell.
         *        kNoAntecedent if this cell is a decision (isDecision == true).
         */
        std::uint32_t antecedentLine{kNoAntecedent};

        /**
         * @name isDecision
         * @brief True if this cell was assigned by a DFS branching decision;
         *        false if assigned by constraint propagation.
         */
        bool isDecision{false};

        /**
         * @name isAssigned
         * @brief True if this entry holds valid data (cell is currently assigned).
         *        False for unassigned cells or after unrecord().
         */
        bool isAssigned{false};
    };
    // NOLINTEND(misc-non-private-member-variables-in-classes)

} // namespace crsce::decompress::solvers
