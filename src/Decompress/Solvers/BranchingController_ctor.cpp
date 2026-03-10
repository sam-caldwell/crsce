/**
 * @file BranchingController_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief BranchingController constructor implementation.
 */
#include "decompress/Solvers/BranchingController.h"

#include "decompress/Solvers/IConstraintStore.h"
#include "decompress/Solvers/IPropagationEngine.h"

namespace crsce::decompress::solvers {
    /**
     * @name BranchingController
     * @brief Construct a branching controller.
     * @param store Reference to the constraint store.
     * @param propagator Reference to the propagation engine.
     * @throws None
     */
    BranchingController::BranchingController(IConstraintStore &store, IPropagationEngine &propagator)
        : store_(store), propagator_(propagator) {
        undoStack_.reserve(261121);
    }
} // namespace crsce::decompress::solvers
