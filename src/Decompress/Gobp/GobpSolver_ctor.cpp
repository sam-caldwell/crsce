/**
 * @file GobpSolver_ctor.cpp
 * @brief Implementation of GobpSolver constructor.
 * © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Gobp/GobpSolver.h"
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"

namespace crsce::decompress {
    /**
     * @name GobpSolver::GobpSolver
     * @brief Construct a GOBP solver over a CSM.
     * @param csm Target CSM to update (bits/locks and data layer for beliefs)
     * @param state Line residual constraints (R and U for row/col/diag/xdiag)
     * @param damping Exponential smoothing on beliefs in [0,1). 0=no smoothing, 0.5=blend.
     * @param assign_confidence Threshold in (0.5,1] to assign 1 if >=, 0 if <= 1-threshold.
     * @return void
     */
    GobpSolver::GobpSolver(Csm &csm, ConstraintState &state,
                           const double damping,
                           const double assign_confidence)
        : csm_(csm), st_(state), damping_(clamp01(damping)), assign_confidence_(clamp_conf(assign_confidence)) {}
} // namespace crsce::decompress
