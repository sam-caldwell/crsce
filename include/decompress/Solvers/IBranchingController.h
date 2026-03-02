/**
 * @file IBranchingController.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Interface for the branching controller managing DFS traversal and undo stack.
 */
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <utility>

namespace crsce::decompress::solvers {
    /**
     * @class IBranchingController
     * @name IBranchingController
     * @brief Selects the next unassigned cell in row-major order and manages the undo stack.
     *
     * The branching controller ensures canonical lexicographic enumeration by always
     * selecting the first unassigned cell in row-major order and branching 0 before 1.
     */
    class IBranchingController {
    public:
        /**
         * @name UndoToken
         * @brief Opaque handle for a save point on the undo stack.
         */
        using UndoToken = std::uint32_t;

        IBranchingController(const IBranchingController &) = delete;
        IBranchingController &operator=(const IBranchingController &) = delete;
        IBranchingController(IBranchingController &&) = delete;
        IBranchingController &operator=(IBranchingController &&) = delete;

        /**
         * @name ~IBranchingController
         * @brief Virtual destructor.
         */
        virtual ~IBranchingController() = default;

        /**
         * @name nextCell
         * @brief Find the first unassigned cell in row-major order.
         * @return Pair (r, c) of the next cell, or nullopt if all cells are assigned.
         * @throws None
         */
        [[nodiscard]] virtual auto nextCell() const
            -> std::optional<std::pair<std::uint16_t, std::uint16_t>> = 0;

        /**
         * @name saveUndoPoint
         * @brief Create a save point on the undo stack for backtracking.
         * @return An opaque token identifying this save point.
         * @throws None
         */
        virtual auto saveUndoPoint() -> UndoToken = 0;

        /**
         * @name undoToSavePoint
         * @brief Revert all assignments made since the given save point.
         * @param token The save point to restore to.
         * @throws None
         */
        virtual void undoToSavePoint(UndoToken token) = 0;

        /**
         * @name branchOrder
         * @brief Return the branching order for cell values.
         * @return An array of bit values in the order they should be tried.
         *         Default canonical order is {0, 1} for lexicographic enumeration.
         * @throws None
         */
        [[nodiscard]] virtual auto branchOrder() const -> std::array<std::uint8_t, 2> = 0;

    protected:
        IBranchingController() = default;
    };
} // namespace crsce::decompress::solvers
