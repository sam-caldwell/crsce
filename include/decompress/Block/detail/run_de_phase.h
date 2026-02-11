/**
 * @file run_de_phase.h
 * @brief Execute Deterministic Elimination phase with timing/metrics.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <span>
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"

namespace crsce::decompress::detail {
    /**
     * @name run_de_phase
     * @brief Execute DE iterations with periodic snapshot updates and prefix commits.
     * @param det DeterministicElimination instance (solver).
     * @param st Constraint state (updated in place).
     * @param csm Cross‑Sum Matrix (updated in place).
     * @param snap Snapshot to update with timings/counters.
     * @param lh LH bytes used by prefix verification.
     * @param max_iters Maximum DE iterations.
     * @return bool False on exception; true otherwise.
     */
    bool run_de_phase(DeterministicElimination &det,
                      ConstraintState &st,
                      Csm &csm,
                      BlockSolveSnapshot &snap,
                      std::span<const std::uint8_t> lh,
                      int max_iters);
}
