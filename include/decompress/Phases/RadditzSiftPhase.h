/**
 * @file RadditzSiftPhase.h
 * @brief Column-focused lateral adoption phase (Radditz Sift), fills column deficits with safe placements.
 *        Preserves constraint invariants; does not unlock.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <cstddef>
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::phases {
    /**
     * @name radditz_sift_phase
     * @brief Fill column deficits by adopting 1s at (r,c) for eligible rows r, respecting R/U bounds across families.
     * @param csm CSM to update.
     * @param st Constraint state (R/U residuals), updated in place.
     * @param snap Snapshot for instrumentation; increments micro_adoptions and timing/iteration counters.
     * @param max_cols Optional cap on columns processed in a single call (0 = no cap).
     * @return std::size_t Number of cells adopted (set+locked) across all processed columns.
     */
    std::size_t radditz_sift_phase(Csm &csm,
                                    ConstraintState &st,
                                    BlockSolveSnapshot &snap,
                                    std::size_t max_cols = 0);
}

