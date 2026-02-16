/**
 * @file pre_polish_audit_and_restart_on_contradiction.h
 * @brief Declare audit_and_restart_on_contradiction helper.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <span>
#include <cstdint>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {
    /**
     * @name audit_and_restart_on_contradiction
     * @brief Audit current state and optionally restart on contradiction.
     * @param csm_out Out: CSM to update with new values.
     * @param st Out: Constraint state to update with new values.
     * @param lh Left-hand side of the constraint.
     * @param baseline_csm CSM to compare against.
     * @param baseline_st Constraint state to compare against.
     * @param snap Snapshot of the block solve state.
     * @param rs Restart seed.
     * @param valid_bits Number of valid bits in the block.
     * @param cooldown_ticks Number of ticks to wait before restarting.
     * @param since_last_restart Out: Number of ticks since last restart.
     * @return true on success; false on contradiction.
     */
    bool audit_and_restart_on_contradiction(Csm &csm_out,
                                            ConstraintState &st,
                                            std::span<const std::uint8_t> lh,
                                            Csm &baseline_csm,
                                            ConstraintState &baseline_st,
                                            BlockSolveSnapshot &snap,
                                            int rs,
                                            std::uint64_t valid_bits,
                                            int cooldown_ticks,
                                            int &since_last_restart);
}
