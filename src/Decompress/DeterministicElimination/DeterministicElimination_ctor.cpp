/**
 * @file DeterministicElimination_ctor.cpp
 * @brief Implementation of DeterministicElimination constructor.
 * © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"

#include <cstddef>

namespace crsce::decompress {
    /**
     * @name DeterministicElimination::DeterministicElimination
     * @brief Construct a deterministic elimination solver over a CSM.
     * @param csm Target cross-sum matrix to solve.
     * @param state Residual constraints (R and U across all lines).
     * @return void
     */
    DeterministicElimination::DeterministicElimination(Csm &csm, ConstraintState &state)
        : csm_(csm), st_(state) {
        validate_bounds(st_);
    }
} // namespace crsce::decompress
