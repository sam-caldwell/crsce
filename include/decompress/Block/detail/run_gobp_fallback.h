/**
 * @file run_gobp_fallback.h
 * @brief GOBP fallback orchestration extracted from solve_block for testability and readability.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <span>

#include "decompress/Csm/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {

    /**
     * @name run_gobp_fallback
     * @brief Run the GOBP fallback with a simple multi‑phase schedule; updates snapshot metrics.
     * @param csm Cross‑Sum Matrix to update.
     * @param st Constraint state (updated in place).
     * @param det DeterministicElimination instance for short propagations.
     * @param baseline_csm Baseline CSM for restart adoption.
     * @param baseline_st Baseline constraints for restart adoption.
     * @param lh LH bytes for prefix verification during restarts.
     * @param lsm Row targets (unused by simplified fallback; kept for signature stability).
     * @param vsm Column targets (unused by simplified fallback; kept for signature stability).
     * @param dsm Diagonal targets (unused by simplified fallback).
     * @param xsm Anti-diagonal targets (unused by simplified fallback).
     * @param snap Snapshot for timings and status updates.
     * @param valid_bits Number of meaningful bits (unused by simplified fallback).
     * @return bool True if det.solved() is achieved; false otherwise.
     */
    bool run_gobp_fallback(Csm &csm,
                       ConstraintState &st,
                       DeterministicElimination &det,
                       Csm &baseline_csm,
                       ConstraintState &baseline_st,
                       std::span<const std::uint8_t> lh,
                       std::span<const std::uint16_t> lsm,
                       std::span<const std::uint16_t> vsm,
                       std::span<const std::uint16_t> dsm,
                       std::span<const std::uint16_t> xsm,
                       BlockSolveSnapshot &snap,
                       std::uint64_t valid_bits);

} // namespace crsce::decompress::detail
