/**
 * @file BranchingController.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Concrete branching controller with undo stack for canonical DFS enumeration.
 */
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "decompress/Solvers/IBranchingController.h"
#include "decompress/Solvers/IConstraintStore.h"
#include "decompress/Solvers/IPropagationEngine.h"

namespace crsce::decompress::solvers {
    /**
     * @class BranchingController
     * @name BranchingController
     * @brief Manages DFS branching in row-major order with a save/restore undo stack.
     *
     * Selects the first unassigned cell in row-major order. Manages an undo stack
     * that records all assignments (both explicit branches and forced propagations)
     * to enable efficient backtracking.
     */
    class BranchingController final : public IBranchingController {
    public:
        /**
         * @name kS
         * @brief Matrix dimension.
         */
        static constexpr std::uint16_t kS = 127;

        /**
         * @name BranchingController
         * @brief Construct a branching controller.
         * @param store Reference to the constraint store.
         * @param propagator Reference to the propagation engine.
         * @throws None
         */
        BranchingController(IConstraintStore &store, IPropagationEngine &propagator);

        [[nodiscard]] auto nextCell() const
            -> std::optional<std::pair<std::uint16_t, std::uint16_t>> override;
        auto saveUndoPoint() -> UndoToken override;
        void undoToSavePoint(UndoToken token) override;
        [[nodiscard]] auto branchOrder() const -> std::array<std::uint8_t, 2> override;

        /**
         * @name recordAssignment
         * @brief Record a cell assignment on the undo stack for later rollback.
         * @param r Row index.
         * @param c Column index.
         * @throws None
         */
        void recordAssignment(std::uint16_t r, std::uint16_t c);

    private:
        /**
         * @struct UndoEntry
         * @name UndoEntry
         * @brief Records one cell assignment for undo purposes.
         */
        struct UndoEntry {
            /**
             * @name r
             * @brief Row index.
             */
            std::uint16_t r;

            /**
             * @name c
             * @brief Column index.
             */
            std::uint16_t c;
        };

        /**
         * @name store_
         * @brief Reference to the constraint store.
         */
        IConstraintStore &store_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

        /**
         * @name propagator_
         * @brief Reference to the propagation engine.
         */
        IPropagationEngine &propagator_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

        /**
         * @name undoStack_
         * @brief Stack of cell assignments for backtracking.
         */
        std::vector<UndoEntry> undoStack_;

        /**
         * @name minUnassignedRow_
         * @brief Hint for nextCell() scan start. Avoids re-scanning completed rows.
         */
        mutable std::uint16_t minUnassignedRow_{0};
    };
} // namespace crsce::decompress::solvers
