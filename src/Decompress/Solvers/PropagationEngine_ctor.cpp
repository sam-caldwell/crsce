/**
 * @file PropagationEngine_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief PropagationEngine constructor implementation.
 */
#include "decompress/Solvers/PropagationEngine.h"

#include "decompress/Solvers/IConstraintStore.h"

namespace crsce::decompress::solvers {
    /**
     * @name PropagationEngine
     * @brief Construct a propagation engine bound to a constraint store.
     * @param store Reference to the constraint store.
     * @throws None
     */
    PropagationEngine::PropagationEngine(IConstraintStore &store) : store_(store) {}
} // namespace crsce::decompress::solvers
