/**
 * @file RowDecomposedController_enumerate.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief RowDecomposedController::enumerate implementation (callback wrapper around coroutine).
 */
#include "decompress/Solvers/RowDecomposedController.h"

namespace crsce::decompress::solvers {

    /**
     * @name enumerate
     * @brief Enumerate feasible solutions via callback, wrapping the coroutine generator.
     * @param callback Called for each solution. Return true to continue, false to stop early.
     * @throws None
     */
    void RowDecomposedController::enumerate(const SolutionCallback &callback) {
        for (const auto &csm : enumerateSolutionsLex()) {
            if (!callback(csm)) {
                return;
            }
        }
    }

} // namespace crsce::decompress::solvers
