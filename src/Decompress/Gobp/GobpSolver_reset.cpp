/**
 * @file GobpSolver_reset.cpp
 * @brief Implementation of GobpSolver::reset.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Gobp/GobpSolver.h"

namespace crsce::decompress {
    /**
     * @name GobpSolver::reset
     * @brief Reset internal state and parameters to defaults.
     * @return void
     */
    void GobpSolver::reset() {
        // No internal state beyond parameters; leave data layer untouched per interface contract
    }
} // namespace crsce::decompress
