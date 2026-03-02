/**
 * @file BranchingController_undoToSavePoint.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief BranchingController::undoToSavePoint implementation.
 */
#include "decompress/Solvers/BranchingController.h"

#include <algorithm>
#include <cstdint>

namespace crsce::decompress::solvers {
    /**
     * @name undoToSavePoint
     * @brief Revert all assignments made since the given save point.
     * @param token The save point to restore to.
     * @throws None
     */
    void BranchingController::undoToSavePoint(const UndoToken token) {
        std::uint16_t minRow = minUnassignedRow_;
        while (undoStack_.size() > token) {
            const auto &entry = undoStack_.back();
            minRow = std::min(entry.r, minRow);
            store_.unassign(entry.r, entry.c);
            undoStack_.pop_back();
        }
        minUnassignedRow_ = minRow;
    }
} // namespace crsce::decompress::solvers
