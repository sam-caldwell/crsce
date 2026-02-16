/**
 * @file execute_bitsplash_and_validate.h
 * @brief Run BitSplash and validate row sums vs LSM.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <span>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {
    /**
     * @name execute_bitsplash_and_validate
     * @brief Execute BitSplash and ensure each row’s 1-count equals LSM; updates snapshot.
     * @param csm Cross‑Sum Matrix to update.
     * @param st Constraint state (unused by BitSplash; kept for signature uniformity).
     * @param snap Snapshot with lsm targets and timing fields.
     * @param lsm Span of row targets.
     * @return bool False if row sums mismatch; true otherwise.
     */
    bool execute_bitsplash_and_validate(Csm &csm,
                                        ConstraintState &st,
                                        BlockSolveSnapshot &snap,
                                        std::span<const std::uint16_t> lsm);
}
