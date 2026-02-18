/**
 * @file execute_radditz_and_validate.h
 * @brief Run Radditz Sift and validate column sums vs VSM + row invariant vs LSM.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/CrossSum/LateralSumMatrix.h"
#include "decompress/CrossSum/VerticalSumMatrix.h"

namespace crsce::decompress::detail {
    /**
     * @name execute_radditz_and_validate
     * @brief Execute Radditz and ensure column sums equal VSM and rows remain equal to LSM.
     * @param csm Cross‑Sum Matrix to update.
     * @param st Constraint state (updated by lateral swaps only as needed).
     * @param snap Snapshot to update with timings and status.
     * @param lsm Row-sum targets (span).
     * @param vsm Column-sum targets (span).
     * @return bool True if post-conditions hold; false otherwise.
     */
    bool execute_radditz_and_validate(Csm &csm,
                                      ConstraintState &st,
                                      BlockSolveSnapshot &snap,
                                      const ::crsce::decompress::xsum::LateralSumMatrix &lsm,
                                      const ::crsce::decompress::xsum::VerticalSumMatrix &vsm);
}
