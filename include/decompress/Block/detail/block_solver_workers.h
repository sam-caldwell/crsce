/**
 * @file block_solver_workers.h
 * @brief Helpers for BlockSolver parallel workers and task types.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <random>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Gobp/GobpSolver.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {

/**
 * @name BTTaskPair
 * @brief Boundary backtrack task encoded as (column index, assume_one flag).
 */
using BTTaskPair = std::pair<std::size_t, bool>;

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
inline void run_restart_worker(std::size_t wi,
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
                               const double kPerturbStep) {
    const auto t0 = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch()).count());
    std::string outcome = "rejected";
    const auto wall_start = std::chrono::steady_clock::now();
    while (!adopted.load(std::memory_order_relaxed)) {
        const std::size_t idx = next_idx.fetch_add(1, std::memory_order_relaxed);
        if (idx >= tasks) { break; }
        const int rr = static_cast<int>(idx) + 1; // pick a future restart index for jitter seed
        Csm c_try = baseline_csm; ConstraintState st_try = baseline_st;
        const double jitter = kPerturbBase + (static_cast<double>(rr - 1) * kPerturbStep);
        std::mt19937_64 rng_local(restart_seed + static_cast<std::uint64_t>(rr));
        std::uniform_real_distribution<double> delta(-jitter, jitter);
        for (std::size_t r0 = 0; r0 < S; ++r0) {
            for (std::size_t c0 = 0; c0 < S; ++c0) {
                if (c_try.is_locked(r0, c0)) { continue; }
                const double v = c_try.get_data(r0, c0);
                const double j = delta(rng_local);
                const double nv = std::clamp(v + j, 0.0, 1.0);
                c_try.set_data(r0, c0, nv);
            }
        }
        // Full multiphase mini-GOBP (bounded by time and adoption)
        std::array<double, 4> dampers{{0.50, 0.10, 0.35, 0.02}};
        std::array<double, 4> confs{{0.995, 0.70, 0.85, 0.55}};
        std::array<int, 4> iters_arr{{3000, 6000, 40000, 200000}}; // trimmed per-worker caps
        const RowHashVerifier v2;
        for (std::size_t ph = 0; ph < 4 && !adopted.load(std::memory_order_relaxed); ++ph) {
            const auto p0 = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now().time_since_epoch()).count());
            GobpSolver gobp(c_try, st_try, dampers.at(ph), confs.at(ph));
            const int max_it = iters_arr.at(ph);
            int guard = 0;
            int verify_tick = 0;
            // verification cadence (env, default 1500)
            int verify_period = 1500;
            if (const char *vp = std::getenv("CRSCE_VERIFY_TICK") /* NOLINT(concurrency-mt-unsafe) */; vp && *vp) {
                const int v = std::atoi(vp); if (v > 0) { verify_period = v; }
            }
            // early-stop on low ROI: break phase if no progress for many iterations
            int no_prog_iters = 0;
            const int no_prog_limit = 4000;
            while (guard++ < max_it && !adopted.load(std::memory_order_relaxed)) {
                const std::size_t gprog = gobp.solve_step();
                if (gprog == 0) { ++no_prog_iters; } else { no_prog_iters = 0; }
                // periodic verify to catch prefix growth
                if ((++verify_tick % verify_period) == 0) {
                    if (base_valid < S && st_try.U_row.at(base_valid) == 0 && st_try.R_row.at(base_valid) == 0) {
                        if (v2.verify_row(c_try, lh, base_valid)) {
                            bool expected=false; if (adopted.compare_exchange_strong(expected,true,std::memory_order_acq_rel)) {
                                const std::scoped_lock lk(adopt_mu);
                                c_winner = c_try; st_winner = st_try; outcome = "adopted";
                            }
                        }
                    }
                }
                const auto now = std::chrono::steady_clock::now();
                const std::size_t ms = static_cast<std::size_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - wall_start).count()
                );
                if (ms > max_ms) { break; }
                if (no_prog_iters > no_prog_limit && ms > (max_ms / 4U)) { break; }
            }
            const auto p1 = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now().time_since_epoch()).count());
            auto &ep = ev_phase_wi.at(ph);
            ep.name = std::string("rsw_") + std::to_string(wi) + std::string("_ph") + std::to_string(ph);
            ep.start_ms = p0; ep.stop_ms = p1; ep.outcome = outcome;
            if (adopted.load(std::memory_order_relaxed)) { break; }
        }
    }
    const auto t1 = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch()).count());
    auto &et = ev_total_wi;
    et.name = std::string("rsw_total_") + std::to_string(wi);
    et.start_ms = t0; et.stop_ms = t1; et.outcome = outcome;
}

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
inline void run_final_backtrack_worker(std::size_t wi,
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
                                       BlockSolveSnapshot::ThreadEvent &ev_out) {
    const auto start = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch()).count());
    std::string outcome = "rejected";
    while (!found.load(std::memory_order_relaxed)) {
        const std::size_t idx = next_idx.fetch_add(1, std::memory_order_relaxed);
        if (idx >= tasks.size()) { break; }
        const BTTaskPair t = tasks[idx];
        Csm c_try = csm_in; ConstraintState st_try = st_in;
        const std::size_t c_pick = t.first;
        const bool assume_one = t.second;
        c_try.put(boundary, c_pick, assume_one);
        c_try.lock(boundary, c_pick);
        if (st_try.U_row.at(boundary) > 0) { --st_try.U_row.at(boundary); }
        if (st_try.U_col.at(c_pick) > 0) { --st_try.U_col.at(c_pick); }
        const std::size_t d0 = (c_pick >= boundary) ? (c_pick - boundary) : (c_pick + S - boundary);
        const std::size_t x0 = (boundary + c_pick) % S;
        if (st_try.U_diag.at(d0) > 0) { --st_try.U_diag.at(d0); }
        if (st_try.U_xdiag.at(x0) > 0) { --st_try.U_xdiag.at(x0); }
        if (assume_one) {
            if (st_try.R_row.at(boundary) > 0) { --st_try.R_row.at(boundary); }
            if (st_try.R_col.at(c_pick) > 0) { --st_try.R_col.at(c_pick); }
            if (st_try.R_diag.at(d0) > 0) { --st_try.R_diag.at(d0); }
            if (st_try.R_xdiag.at(x0) > 0) { --st_try.R_xdiag.at(x0); }
        }
        DeterministicElimination det_bt{c_try, st_try};
        static constexpr int bt_iters = 6000;
        for (int it = 0; it < bt_iters; ++it) {
            const std::size_t prog = det_bt.solve_step();
            if (st_try.U_row.at(boundary) == 0 || prog == 0) { break; }
        }
        if (st_try.U_row.at(boundary) == 0) {
            const std::size_t check_rows = std::min<std::size_t>(valid_now + 1, S);
            const RowHashVerifier verifier_try;
            if (check_rows > 0 && verifier_try.verify_rows(c_try, lh, check_rows)) {
                bool expected = false;
                if (found.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
                    const std::scoped_lock lk(adopt_mu);
                    c_winner = c_try; st_winner = st_try; outcome = "adopted";
                }
            }
        }
    }
    const auto stop = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch()).count());
    ev_out.name = std::string("btw_total_") + std::to_string(wi);
    ev_out.start_ms = start; ev_out.stop_ms = stop; ev_out.outcome = outcome;
}

} // namespace crsce::decompress::detail

