/**
 * @file RowConstraintPhase.h
 * @brief Row-first constraint satisfaction phase (post-DE), adopts high-confidence row assignments.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <cstddef>
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::phases {
    /**
     * @name row_constraint_phase
     * @brief Greedy row-first adoption: for each row, place up to R_row[r] ones into unlocked cells, guided by
     *        column/diag/xdiag residual pressure and optional belief scores. Updates residuals and locks cells.
     * @param csm CSM to update.
     * @param st Constraint state (R/U residuals), updated in place.
     * @param snap Snapshot for instrumentation; increments micro_adoptions and timing/iteration counters.
     * @param max_rows Optional cap on rows processed in a single call (0 = no cap).
     * @return std::size_t Number of cells adopted (set+locked) across all rows.
     */
    std::size_t row_constraint_phase(Csm &csm,
                                     ConstraintState &st,
                                     BlockSolveSnapshot &snap,
                                     std::size_t max_rows = 0);
}

