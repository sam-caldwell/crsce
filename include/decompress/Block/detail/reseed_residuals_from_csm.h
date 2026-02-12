/**
 * @file reseed_residuals_from_csm.h
 * @brief Recompute residual R/U arrays from the current CSM and target cross-sums.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "decompress/Csm/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Utils/detail/index_calc_d.h"
#include "decompress/Utils/detail/index_calc_x.h"

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
                                   std::span<const std::uint16_t> lsm,
                                   std::span<const std::uint16_t> vsm,
                                   std::span<const std::uint16_t> dsm,
                                   std::span<const std::uint16_t> xsm);
}

