/**
 * @file run_hybrid_sift_accel.h
 * @brief Accelerated Hybrid Sift wrapper: routes to GPU-accelerated candidate ranking
 *        when available, otherwise falls back to the existing run_hybrid_sift().
 *        Keeps API parity with the original helper to ease integration.
 * @author Sam Caldwell
 * © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/HashMatrix/LateralHashMatrix.h"
#include "decompress/CrossSum/VerticalSumMatrix.h"
#include "decompress/CrossSum/DiagonalSumMatrix.h"
#include "decompress/CrossSum/AntiDiagonalSumMatrix.h"

namespace crsce::decompress::detail {
    /**
     * @name run_hybrid_sift_accel
     * @brief Execute a bounded hybrid sift guided by beliefs and DSM/XSM error.
     *        If GPU acceleration is compiled in, use it to rank 2x2 swaps; otherwise,
     *        delegate to run_hybrid_sift().
     * @param csm  Cross‑Sum Matrix to update.
     * @param st   Constraint residual state to update.
     * @param snap Snapshot for telemetry.
     * @param lh   Row hash matrix backing data (from LH bytes).
     * @param dsm  Target per‑diag sums.
     * @param xsm  Target per‑anti‑diag sums.
     * @return std::size_t Number of accepted swaps.
     */
    std::size_t run_hybrid_sift_accel(Csm &csm,
                                       crsce::decompress::ConstraintState &st,
                                       crsce::decompress::BlockSolveSnapshot &snap,
                                       const ::crsce::decompress::hashes::LateralHashMatrix &lh,
                                       const ::crsce::decompress::xsum::VerticalSumMatrix &vsm,
                                       const ::crsce::decompress::xsum::DiagonalSumMatrix &dsm,
                                       const ::crsce::decompress::xsum::AntiDiagonalSumMatrix &xsm);
}
