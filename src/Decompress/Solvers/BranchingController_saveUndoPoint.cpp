/**
 * @file BranchingController_saveUndoPoint.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief BranchingController::saveUndoPoint implementation.
 */
#include "decompress/Solvers/BranchingController.h"

namespace crsce::decompress::solvers {
    /**
     * @name saveUndoPoint
     * @brief Create a save point on the undo stack.
     * @return An opaque token identifying this save point (stack size at time of save).
     * @throws None
     */
    auto BranchingController::saveUndoPoint() -> UndoToken {
        return static_cast<UndoToken>(undoStack_.size());
    }
} // namespace crsce::decompress::solvers
