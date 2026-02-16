/**
 * @file run_hybrid_sift.h
 * @brief Hybrid Radditz‑Sift guided by GOBP beliefs (deterministic 2x2 swaps).
 *
 * Replaces DSM/XSM sifts with a unified pass that preserves row/col counts
 * and accepts rectangles when a combined cost over DSM/XSM residual error and
 * belief placement strictly improves.
 */
#pragma once

#include <span>
#include <cstdint>

#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {
    /**
     * @name run_hybrid_sift
     * @brief Execute a bounded, deterministic hybrid sift pass that evaluates
     *        2x2 rectangle swaps and preserves row/col sums while improving
     *        DSM/XSM error; breaks ties using per‑cell belief data.
     *
     * Env flags (all optional):
     *  - CRSCE_HYB_TIMEOUT_MS: wall‑clock budget; 0 disables.
     *  - CRSCE_HYB_MAX_PASSES: max outer passes; 0 = auto (defaults to 8).
     *  - CRSCE_HYB_SAMPLES_PER_PASS: rectangles to evaluate per pass (default 4096).
     *  - CRSCE_HYB_ACCEPTS_PER_PASS: max accepted swaps per pass (default 128).
     *  - CRSCE_HYB_SAT_LOSS_MIN_IMPROVE: if swap breaks satisfied diag/xdiag, require at least
     *       this absolute improvement in total DSM/XSM error (default 2).
     *  - CRSCE_HYB_LOCK_DSM_SAT, CRSCE_HYB_LOCK_XSM_SAT: 1 to disallow breaking satisfied families.
     *  - CRSCE_HYB_ALPHA, CRSCE_HYB_BETA: weights for error vs belief score (default 1.0 each).
     *
     * @param csm  Cross‑Sum Matrix to update.
     * @param st   Constraint state (R/U) updated for touched diag/xdiag indices.
     * @param snap Snapshot for telemetry updates.
     * @param lh   Row hash payload for optional LH tie‑break checks.
     * @param dsm  Target per‑diag sums (size ≥ S).
     * @param xsm  Target per‑anti‑diag sums (size ≥ S).
     * @return std::size_t Number of accepted swaps during this run.
     */
    std::size_t run_hybrid_sift(Csm &csm,
                                 crsce::decompress::ConstraintState &st,
                                 crsce::decompress::BlockSolveSnapshot &snap,
                                 std::span<const std::uint8_t> lh,
                                 std::span<const std::uint16_t> dsm,
                                 std::span<const std::uint16_t> xsm);
}
