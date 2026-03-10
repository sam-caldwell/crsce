/**
 * @file RowDecomposedController_reset.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief RowDecomposedController::reset implementation.
 */
#include "decompress/Solvers/RowDecomposedController.h"

namespace crsce::decompress::solvers {

    /**
     * @name reset
     * @brief Reset the enumerator (no-op; re-create for new constraint sets).
     * @throws None
     */
    void RowDecomposedController::reset() {
        // The enumerator is constructed per-block with fresh components.
        // Reset is a no-op; create a new RowDecomposedController for a new block.
    }

} // namespace crsce::decompress::solvers
