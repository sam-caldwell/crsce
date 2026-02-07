/**
 * @file run_restart_worker.h
 * @brief Declaration for parallel restart worker (mini-GOBP with jitter).
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell
 */
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {

/**
 * @name run_restart_worker
 * @brief Per-restart parallel worker that jitter-perturbs a baseline state and runs mini-GOBP phases.
 * @param wi Worker index.
 * @param baseline_csm Baseline CSM to copy for each trial.
 * @param baseline_st Baseline state to copy for each trial.
 * @param c_winner Out: adopted winning CSM when success occurs.
 * @param st_winner Out: adopted winning state when success occurs.
 * @param adopted Shared flag indicating winner adoption has happened.
 * @param adopt_mu Mutex guarding adoption writes to winner outputs.
 * @param next_idx Shared counter of next task index to process.
 * @param tasks Total number of tasks to consider.
 * @param lh LH digest span.
 * @param restart_seed Base seed for local RNG jitter.
 * @param S Board size (CSM::kS).
 * @param base_valid Current valid prefix rows.
 * @param max_ms Time budget per worker in milliseconds.
 * @param ev_phase_wi Out: per-phase events for worker.
 * @param ev_total_wi Out: total event for worker.
 * @param kPerturbBase Base jitter magnitude.
 * @param kPerturbStep Per-restart jitter increment.
 * @return void
 */
void run_restart_worker(std::size_t wi,
                        const Csm &baseline_csm,
                        const ConstraintState &baseline_st,
                        Csm &c_winner,
                        ConstraintState &st_winner,
                        std::atomic<bool> &adopted,
                        std::mutex &adopt_mu,
                        std::atomic<std::size_t> &next_idx,
                        std::size_t tasks,
                        const std::span<const std::uint8_t> lh,
                        const std::uint64_t restart_seed,
                        const std::size_t S,
                        const std::size_t base_valid,
                        const std::size_t max_ms,
                        std::array<BlockSolveSnapshot::ThreadEvent,4> &ev_phase_wi,
                        BlockSolveSnapshot::ThreadEvent &ev_total_wi,
                        const double kPerturbBase,
                        const double kPerturbStep);

} // namespace crsce::decompress::detail
