/**
 * @file BranchingController_undoToSavePoint.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief BranchingController::undoToSavePoint implementation.
 */
#include "decompress/Solvers/BranchingController.h"

#include <algorithm>
#include <cstdint>

#include "decompress/Solvers/ConstraintStore.h"

namespace crsce::decompress::solvers {
    /**
     * @name undoToSavePoint
     * @brief Revert all assignments made since the given save point.
     * @param token The save point to restore to.
     * @throws None
     */
    void BranchingController::undoToSavePoint(const UndoToken token) {
        // Devirtualize: store_ is guaranteed to be ConstraintStore (final class)
        auto &cs = static_cast<ConstraintStore &>(store_); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        std::uint16_t minRow = minUnassignedRow_;
        while (undoStack_.size() > token) {
            const auto &entry = undoStack_.back();
            minRow = std::min(entry.r, minRow);
            cs.unassign(entry.r, entry.c);
            undoStack_.pop_back();
        }
        minUnassignedRow_ = minRow;
    }
} // namespace crsce::decompress::solvers
