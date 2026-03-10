/**
 * @file BranchingController_recordAssignment.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief BranchingController::recordAssignment implementation.
 */
#include "decompress/Solvers/BranchingController.h"

#include <cstdint>

namespace crsce::decompress::solvers {
    /**
     * @name recordAssignment
     * @brief Record a cell assignment on the undo stack for later rollback.
     * @param r Row index.
     * @param c Column index.
     * @throws None
     */
    void BranchingController::recordAssignment(const std::uint16_t r, const std::uint16_t c) {
        undoStack_.push_back({.r = r, .c = c});
    }
} // namespace crsce::decompress::solvers
