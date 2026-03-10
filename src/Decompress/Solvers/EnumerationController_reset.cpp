/**
 * @file EnumerationController_reset.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief EnumerationController::reset implementation.
 */
#include "decompress/Solvers/EnumerationController.h"

namespace crsce::decompress::solvers {
    /**
     * @name reset
     * @brief Reset the enumerator (no-op; re-create for new constraint sets).
     * @throws None
     */
    void EnumerationController::reset() {
        // The enumerator is constructed per-block with fresh components.
        // Reset is a no-op; create a new EnumerationController for a new block.
    }
} // namespace crsce::decompress::solvers
