/**
 * @file PropagationEngine_reset.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief PropagationEngine::reset implementation.
 */
#include "decompress/Solvers/PropagationEngine.h"

namespace crsce::decompress::solvers {
    /**
     * @name reset
     * @brief Clear the list of forced assignments.
     * @throws None
     */
    void PropagationEngine::reset() {
        forced_.clear();
    }
} // namespace crsce::decompress::solvers
