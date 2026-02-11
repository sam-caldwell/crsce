/**
 * @file run_restart_worker.cpp
 * @brief Definition of parallel restart worker (mini-GOBP with jitter).
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Block/detail/run_restart_worker.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <random>
#include <span>
#include <string>

#include "decompress/Phases/Gobp/GobpSolver.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Utils/detail/now_ms.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"

namespace crsce::decompress::detail {

/**
 * @name run_restart_worker
 * @brief Per-restart parallel worker that jitter-perturbs a baseline state and runs mini-GOBP phases.
 * @param wi Worker index for naming per-phase/total events.
 * @param baseline_csm Baseline CSM to copy and perturb.
 * @param baseline_st Baseline state to copy for the attempt.
 * @param c_winner Out: adopted CSM on success.
 * @param st_winner Out: adopted state on success.
 * @param adopted Atomic flag set to true when any worker adopts.
 * @param adopt_mu Mutex guarding winner adoption into outputs.
 * @param next_idx Atomic cursor across restart tasks.
 * @param tasks Total number of restart tasks to execute.
 * @param lh Span of LH digest bytes for verification.
 * @param restart_seed Seed for per-restart RNG jitter.
 * @param S CSM dimension.
 * @param base_valid Current verified prefix length.
 * @param max_ms Overall time budget per worker in milliseconds.
 * @param ev_phase_wi Out: four phase event records for this worker.
 * @param ev_total_wi Out: total event record for this worker.
 * @param kPerturbBase Base jitter magnitude.
 * @param kPerturbStep Per-restart jitter increment step.
 * @return void
 */
void run_restart_worker(std::size_t wi, // NOLINT(misc-use-internal-linkage)
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
    const auto t0 = now_ms();
    std::string outcome = "rejected";
    const auto wall_start = std::chrono::steady_clock::now();
    while (!adopted.load(std::memory_order_relaxed)) {
        const std::size_t idx = next_idx.fetch_add(1, std::memory_order_relaxed);
        if (idx >= tasks) { break; }
        const int rr = static_cast<int>(idx) + 1;
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
        std::array<double, 4> dampers{{0.50, 0.10, 0.35, 0.02}};
        std::array<double, 4> confs{{0.995, 0.70, 0.85, 0.55}};
        std::array<int, 4> iters_arr{{3000, 6000, 40000, 200000}};
        const RowHashVerifier v2;
        for (std::size_t ph = 0; ph < 4 && !adopted.load(std::memory_order_relaxed); ++ph) {
            const auto p0 = now_ms();
            GobpSolver gobp(c_try, st_try, dampers.at(ph), confs.at(ph));
            const int max_it = iters_arr.at(ph);
            int guard = 0;
            int verify_tick = 0;
            int verify_period = 1500;
            if (const char *vp = std::getenv("CRSCE_VERIFY_TICK") /* NOLINT(concurrency-mt-unsafe) */; vp && *vp) {
                const int v = std::atoi(vp); if (v > 0) { verify_period = v; }
            }
            int no_prog_iters = 0;
            const int no_prog_limit = 4000;
            while (guard++ < max_it && !adopted.load(std::memory_order_relaxed)) {
                const std::size_t gprog = gobp.solve_step();
                if (gprog == 0) { ++no_prog_iters; } else { no_prog_iters = 0; }
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
            const auto p1 = now_ms();
            auto &ep = ev_phase_wi.at(ph);
            ep.name = std::string("rsw_") + std::to_string(wi) + std::string("_ph") + std::to_string(ph);
            ep.start_ms = p0; ep.stop_ms = p1; ep.outcome = outcome;
            if (adopted.load(std::memory_order_relaxed)) { break; }
        }
    }
    const auto t1 = now_ms();
    auto &et = ev_total_wi;
    et.name = std::string("rsw_total_") + std::to_string(wi);
    et.start_ms = t0; et.stop_ms = t1; et.outcome = outcome;
}

} // namespace crsce::decompress::detail
