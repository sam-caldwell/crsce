/**
 * @file reseed_residuals_from_csm.h
 * @brief Recompute residual R/U arrays from the current CSM and target cross-sums.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Utils/detail/index_calc_d.h"
#include "decompress/Utils/detail/index_calc_x.h"
#include "decompress/CrossSum/LateralSumMatrix.h"
#include "decompress/CrossSum/VerticalSumMatrix.h"
#include "decompress/CrossSum/DiagonalSumMatrix.h"
#include "decompress/CrossSum/AntiDiagonalSumMatrix.h"

namespace crsce::decompress::detail {
    /**
     * @name reseed_residuals_from_csm
     * @brief Recompute residual counts (R_*, U_*) from the current assignments and locks in `csm`.
     *        This aligns the ConstraintState with the already-applied BitSplash/Radditz adjustments
     *        so that later phases (e.g., GOBP) respect established row/column invariants.
     * @param csm Cross-Sum Matrix containing current bits and locks.
     * @param st Residual state to overwrite with recomputed values.
     * @param lsm Target per-row sums.
     * @param vsm Target per-column sums.
     * @param dsm Target per-diagonal sums.
     * @param xsm Target per-anti-diagonal sums.
     */
    void reseed_residuals_from_csm(const Csm &csm,
                                   ConstraintState &st,
                                   const ::crsce::decompress::xsum::LateralSumMatrix &lsm,
                                   const ::crsce::decompress::xsum::VerticalSumMatrix &vsm,
                                   const ::crsce::decompress::xsum::DiagonalSumMatrix &dsm,
                                   const ::crsce::decompress::xsum::AntiDiagonalSumMatrix &xsm);
}
