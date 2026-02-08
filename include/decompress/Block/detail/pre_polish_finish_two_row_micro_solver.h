/**
 * @file pre_polish_finish_two_row_micro_solver.h
 * @brief Declare two-row boundary micro-solver.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <span>
#include <cstdint>
#include "decompress/Csm/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {
    /**
     * @name finish_two_row_micro_solver
     * @brief Heuristically finish the two-row boundary row if the prefix verifies.
     * @param csm_out Out: CSM to update with new values.
     * @param st Out: ConstraintState to update with new values.
     * @param lh Left-hand side of the constraint.
     * @param baseline_csm CSM to compare against.
     * @param baseline_st ConstraintState to compare against.
     * @param snap Snapshot of the block solve state.
     * @param rs Restart seed.
     * @return true on success; false on failure.
     */
    bool finish_two_row_micro_solver(Csm &csm_out,
                                     ConstraintState &st,
                                     std::span<const std::uint8_t> lh,
                                     Csm &baseline_csm,
                                     ConstraintState &baseline_st,
                                     BlockSolveSnapshot &snap,
                                     int rs);
}
