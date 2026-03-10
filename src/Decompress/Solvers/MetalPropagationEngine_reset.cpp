/**
 * @file MetalPropagationEngine_reset.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief MetalPropagationEngine::reset implementation.
 */
#include "decompress/Solvers/MetalPropagationEngine.h"

namespace crsce::decompress::solvers {
    /**
     * @name reset
     * @brief Clear the list of forced assignments.
     */
    void MetalPropagationEngine::reset() {
        forced_.clear();
    }
} // namespace crsce::decompress::solvers
