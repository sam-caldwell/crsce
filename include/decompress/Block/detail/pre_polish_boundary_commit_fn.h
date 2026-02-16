/**
 * @file pre_polish_boundary_commit_fn.h
 * @brief Declare pre_polish_boundary_commit helper.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <span>
#include <string>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {
    /**
     * @name pre_polish_boundary_commit
     * @brief Pre-polish boundary commit helper function.
     * @param csm_out Out: CSM to update with new values.
     * @param st Out: Constraint state to update.
     * @param lh Left-hand side of the constraint.
     * @param seed Seed for the restart.
     * @param baseline_csm CSM to compare against.
     * @param baseline_st Constraint state to compare against.
     * @param snap Snapshot of the block solve state.
     * @param rs Restart seed.
     * @return true on success; false on contradiction.
     */
    bool pre_polish_boundary_commit(Csm &csm_out,
                                    ConstraintState &st,
                                    std::span<const std::uint8_t> lh,
                                    const std::string &seed,
                                    Csm &baseline_csm,
                                    ConstraintState &baseline_st,
                                    BlockSolveSnapshot &snap,
                                    int rs);
}
