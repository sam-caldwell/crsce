/**
 * @file run_final_backtrack_worker.h
 * @brief Declaration for final boundary backtrack worker.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell
 */
#pragma once

#include <atomic>
#include <cstddef>
#include <span>
#include <vector>

#include "decompress/Block/detail/BTTaskPair.h"
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {

/**
 * @name run_final_backtrack_worker
 * @brief Worker for final single-cell boundary backtrack over top-K candidates.
 * @param wi Worker index.
 * @param tasks Tasks to attempt (column, assumed value) as pairs.
 * @param next_idx Shared index allocator.
 * @param found Shared flag indicating a winning candidate was found and adopted.
 * @param lh LH digest span.
 * @param boundary Boundary row index under consideration.
 * @param valid_now Current valid prefix rows.
 * @param S Board size (CSM::kS).
 * @param csm_in Base CSM at time of candidate generation.
 * @param st_in Base state at time of candidate generation.
 * @param adopt_mu Mutex guarding winner adoption.
 * @param c_winner Out: adopted winning CSM when success occurs.
 * @param st_winner Out: adopted winning state when success occurs.
 * @param ev_out Out: event info for worker lifetime.
 * @return void
 */
void run_final_backtrack_worker(std::size_t wi,
                                const std::vector<BTTaskPair> &tasks,
                                std::atomic<std::size_t> &next_idx,
                                std::atomic<bool> &found,
                                const std::span<const std::uint8_t> lh,
                                const std::size_t boundary,
                                const std::size_t valid_now,
                                const std::size_t S,
                                const Csm &csm_in,
                                const ConstraintState &st_in,
                                std::mutex &adopt_mu,
                                Csm &c_winner,
                                ConstraintState &st_winner,
                                BlockSolveSnapshot::ThreadEvent &ev_out);

} // namespace crsce::decompress::detail

