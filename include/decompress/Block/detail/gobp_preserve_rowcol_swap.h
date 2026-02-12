/**
 * @file gobp_preserve_rowcol_swap.h
 * @brief 2x2 rectangle swap micro-pass that preserves row/col while improving DSM/XSM/LH.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"

namespace crsce::decompress::detail {
    /**
     * @name gobp_preserve_rowcol_swap
     * @brief Sample up to `sample_rects` rectangles and apply up to `accept_limit` swaps
     *        that strictly reduce a DSM/XSM cost (tie-break: improves LH on affected rows),
     *        while preserving row/column sums. Uses only unlocked cells.
     * @param csm Cross‑Sum Matrix (updated in place).
     * @param st Residual state (R/U) updated for diag/xdiag indices touched.
     * @param lh LH bytes for per-row verification tie-breaker.
     * @param dsm Target per-diagonal sums.
     * @param xsm Target per-anti-diagonal sums.
     * @param sample_rects Maximum rectangles to evaluate this pass.
     * @param accept_limit Maximum accepted swaps to apply this pass.
     * @return std::size_t Number of swaps applied.
     */
    std::size_t gobp_preserve_rowcol_swap(Csm &csm,
                                          ConstraintState &st,
                                          std::span<const std::uint8_t> lh,
                                          std::span<const std::uint16_t> dsm,
                                          std::span<const std::uint16_t> xsm,
                                          unsigned sample_rects,
                                          unsigned accept_limit);
}

