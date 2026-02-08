/**
 * @file BlockSolver.cpp
 * @brief High-level block solver implementation: reconstruct CSM from LH and cross-sum payloads.
 * @copyright © Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Block/detail/solve_block.h"
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Gobp/GobpSolver.h"
#include "decompress/Utils/detail/verify_cross_sums.h"
#include "decompress/Utils/detail/decode9.tcc"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Block/detail/set_block_solve_snapshot.h"
#include "decompress/Block/detail/pre_polish_commit_valid_prefix.h"
#include "decompress/Block/detail/pre_polish_finish_boundary_row_adaptive.h"
#include "decompress/Block/detail/pre_polish_finish_boundary_row_micro_solver.h"
#include "decompress/Block/detail/pre_polish_finish_boundary_row_segment_solver.h"
#include "decompress/Block/detail/pre_polish_finish_two_row_micro_solver.h"
#include "decompress/Block/detail/BTTaskPair.h"
#include "decompress/Block/detail/run_restart_worker.h"
#include "decompress/Block/detail/run_final_backtrack_worker.h"
#include "decompress/Block/detail/pre_polish_commit_any_verified_rows.h"
#include "decompress/Block/detail/pre_polish_finish_near_complete_top_rows.h"
#include "decompress/Block/detail/pre_polish_finish_near_complete_any_row.h"
#include "decompress/Block/detail/pre_polish_finish_near_complete_top_columns.h"
#include "decompress/Block/detail/pre_polish_finish_near_complete_rows_scored.h"
#include "decompress/Block/detail/pre_polish_finish_near_complete_columns_scored.h"
#include "decompress/Block/detail/pre_polish_audit_and_restart_on_contradiction.h"
#include "decompress/Block/detail/solver_env_read_seed.h"
#include "decompress/Block/detail/solver_env_trace_sumu.h"
#include "decompress/Utils/detail/index_calc.h"

#include <cstddef>
#include <cstdint> //NOLINT
#include <cstdlib>
#include <array>
#include <chrono>
#include <algorithm>
#include <random>
#include <span>
#include <string>
#include <exception>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <utility>
#include <functional>
#include <cmath>

#include "decompress/Block/detail/compute_prefix.h"
#include "common/O11y/event.h"
#include "common/O11y/counter.h"
#include "common/O11y/metric_i64.h"

namespace {
    using crsce::decompress::Csm;
    using crsce::decompress::ConstraintState;
    using crsce::decompress::DeterministicElimination;
    using crsce::decompress::RowHashVerifier;
    using crsce::decompress::detail::run_restart_worker;
    using crsce::decompress::detail::run_final_backtrack_worker;
    using crsce::decompress::detail::BTTaskPair;
    using crsce::decompress::BlockSolveSnapshot;
    using crsce::decompress::detail::read_seed_or_default;
    using crsce::decompress::detail::trace_sumu_enabled;

} // anonymous namespace

#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wreturn-type"
#endif

namespace crsce::decompress {
    /**
     * @name solve_block
     * @brief Reconstruct a block CSM from LH and cross-sum payloads.
     * @param lh Span of 511 chained LH digests (32 bytes each).
     * @param sums Span of 4 serialized cross-sum vectors (2300 bytes total).
     * @param csm_out Output CSM to populate.
     * @param seed Seed string used by LH chain.
     * @param valid_bits Number of meaningful bits in this block. Any cells beyond
     *        this count are treated as padded: they are set to 0 and locked prior
     *        to solving, so DE/GOBP operate only on valid data.
     * @return bool True on success; false if solving or verification fails.
     */
    bool solve_block(const std::span<const std::uint8_t> lh, // NOLINT(readability-function-size)
                     const std::span<const std::uint8_t> sums,
                     Csm &csm_out,
                     const std::string &seed,
                     const std::uint64_t valid_bits) {

        // Logging via macros defined at top of file (no lambdas per ODPCPP)
        constexpr std::size_t S = Csm::kS;
        constexpr std::size_t vec_bytes = 575U;
        (void)seed; // unused with per-row hashing and belief seeding

        const auto lsm = decode_9bit_stream<S>(sums.subspan(0 * vec_bytes, vec_bytes));
        const auto vsm = decode_9bit_stream<S>(sums.subspan(1 * vec_bytes, vec_bytes));
        const auto dsm = decode_9bit_stream<S>(sums.subspan(2 * vec_bytes, vec_bytes));
        const auto xsm = decode_9bit_stream<S>(sums.subspan(3 * vec_bytes, vec_bytes));

        ConstraintState st{};
        st.R_row = lsm;
        st.R_col = vsm;
        st.R_diag = dsm;
        st.R_xdiag = xsm;
        st.U_row.fill(S);
        st.U_col.fill(S);
        st.U_diag.fill(S);
        st.U_xdiag.fill(S);

        // Ensure clean CSM before applying any pre-locks
        csm_out.reset();
        // Early stdout log to indicate start of DE phase
        ::crsce::o11y::event("block_start_de_phase");

        // Pre-lock padded cells (beyond valid_bits) as zeros and decrement unknown counts.
        if (constexpr std::uint64_t total_bits = static_cast<std::uint64_t>(S) * static_cast<std::uint64_t>(S); valid_bits < total_bits) {

            // Optional debug: emit one-line summary when enabled
            static int prelock_dbg = -1; // -1 unset, 0 off, 1 on
            if (prelock_dbg < 0) {
                const char *e = std::getenv("CRSCE_PRELOCK_DEBUG"); // NOLINT(concurrency-mt-unsafe)
                prelock_dbg = (e && *e) ? 1 : 0;
            }

            if (prelock_dbg == 1) {
                const auto start_r = static_cast<std::size_t>(valid_bits / S);
                const auto start_c = static_cast<std::size_t>(valid_bits % S);
                ::crsce::o11y::metric(
                    "prelock_padded_tail",
                    static_cast<std::int64_t>(valid_bits),
                    {
                        {"r", std::to_string(start_r)},
                        {"c", std::to_string(start_c)},
                        {"count", std::to_string(total_bits - valid_bits)}
                    }
                );
            }
            for (std::uint64_t idx = valid_bits; idx < total_bits; ++idx) {
                const auto r = static_cast<std::size_t>(idx / S);
                const auto c = static_cast<std::size_t>(idx % S);
                // Set and lock the padded cell
                csm_out.put(r, c, false);
                csm_out.lock(r, c);
                // Update unknown counts for affected lines
                if (st.U_row.at(r) > 0) {
                    --st.U_row.at(r);
                }
                if (st.U_col.at(c) > 0) {
                    --st.U_col.at(c);
                }
                const std::size_t d = ::crsce::decompress::detail::calc_d(r, c);
                const std::size_t x = ::crsce::decompress::detail::calc_x(r, c);
                if (st.U_diag.at(d) > 0) {
                    --st.U_diag.at(d);
                }
                if (st.U_xdiag.at(x) > 0) {
                    --st.U_xdiag.at(x);
                }
            }
            }
            DeterministicElimination det{csm_out, st};
            // (legacy) flush watchdog removed; o11y sink handles buffering/flush

            // Seed initial beliefs from LSM, balanced by VSM/DSM/XSM residual pressure and add small jitter (±0.02)
            std::uint64_t belief_seed_used = 0;
            {
                belief_seed_used = read_seed_or_default(0x0BADC0FFEEULL);
                std::mt19937_64 rng(belief_seed_used);
                std::uniform_real_distribution<double> noise(-0.02, 0.02);
                constexpr double high = 0.90;
                constexpr double low = 0.10;
                for (std::size_t r = 0; r < S; ++r) {
                    if (st.U_row.at(r) == 0) {
                        continue; // already fully locked (e.g., padded row)
                    }
                    const auto ones = static_cast<std::size_t>(st.R_row.at(r));
                    // Build candidates scored by (residual/unknown) across VSM/DSM/XSM
                    std::vector<std::pair<double, std::size_t>> cand; cand.reserve(S);
                    for (std::size_t c = 0; c < S; ++c) {
                        if (csm_out.is_locked(r, c)) {
                            continue;
                        }
                        const std::size_t d = ::crsce::decompress::detail::calc_d(r, c);
                        const std::size_t x = ::crsce::decompress::detail::calc_x(r, c);
                        const double col_need = static_cast<double>(st.R_col.at(c)) / static_cast<double>(std::max<std::uint16_t>(1, st.U_col.at(c)));
                        const double diag_need = static_cast<double>(st.R_diag.at(d)) / static_cast<double>(std::max<std::uint16_t>(1, st.U_diag.at(d)));
                        const double x_need = static_cast<double>(st.R_xdiag.at(x)) / static_cast<double>(std::max<std::uint16_t>(1, st.U_xdiag.at(x)));
                        const double score = col_need + diag_need + x_need;
                        cand.emplace_back(score, c);
                    }
                    if (cand.empty()) {
                        continue;
                    }
                    std::ranges::sort(cand, std::greater<>{});
                    const std::size_t pick = std::min<std::size_t>(ones, cand.size());
                    // Mark chosen columns as high belief; rest as low
                    std::vector<char> chosen(S, 0);
                    for (std::size_t i = 0; i < pick; ++i) {
                        chosen[cand[i].second] = 1;
                    }
                    for (std::size_t c = 0; c < S; ++c) {
                        if (csm_out.is_locked(r, c)) {
                            continue;
                        }
                        const double base = chosen[c] ? high : low;
                        double v = base + noise(rng);
                        {
                            v = std::max(v, 0.0);
                            v = std::min(v, 1.0);
                        }
                        csm_out.set_data(r, c, v);
                    }
                }
            }

            // Initialize snapshot
            BlockSolveSnapshot snap{};
            snap.S = S;
            snap.iter = 0;
            snap.phase = BlockSolveSnapshot::Phase::init;
            snap.message = "";
            snap.lsm.assign(lsm.begin(), lsm.end());
            snap.vsm.assign(vsm.begin(), vsm.end());
            snap.dsm.assign(dsm.begin(), dsm.end());
            snap.xsm.assign(xsm.begin(), xsm.end());
            snap.U_row.assign(st.U_row.begin(), st.U_row.end());
            snap.U_col.assign(st.U_col.begin(), st.U_col.end());
            snap.U_diag.assign(st.U_diag.begin(), st.U_diag.end());
            snap.U_xdiag.assign(st.U_xdiag.begin(), st.U_xdiag.end());
            {
                std::size_t sumU = 0;
                for (const auto u: st.U_row) {
                    sumU += static_cast<std::size_t>(u);
                }
                snap.unknown_total = sumU;
                snap.solved = (static_cast<std::size_t>(S) * static_cast<std::size_t>(S)) - sumU;
            }
            // record belief seed used
            snap.rng_seed_belief = belief_seed_used;
            set_block_solve_snapshot(snap);

            // Initialize baseline (for restarts/lock-ins)
            Csm baseline_csm = csm_out;
            ConstraintState baseline_st = st;

            // Incremental LH prefix cache for csm_out without introducing extra record definitions
            std::vector<std::uint64_t> pc_ver_seen; // per-row version when last verified
            std::vector<char> pc_ok;                // per-row success flag (nonzero if verified)
            std::size_t pc_prefix_len = 0;          // length of verified prefix [0..pc_prefix_len)

            if (pc_ver_seen.size() != S) {
                pc_ver_seen.assign(S, 0ULL);
                pc_ok.assign(S, 0);
                pc_prefix_len = 0;
            }

            // Hard-coded deterministic elimination iteration cap
            static constexpr int kMaxIters = 60000;

            for (int iter = 0; iter < kMaxIters; ++iter) {
                std::size_t progress = 0;
                // (legacy) flush to stdout removed; metrics are buffered asynchronously
                try {
                    snap.phase = BlockSolveSnapshot::Phase::de;
                    {
                        const auto t0 = std::chrono::steady_clock::now();
                        const std::size_t v = det.solve_step();
                        const auto t1 = std::chrono::steady_clock::now();
                        progress += v;
                        snap.time_de_ms += static_cast<std::size_t>(
                            std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
                        );
                    }
                } catch (const std::exception &e) {
                    snap.iter = static_cast<std::size_t>(iter);
                    snap.message = e.what();
                    snap.U_row.assign(st.U_row.begin(), st.U_row.end());
                    snap.U_col.assign(st.U_col.begin(), st.U_col.end());
                    snap.U_diag.assign(st.U_diag.begin(), st.U_diag.end());
                    snap.U_xdiag.assign(st.U_xdiag.begin(), st.U_xdiag.end());
                    {
                        std::size_t sumU = 0;
                        for (const auto u: st.U_row) {
                            sumU += static_cast<std::size_t>(u);
                        }
                        snap.unknown_total = sumU;
                        snap.solved = (static_cast<std::size_t>(S) * static_cast<std::size_t>(S)) - sumU;
                    }
                    set_block_solve_snapshot(snap);
                    return false;
                }
                // Update snapshot after iteration
                snap.iter = static_cast<std::size_t>(iter);
                snap.U_row.assign(st.U_row.begin(), st.U_row.end());
                snap.U_col.assign(st.U_col.begin(), st.U_col.end());
                snap.U_diag.assign(st.U_diag.begin(), st.U_diag.end());
                snap.U_xdiag.assign(st.U_xdiag.begin(), st.U_xdiag.end());
                {
                    std::size_t sumU = 0;
                    for (const auto u: st.U_row) {
                        sumU += static_cast<std::size_t>(u);
                    }
                    snap.unknown_total = sumU;
                    snap.solved = (static_cast<std::size_t>(S) * static_cast<std::size_t>(S)) - sumU;
                    snap.unknown_history.push_back(sumU);
                    if (trace_sumu_enabled()) {
                        ::crsce::o11y::event(
                            "de_iter_trace",
                            {
                                {"iter", std::to_string(iter)},
                                {"sumU", std::to_string(sumU)},
                                {"progress", std::to_string(progress)}
                            }
                        );
                    }
                    // Periodic DE heartbeat: structured metric
                    constexpr int kDEHeartbeat = 1000;
                    if ((iter % kDEHeartbeat) == 0) {
                        ::crsce::o11y::metric(
                            "de_heartbeat",
                            static_cast<std::int64_t>(progress),
                            {
                                {"iter", std::to_string(iter)},
                                {"sumU", std::to_string(snap.unknown_total)}
                            }
                        );
                    }
                }
                set_block_solve_snapshot(snap);
                // Prefix gating: lock any contiguous valid prefix after improvements
                {
                    {
                        const auto t0_lh = std::chrono::steady_clock::now();
                        const bool anyp = ::crsce::decompress::detail::commit_valid_prefix(
                            csm_out,
                            st,
                            lh,
                            baseline_csm,
                            baseline_st,
                            snap,
                            /*rs*/0
                        );
                        const auto t1_lh = std::chrono::steady_clock::now();
                        snap.time_lh_ms += static_cast<std::size_t>(
                            std::chrono::duration_cast<std::chrono::milliseconds>(t1_lh - t0_lh).count()
                        );
                        if (anyp) {
                            for (int it = 0; it < 50 && !det.solved(); ++it) {
                            const auto t0 = std::chrono::steady_clock::now();
                            const auto vv = det.solve_step();
                            const auto t1 = std::chrono::steady_clock::now();
                            snap.time_de_ms += static_cast<std::size_t>(
                                std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
                            );
                            if (vv == 0) {
                                break;
                            }
                        }
                        }
                    }
                }
                if (det.solved()) {
                    ::crsce::o11y::event(
                        "block_terminating",
                        {
                            {"reason", std::string("possible_solution")}
                        }
                    );
                    break;
                }
                // Optional early stop when no additional progress is made.
                if (progress == 0) {
                    ::crsce::o11y::event("de_steady_state");
                    ::crsce::o11y::metric("de_to_gobp", 1LL);
                    // Force early finisher attempts before GOBP
                    try {
                        (void)::crsce::decompress::detail::finish_boundary_row_adaptive(
                            csm_out,
                            st,
                            lh,
                            baseline_csm,
                            baseline_st,
                            snap,
                            /*rs*/ 0,
                            /*stall_ticks*/600
                        );

                        // Prefer micro-solvers only later in polish; allow early run only with env override
                        bool ms_early = false;
                        if (const char *e = std::getenv("CRSCE_MS_EARLY") /* NOLINT(concurrency-mt-unsafe) */) {
                            ms_early = (*e=='1');
                        }
                        if (ms_early) {
                            (void)::crsce::decompress::detail::finish_boundary_row_micro_solver(
                                csm_out,
                                st,
                                lh,
                                /*baseline*/baseline_csm,
                                /*baseline*/baseline_st,
                                snap,
                                /*rs*/0
                            );
                            (void)::crsce::decompress::detail::finish_boundary_row_segment_solver(
                                csm_out,
                                st,
                                lh,
                                /*baseline*/baseline_csm,
                                /*baseline*/baseline_st,
                                snap,
                                /*rs*/0
                            );
                            (void)::crsce::decompress::detail::finish_two_row_micro_solver(
                                csm_out,
                                st,
                                lh,
                                /*baseline*/baseline_csm,
                                /*baseline*/baseline_st,
                                snap,
                                /*rs*/0
                            );
                        }
                        (void)::crsce::decompress::detail::finish_near_complete_top_rows(
                            csm_out,
                            st,
                            lh,
                            baseline_csm,
                            baseline_st,
                            snap,
                            /*rs*/0,
                            /*top_n*/32,
                            /*top_k_cells*/256
                        );
                        (void)::crsce::decompress::detail::finish_near_complete_top_columns(
                            csm_out,
                            st,
                            lh,
                            baseline_csm,
                            baseline_st,
                            snap,
                            /*rs*/0,
                            /*top_n_cols*/16,
                            /*top_k_cells*/160
                        );
                    } catch (...) {
                        /* ignore */
                    }
                    break;
                }
                ::crsce::o11y::metric(
                    "block_iter_end",
                    static_cast<std::int64_t>(iter),
                    {
                        {"max", std::to_string(kMaxIters)},
                        {"progress", std::to_string(progress)}
                    }
                );
            }
            // If DE has already solved the block, skip GOBP and proceed to verification.
            if (det.solved()) {
                {
                    const auto t0cs = std::chrono::steady_clock::now();
                    const bool okcs = verify_cross_sums(csm_out, lsm, vsm, dsm, xsm);
                    const auto t1cs = std::chrono::steady_clock::now();
                    snap.time_cross_verify_ms += static_cast<std::size_t>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(t1cs - t0cs).count()
                    );
                    if (!okcs) {
                        ::crsce::o11y::event("block_end_verify_fail");
                        ::crsce::o11y::metric("block_end_verify_fail", 1LL);
                        snap.phase = BlockSolveSnapshot::Phase::verify;
                        snap.message = "cross-sum verification failed";
                        set_block_solve_snapshot(snap);
                        return false;
                    }
                }
                const RowHashVerifier verifier;
                const auto t0va = std::chrono::steady_clock::now();
                const bool result = verifier.verify_all(csm_out, lh);
                const auto t1va = std::chrono::steady_clock::now();
                {
                    const auto ms = static_cast<std::size_t>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(t1va - t0va).count()
                    );
                    snap.time_verify_all_ms += ms;
                    snap.time_lh_ms += ms;
                }
                if (result) {
                    ::crsce::o11y::event("block_end_ok");
                } else {
                    ::crsce::o11y::event("block_end_verify_fail");
                }
                return result;
            }
            ::crsce::o11y::metric("block_terminating", 1LL);
            ::crsce::o11y::metric("gobp_start_multiphase", 1LL);
            if (!det.solved()) {
                // --- GOBP fallback (adaptive schedule) ---
                static constexpr double gobp_damp = 0.50;
                static constexpr double gobp_conf = 0.995;
                static constexpr int gobp_iters = 10000000;
                static constexpr bool multiphase = true;
                // Adaptive restarts + perturbation (plateau escape)
                static constexpr int kRestarts = 12; // additional attempts after the initial run
                static constexpr double kPerturbBase = 0.008; // base jitter magnitude
                static constexpr double kPerturbStep = 0.006; // per-restart increment
                const std::uint64_t restart_seed = read_seed_or_default(0x00C0FFEEULL);
                std::mt19937_64 rng(restart_seed);

                for (int rs = 0; rs <= kRestarts && !det.solved(); ++rs) {
                    bool restart_triggered = false;
                    if (rs > 0) {
                        // Revert to last locked-in baseline and perturb beliefs slightly to escape plateaus
                        csm_out = baseline_csm;
                        st = baseline_st;
                        const double jitter = kPerturbBase + (static_cast<double>(rs - 1) * kPerturbStep);
                        ::crsce::o11y::event(
                            "gobp_restart",
                            {
                                {"rs", std::to_string(rs)},
                                {"jitter", std::to_string(jitter)}
                            }
                        );
                        std::uniform_real_distribution<double> delta(-jitter, jitter);
                        for (std::size_t r0 = 0; r0 < S; ++r0) {
                            for (std::size_t c0 = 0; c0 < S; ++c0) {
                                if (csm_out.is_locked(r0, c0)) {
                                    continue;
                                }
                                const double v = csm_out.get_data(r0, c0);
                                const double j = delta(rng);
                                const double nv = std::clamp(v + j, 0.0, 1.0);
                                csm_out.set_data(r0, c0, nv);
                            }
                        }
                    }

                    if (multiphase) {
                    // Optional parallel multi-start probe (disabled by default)
                    int par_rs = 0;
                    if (const char *p = std::getenv("CRSCE_PAR_RS") /* NOLINT(concurrency-mt-unsafe) */; p && *p) {
                        par_rs = std::max(0, std::atoi(p));
                    }
                    if (par_rs > 1 && rs < kRestarts) {
                        const std::size_t workers = static_cast<std::size_t>(std::min(par_rs, 8));
                        const std::size_t base_valid = ::crsce::decompress::detail::compute_prefix(pc_ver_seen, pc_ok, pc_prefix_len, csm_out, st, lh, snap);
                        std::atomic<bool> adopted{false};
                        std::mutex adopt_mu;
                        Csm c_winner = csm_out;
                        ConstraintState st_winner = st;
                        // Per-worker total and per-phase timings
                        std::vector<BlockSolveSnapshot::ThreadEvent> ev_total(workers);
                        std::vector<std::array<BlockSolveSnapshot::ThreadEvent,4>> ev_phase(workers);
                        std::vector<std::thread> pool;
                        pool.reserve(workers);
                        std::atomic<std::size_t> next_idx{0};
                        const std::size_t tasks = std::min<std::size_t>(workers, static_cast<std::size_t>(kRestarts - rs));
                        // Time budget per worker (ms)
                        std::size_t max_ms = 10000;
                        if (const char *p = std::getenv("CRSCE_PAR_RS_MAX_MS") /* NOLINT(concurrency-mt-unsafe) */; p && *p) {
                            const std::int64_t v = std::strtoll(p, nullptr, 10);
                            if (v > 0) {
                                max_ms = static_cast<std::size_t>(v);
                            }
                        }
                        for (std::size_t wi = 0; wi < workers; ++wi) {
                            pool.emplace_back(
                                run_restart_worker,
                                wi,
                                std::cref(baseline_csm),
                                std::cref(baseline_st),
                                std::ref(c_winner),
                                std::ref(st_winner),
                                std::ref(adopted),
                                std::ref(adopt_mu),
                                std::ref(next_idx),
                                tasks,
                                lh,
                                restart_seed,
                                S,
                                base_valid,
                                max_ms,
                                std::ref(ev_phase.at(wi)),
                                std::ref(ev_total.at(wi)),
                                kPerturbBase,
                                kPerturbStep
                            );
                        }
                        for (auto &t : pool) { if (t.joinable()) { t.join(); } }
                        for (const auto &et : ev_total) { snap.thread_events.push_back(et); }
                        for (const auto &arr : ev_phase) { for (const auto &ep : arr) { if (!ep.name.empty()) { snap.thread_events.push_back(ep); } } }
                        if (adopted.load(std::memory_order_acquire)) {
                            csm_out = c_winner; st = st_winner; baseline_csm = csm_out; baseline_st = st;
                            BlockSolveSnapshot::RestartEvent ev{}; ev.restart_index = rs + 1; ev.prefix_rows = base_valid + 1; ev.unknown_total = snap.unknown_total; ev.action = BlockSolveSnapshot::RestartAction::lockInParRs;
                            snap.restarts.push_back(ev);
                            continue; // proceed after adopting winner
                        }
                    }
                    // Four-phase schedule (fixed): 0: bootstrap; 1: smoothing; 2: REHEAT; 3: POLISH
                    // Tuned in code; no environment overrides are applied.
                    std::array<double, 4> dampers{{0.50, 0.10, 0.35, 0.02}};
                    // Original confidence schedule
                    std::array<double, 4> confs{{0.995, 0.70, 0.85, 0.55}};
                    std::array<int, 4> iters_arr{{8000, 12000, 300000, 2000000}};
                    // Polish-stage micro-shake attempts on plateau before giving up
                    static constexpr int kPolishShakes = 6;
                    static constexpr double kPolishShakeJitter = 0.008;

                    // Per-row pre-anneal commit and finisher
                    (void)::crsce::decompress::detail::commit_valid_prefix(csm_out, st, lh, baseline_csm, baseline_st, snap, rs);
                    while (::crsce::decompress::detail::commit_any_verified_rows(csm_out, st, lh, baseline_csm, baseline_st, snap, rs)) {
                        for (int it = 0; it < 25 && !det.solved(); ++it) {
                            if (det.solve_step() == 0) {
                                break;
                            }
                        }
                    }
                    (void)::crsce::decompress::detail::finish_near_complete_any_row(csm_out, st, lh, baseline_csm, baseline_st, snap, rs);
                        int since_restart = 0;
                        const int dynamic_cooldown_base = 150; // reduced cooldown for exploration
                        std::vector<std::size_t> recent_restart_rows;
                        for (std::size_t ph = 0; ph < dampers.size() && !det.solved(); ++ph) {
                            // Announce phase start with structured tags
                            {
                                std::size_t sumU0 = 0; for (const auto u : st.U_row) { sumU0 += static_cast<std::size_t>(u); }
                                ::crsce::o11y::metric(
                                    "gobp_phase_start",
                                    1LL,
                                    {{"phase", std::to_string(ph)},
                                     {"conf", std::to_string(confs.at(ph))},
                                     {"damp", std::to_string(dampers.at(ph))},
                                     {"iters", std::to_string(iters_arr.at(ph))},
                                     {"sumU", std::to_string(sumU0)}});
                            }
                            // Adaptive anneal tweaks based on stall
                            if (since_restart > 400 && ph == 1) {
                                confs.at(ph) = std::max(0.60, confs.at(ph) - 0.10);
                                dampers.at(ph) = std::max(0.05, dampers.at(ph) - 0.05);
                                ::crsce::o11y::event(
                                    "gobp_phase_adaptive_tweak",
                                    {{"phase", std::to_string(ph)},
                                     {"conf", std::to_string(confs.at(ph))},
                                     {"damp", std::to_string(dampers.at(ph))},
                                     {"since_restart", std::to_string(since_restart)}});
                            }
                            if (since_restart > 800 && ph == 2) {
                                confs.at(ph) = std::max(0.70, confs.at(ph) - 0.05);
                                dampers.at(ph) = std::max(0.10, dampers.at(ph) - 0.05);
                                ::crsce::o11y::event(
                                    "gobp_phase_adaptive_tweak",
                                    {{"phase", std::to_string(ph)},
                                     {"conf", std::to_string(confs.at(ph))},
                                     {"damp", std::to_string(dampers.at(ph))},
                                     {"since_restart", std::to_string(since_restart)}});
                            }
                            GobpSolver gobp(csm_out, st, dampers.at(ph), confs.at(ph));
                            int polish_shakes_remaining = (ph == (dampers.size() - 1)) ? kPolishShakes : 0;
                            // Heartbeat-based stall tracking using best sumU in phase
                            std::size_t best_sumU_in_phase = 0; {
                                for (const auto u : st.U_row) {
                                    best_sumU_in_phase += static_cast<std::size_t>(u);
                                }
                            }
                            int hb_no_improve = 0;
                            bool scan_flipped = false;
                            int plateau_flip_attempts = 0;
                            const int heartbeat = ((ph + 1U) == dampers.size()) ? 500 : 2000; // more frequent in POLISH
                            // Verification cadence: configurable tick; adaptively increase while stalled
                            int verify_tick_base = 1500; // default cadence
                            if (const char *e = std::getenv("CRSCE_VERIFY_TICK") /* NOLINT(concurrency-mt-unsafe) */) {
                                verify_tick_base = std::max(1, std::atoi(e));
                            }
                            int verify_tick_cur = verify_tick_base;
                            // ROI early-stop controls (per phase)
                            double roi_min_per_1k = 8.0; // cells improved per 1000 iterations
                            if (const char *e = std::getenv("CRSCE_ROI_MIN_PER_1K") /* NOLINT(concurrency-mt-unsafe) */) {
                                const double v = std::atof(e);
                                roi_min_per_1k = (v > 0.0 ? v : 0.0);
                            }
                            int roi_min_iters = 1000;
                            if (const char *e = std::getenv("CRSCE_ROI_MIN_ITERS") /* NOLINT(concurrency-mt-unsafe) */) {
                                roi_min_iters = std::max(0, std::atoi(e));
                            }
                            int roi_low_streak = 0;
                            int roi_last_gi = 0;
                            std::size_t roi_last_sumU = best_sumU_in_phase;
                            const int hb_terminate = ((ph + 1U) == dampers.size()) ? 10 : 5;   // require more patience in POLISH
                            const double amb_eps = 0.02; // ambiguity window around 0.5 for micro-shake targeting
                            for (int gi = 0; gi < iters_arr.at(ph); ++gi) {
                                // Removed legacy flush; metrics are buffered asynchronously
                                std::size_t gprog = 0;
                                try {
                                snap.phase = BlockSolveSnapshot::Phase::gobp;
                                {
                                    const auto t0g = std::chrono::steady_clock::now();
                                    const std::size_t gobp_only = gobp.solve_step();
                                    const auto t1g = std::chrono::steady_clock::now();
                                    snap.time_gobp_ms += static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1g - t0g).count());
                                    gprog += gobp_only;
                                    snap.gobp_cells_solved_total += gobp_only;
                                }
                                // Follow with a DE sweep to propagate any newly forced moves
                                {
                                    const auto t0d = std::chrono::steady_clock::now();
                                    const std::size_t vv = det.solve_step();
                                    const auto t1d = std::chrono::steady_clock::now();
                                    gprog += vv;
                                    const auto ms = static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1d - t0d).count());
                                    snap.time_de_ms += ms;
                                    snap.time_de_in_gobp_ms += ms;
                                }
                                ++snap.gobp_iters_run;
                            } catch (const std::exception &e) {
                                snap.iter = static_cast<std::size_t>(gi);
                                snap.message = e.what();
                                snap.U_row.assign(st.U_row.begin(), st.U_row.end());
                                snap.U_col.assign(st.U_col.begin(), st.U_col.end());
                                snap.U_diag.assign(st.U_diag.begin(), st.U_diag.end());
                                snap.U_xdiag.assign(st.U_xdiag.begin(), st.U_xdiag.end());
                                {
                                    std::size_t sumU = 0;
                                    for (const auto u: st.U_row) {
                                        sumU += static_cast<std::size_t>(u);
                                    }
                                    snap.unknown_total = sumU;
                                    snap.solved = (static_cast<std::size_t>(S) * static_cast<std::size_t>(S)) - sumU;
                                }
                                set_block_solve_snapshot(snap);
                                return false;
                            }
                            // Update snapshot after this iteration
                            snap.iter = static_cast<std::size_t>(gi);
                            snap.U_row.assign(st.U_row.begin(), st.U_row.end());
                            snap.U_col.assign(st.U_col.begin(), st.U_col.end());
                            snap.U_diag.assign(st.U_diag.begin(), st.U_diag.end());
                            snap.U_xdiag.assign(st.U_xdiag.begin(), st.U_xdiag.end());
                            {
                                std::size_t sumU = 0;
                                for (const auto u: st.U_row) {
                                    sumU += static_cast<std::size_t>(u);
                                }
                                snap.unknown_total = sumU;
                                snap.solved = (static_cast<std::size_t>(S) * static_cast<std::size_t>(S)) - sumU;
                                snap.unknown_history.push_back(sumU);
                                if (trace_sumu_enabled()) {
                                    ::crsce::o11y::metric(
                                        "gobp_iter",
                                        static_cast<std::int64_t>(gi),
                                        {{"phase", std::to_string(ph)}, {"sumU", std::to_string(sumU)}, {"gprog", std::to_string(gprog)}});
                                }
                                if ((gi % heartbeat) == 0) {
                                    ::crsce::o11y::metric(
                                        "gobp_heartbeat",
                                        1LL,
                                        {{"phase", std::to_string(ph)},
                                         {"iter", std::to_string(gi)},
                                         {"sumU", std::to_string(sumU)},
                                         {"gprog", std::to_string(gprog)}});
                                    // ROI measurement and early-stop
                                    const int gi_span = (gi - roi_last_gi) + 1;
                                    if (gi_span >= roi_min_iters) {
                                        const double imp = static_cast<double>(roi_last_sumU) - static_cast<double>(sumU);
                                        const double per1k = (gi_span > 0) ? (imp * 1000.0 / static_cast<double>(gi_span)) : imp * 1000.0;
                                        if (per1k < roi_min_per_1k) {
                                            ++roi_low_streak;
                                        } else {
                                            roi_low_streak = 0;
                                        }
                                        roi_last_gi = gi;
                                        roi_last_sumU = sumU;
                                        if (roi_low_streak >= 2) {
                                            ::crsce::o11y::event(
                                                "gobp_phase_terminated",
                                                {{"phase", std::to_string(ph)},
                                                 {"reason", std::string("low_roi")},
                                                 {"per1k", std::to_string(per1k)}});
                                            break;
                                        }
                                    }
                                    if (sumU < best_sumU_in_phase) {
                                        best_sumU_in_phase = sumU;
                                        hb_no_improve = 0;
                                        verify_tick_cur = verify_tick_base; // reset cadence when improving
                                    } else {
                                        ++hb_no_improve;
                                        // Less frequent verifications while stalled to reduce hashing overhead
                                        verify_tick_cur = std::min(verify_tick_cur + verify_tick_base, verify_tick_base * 8);
                                        // Small conf decay in POLISH when stalled; allow deeper decay with more heartbeats
                                        if ((ph + 1U) == dampers.size()) {
                                            if (hb_no_improve > 20) {
                                                confs.at(ph) = std::max(0.50, confs.at(ph) - 0.01);
                                            } else if (hb_no_improve > 10) {
                                                confs.at(ph) = std::max(0.52, confs.at(ph) - 0.01);
                                            } else if (hb_no_improve > 5) {
                                                confs.at(ph) = std::max(0.55, confs.at(ph) - 0.01);
                                            }
                                        }
                                        if (hb_no_improve >= hb_terminate) {
                                            ::crsce::o11y::event(
                                                "gobp_phase_terminated",
                                                {{"phase", std::to_string(ph)},
                                                 {"reason", std::string("stall")},
                                                 {"heartbeats", std::to_string(hb_no_improve)}});
                                            break;
                                        }
                                    }
                                }
                            }
                            // keep counters and seeds updated
                            snap.rng_seed_restarts = restart_seed;
                            snap.restarts_total = snap.restarts.size();
                            set_block_solve_snapshot(snap);
                            // Plateau flip: try alternate scan perspective up to three times before other measures
                            if (gprog == 0) {
                                if (plateau_flip_attempts < 3) {
                                    scan_flipped = !scan_flipped;
                                    gobp.set_scan_flipped(scan_flipped);
                                    ++plateau_flip_attempts;
                                    ::crsce::o11y::event(
                                        "gobp_plateau_flip",
                                        {{"attempt", std::to_string(plateau_flip_attempts)},
                                         {"mode", std::string(scan_flipped ? "flipped" : "row-major")}});
                                    continue; // retry immediately with flipped (or restored) perspective
                                }
                            } else {
                                plateau_flip_attempts = 0; // reset when progress is made
                            }
                            // Per-row/col gating and finishers with sink-avoidance
                            {
                                ++snap.gating_calls;
                                ::crsce::o11y::counter("gating_calls");
                                bool any_commit = false;
                                const bool do_verify = ((gi % std::max(1, verify_tick_cur)) == 0);
                                if (do_verify) {
                                    const auto t0v1 = std::chrono::steady_clock::now();
                                    const bool any1 = ::crsce::decompress::detail::commit_valid_prefix(csm_out, st, lh, baseline_csm, baseline_st, snap, rs);
                                    const auto t1v1 = std::chrono::steady_clock::now();
                                    snap.time_lh_ms += static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1v1 - t0v1).count());
                                    if (any1) {
                                        any_commit = true;
                                    }
                                    const auto t0v2 = std::chrono::steady_clock::now();
                                    const bool any2 = ::crsce::decompress::detail::commit_any_verified_rows(csm_out, st, lh, baseline_csm, baseline_st, snap, rs);
                                    const auto t1v2 = std::chrono::steady_clock::now();
                                    snap.time_lh_ms += static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1v2 - t0v2).count());
                                    if (any2) {
                                        any_commit = true;
                                    }
                                }
                                (void)::crsce::decompress::detail::finish_boundary_row_adaptive(csm_out, st, lh, baseline_csm, baseline_st, snap, rs, since_restart);
                                // Gate micro-solvers to polish phase by default (env CRSCE_MS_ONLY_POLISH=1)
                                bool only_polish = true;
                                if (const char *e = std::getenv("CRSCE_MS_ONLY_POLISH") /* NOLINT(concurrency-mt-unsafe) */) {
                                    only_polish = (*e!='0');
                                }
                                const bool on_polish = ((ph + 1U) == dampers.size());
                                if (!only_polish || on_polish) {
                                    {
                                        (void)::crsce::decompress::detail::finish_boundary_row_micro_solver(csm_out, st, lh, baseline_csm, baseline_st, snap, rs);
                                        (void)::crsce::decompress::detail::finish_boundary_row_segment_solver(csm_out, st, lh, baseline_csm, baseline_st, snap, rs);
                                        (void)::crsce::decompress::detail::finish_two_row_micro_solver(csm_out, st, lh, baseline_csm, baseline_st, snap, rs);
                                    }
                                }
                                // Build sink-averse scored candidates for rows/cols
                                std::vector<std::pair<double,std::size_t>> row_scores; row_scores.reserve(S);
                                std::vector<std::pair<double,std::size_t>> col_scores; col_scores.reserve(S);
                                const double w1 = 0.6;
                                const double w2 = 0.4;
                                for (std::size_t r2 = 0; r2 < S; ++r2) {
                                    const auto u = static_cast<double>(st.U_row.at(r2));
                                    if (u <= 0.0) { continue; }
                                    const double s_u = 1.0 - (u / static_cast<double>(S));
                                    const double ru = std::max(1.0, u);
                                    const double rneed = std::abs((static_cast<double>(st.R_row.at(r2)) / ru) - 0.5);
                                    const double s = (w1 * s_u) + (w2 * rneed);
                                    row_scores.emplace_back(s, r2);
                                }
                                for (std::size_t c2 = 0; c2 < S; ++c2) {
                                    const auto u = static_cast<double>(st.U_col.at(c2));
                                    if (u <= 0.0) { continue; }
                                    const double s_u = 1.0 - (u / static_cast<double>(S));
                                    const double ru = std::max(1.0, u);
                                    const double rneed = std::fabs((static_cast<double>(st.R_col.at(c2)) / ru) - 0.5);
                                    const double s = (w1 * s_u) + (w2 * rneed);
                                    col_scores.emplace_back(s, c2);
                                }
                                // Sort by score descending without lambdas (ODPCPP-friendly)
                                std::ranges::sort(row_scores, std::greater<double>{}, &std::pair<double,std::size_t>::first);
                                std::ranges::sort(col_scores, std::greater<double>{}, &std::pair<double,std::size_t>::first);
                                std::vector<std::size_t> best_rows; best_rows.reserve(24);
                                std::vector<std::size_t> best_cols; best_cols.reserve(16);
                                for (std::size_t i = 0; i < row_scores.size() && i < 24; ++i) {
                                    best_rows.push_back(row_scores[i].second);
                                }
                                for (std::size_t i = 0; i < col_scores.size() && i < 16; ++i) {
                                    best_cols.push_back(col_scores[i].second);
                                }
                                // Try scored rows/cols first (avoid sinks); fall back to top_* helpers
                                const bool r_try = ::crsce::decompress::detail::finish_near_complete_rows_scored(
                                    csm_out,
                                    st,
                                    lh,
                                    baseline_csm,
                                    baseline_st,
                                    snap,
                                    rs,
                                    best_rows,
                                    256
                                );
                                const bool c_try = ::crsce::decompress::detail::finish_near_complete_columns_scored(
                                    csm_out,
                                    st,
                                    lh,
                                    baseline_csm,
                                    baseline_st,
                                    snap,
                                    rs,
                                    best_cols,
                                    160
                                );
                                if (!r_try) {
                                    const std::size_t rows_top_n = std::min<std::size_t>(64, 16 + static_cast<std::size_t>(since_restart / 200));
                                    (void)::crsce::decompress::detail::finish_near_complete_top_rows(csm_out, st, lh, baseline_csm, baseline_st, snap, rs, rows_top_n, 256);
                                }
                                if (!c_try) {
                                    const std::size_t cols_top_n = std::min<std::size_t>(32, 8 + static_cast<std::size_t>(since_restart / 300));
                                    (void)::crsce::decompress::detail::finish_near_complete_top_columns(csm_out, st, lh, baseline_csm, baseline_st, snap, rs, cols_top_n, 160);
                                }
                                if (any_commit) {
                                    for (int it = 0; it < 50 && !det.solved(); ++it) { if (det.solve_step() == 0) { break; } }
                                    since_restart = 0;
                                } else {
                                    since_restart++;
                                }
                                // Always-on micro-shake on plateau to perturb ambiguous beliefs slightly (after flip attempts)
                                if (gprog == 0 && plateau_flip_attempts >= 3) {
                                    std::uniform_real_distribution<double> delta(-0.001, 0.001);
                                    for (std::size_t r0 = 0; r0 < S; ++r0) {
                                        for (std::size_t c0 = 0; c0 < S; ++c0) {
                                            if (csm_out.is_locked(r0, c0)) { continue; }
                                            const double v = csm_out.get_data(r0, c0);
                                            if (std::fabs(v - 0.5) < amb_eps) {
                                                const double nv = std::clamp(v + delta(rng), 0.0, 1.0);
                                                csm_out.set_data(r0, c0, nv);
                                            }
                                        }
                                    }
                                    ::crsce::o11y::metric("gobp_micro_shake", 1LL);
                                    // Try quick finishers after shake
                                    bool any = false;
                                    // Gate row verification by verify cadence as well
                                    const bool do_verify2 = ((gi % std::max(1, verify_tick_cur)) == 0);
                                    if (do_verify2) {
                                        const auto t0v3 = std::chrono::steady_clock::now();
                                        any = ::crsce::decompress::detail::commit_any_verified_rows(
                                            csm_out,
                                            st,
                                            lh,
                                            baseline_csm,
                                            baseline_st,
                                            snap,
                                            rs
                                        ) || any;
                                        const auto t1v3 = std::chrono::steady_clock::now();
                                        snap.time_lh_ms += static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1v3 - t0v3).count());
                                    }

                                    (void)::crsce::decompress::detail::finish_near_complete_top_rows(
                                        csm_out,
                                        st,
                                        lh,
                                        baseline_csm,
                                        baseline_st,
                                        snap,
                                        rs,
                                        32,
                                        256
                                    );
                                    (void)::crsce::decompress::detail::finish_near_complete_top_columns(
                                        csm_out,
                                        st,
                                        lh,
                                        baseline_csm,
                                        baseline_st,
                                        snap,
                                        rs,
                                        16,
                                        160
                                    );
                                    (void)::crsce::decompress::detail::finish_boundary_row_adaptive(
                                        csm_out,
                                        st,
                                        lh,
                                        baseline_csm,
                                        baseline_st,
                                        snap,
                                        rs,
                                        since_restart
                                    );
                                    bool only_polish2 = true;
                                    if (const char *e = std::getenv("CRSCE_MS_ONLY_POLISH") /* NOLINT(concurrency-mt-unsafe) */) {
                                        only_polish2 = (*e!='0');
                                    }
                                    const bool on_polish2 = ((ph + 1U) == dampers.size());
                                    if (!only_polish2 || on_polish2) {
                                        (void)::crsce::decompress::detail::finish_boundary_row_micro_solver(
                                            csm_out,
                                            st,
                                            lh,
                                            baseline_csm,
                                            baseline_st,
                                            snap,
                                            rs
                                        );
                                        (void)::crsce::decompress::detail::finish_boundary_row_segment_solver(
                                            csm_out,
                                            st,
                                            lh,
                                            baseline_csm,
                                            baseline_st,
                                            snap,
                                            rs
                                        );
                                        (void)::crsce::decompress::detail::finish_two_row_micro_solver(
                                            csm_out,
                                            st,
                                            lh,
                                            baseline_csm,
                                            baseline_st,
                                            snap,
                                            rs
                                        );
                                    }
                                    bool only_polish3 = true;
                                    if (const char *e = std::getenv("CRSCE_MS_ONLY_POLISH") /* NOLINT(concurrency-mt-unsafe) */) {
                                        only_polish3 = (*e!='0');
                                    }
                                    const bool on_polish3 = ((ph + 1U) == dampers.size());
                                    if (!only_polish3 || on_polish3) {
                                        (void)::crsce::decompress::detail::finish_boundary_row_micro_solver(
                                            csm_out, st, lh, baseline_csm, baseline_st, snap, rs
                                        );
                                        (void)::crsce::decompress::detail::finish_boundary_row_segment_solver(
                                            csm_out, st, lh, baseline_csm, baseline_st, snap, rs
                                        );
                                        (void)::crsce::decompress::detail::finish_two_row_micro_solver(
                                            csm_out, st, lh, baseline_csm, baseline_st, snap, rs
                                        );
                                    }
                                    if (any) {
                                        for (int it2 = 0; it2 < 50 && !det.solved(); ++it2) {
                                            if (det.solve_step() == 0) {
                                                break;
                                            }
                                        }
                                    }
                                }
                                int repeats = 0;
                                if (!snap.restarts.empty()) {
                                    const auto last_r = snap.restarts.back().prefix_rows;
                                    int cnt = 0; for (const auto r0 : recent_restart_rows) {
                                        if (r0 == last_r) {
                                            ++cnt;
                                        }
                                    }
                                    repeats = cnt;
                                }
                                const int cooldown_ticks = std::clamp(
                                    dynamic_cooldown_base + (since_restart / 2) - (repeats * 150),
                                    100,
                                    2000
                                );
                                if (
                                    ::crsce::decompress::detail::audit_and_restart_on_contradiction(
                                        csm_out,
                                        st,
                                        lh,
                                        baseline_csm,
                                        baseline_st,
                                        snap,
                                        rs,
                                        valid_bits,
                                        cooldown_ticks,
                                        since_restart
                                    )
                                ) {
                                    if (!snap.restarts.empty()) {
                                        const auto r = snap.restarts.back().prefix_rows;
                                        recent_restart_rows.push_back(r);
                                        if (recent_restart_rows.size() > 8U) {
                                            recent_restart_rows.erase(recent_restart_rows.begin());
                                        }
                                    }
                                    ::crsce::o11y::event(
                                        "gobp_restart_triggered",
                                        {{"rs", std::to_string(rs)},
                                         {"cooldown", std::to_string(cooldown_ticks)},
                                         {"since_restart", std::to_string(since_restart)}});
                                    restart_triggered = true;
                                    break;
                                }
                            }
                            // prefix gating removed
                            // Focused row completion (boundary row only): deep localized DE/backtrack
                            // to tip ≥95% rows to 100% and grow the verified prefix.
                            {
                                // Determine boundary row (first row with unknowns)
                                std::size_t boundary = 0;
                                for (; boundary < S; ++boundary) {
                                    if (st.U_row.at(boundary) > 0) {
                                        break;
                                    }
                                }
                                if (boundary < S) {
                                    const std::size_t valid_now = ::crsce::decompress::detail::compute_prefix(pc_ver_seen, pc_ok, pc_prefix_len, csm_out, st, lh, snap);
                                    auto near_thresh = static_cast<std::uint16_t>((S + 4U) / 5U); // ~20%
                                    if (const char *e = std::getenv("CRSCE_FOCUS_NEAR_THRESH") /* NOLINT(concurrency-mt-unsafe) */; e && *e) {
                                        int pct = std::atoi(e);
                                        pct = std::max(pct, 5);
                                        pct = std::min(pct, 70);
                                        near_thresh = static_cast<std::uint16_t>((static_cast<unsigned>(S) * static_cast<unsigned>(pct) + 99U) / 100U);
                                    }
                                    if (st.U_row.at(boundary) > 0 && st.U_row.at(boundary) <= near_thresh) {
                                        ++snap.focus_boundary_attempts;
                                        ::crsce::o11y::counter("focus_boundary_attempts");
                                        int steps = 0;
                                        static constexpr int kFocusMaxSteps = 48;
                                        // Allow mode-based tuning via environment (A/B experiments)
                                        int kFocusBtIters = 8000;
                                        if (const char *m = std::getenv("CRSCE_BT_MODE") /* NOLINT(concurrency-mt-unsafe) */; m && (*m == 'A' || *m == 'a')) {
                                            kFocusBtIters = 10000;
                                        }
                                        while (st.U_row.at(boundary) > 0 && steps < kFocusMaxSteps) {
                                            ++steps;
                                            // Pick highest-pressure unlocked cell in boundary row
                                            bool found = false; std::size_t c_pick = 0;
                                            double best_score = -1.0;
                                            for (std::size_t c0 = 0; c0 < S; ++c0) {
                                                if (csm_out.is_locked(boundary, c0)) {
                                                    continue;
                                                }
                                                const std::size_t d0 = ::crsce::decompress::detail::calc_d(boundary, c0);
                                                const std::size_t x0 = ::crsce::decompress::detail::calc_x(boundary, c0);
                                                const double u_c = std::max(1.0, static_cast<double>(st.U_col.at(c0)));
                                                const double u_d = std::max(1.0, static_cast<double>(st.U_diag.at(d0)));
                                                const double u_x = std::max(1.0, static_cast<double>(st.U_xdiag.at(x0)));
                                                const double rneed_c = std::fabs((static_cast<double>(st.R_col.at(c0)) / u_c) - 0.5);
                                                const double rneed_d = std::fabs((static_cast<double>(st.R_diag.at(d0)) / u_d) - 0.5);
                                                const double rneed_x = std::fabs((static_cast<double>(st.R_xdiag.at(x0)) / u_x) - 0.5);
                                                const double score = (0.6 * rneed_c) + (0.2 * rneed_d) + (0.2 * rneed_x);
                                                if (score > best_score) {
                                                    best_score = score;
                                                    c_pick = c0;
                                                    found = true;
                                                }
                                            }
                                            if (!found) {
                                                break;
                                            }
                                            bool adopted_step = false;
                                            const std::uint16_t before = st.U_row.at(boundary);
                                            for (int vv = 0; vv < 2 && !adopted_step; ++vv) {
                                                const bool assume_one = (vv == 1);
                                                Csm c_try = csm_out;
                                                ConstraintState st_try = st;
                                                c_try.put(boundary, c_pick, assume_one);
                                                c_try.lock(boundary, c_pick);
                                                if (st_try.U_row.at(boundary) > 0) {
                                                    --st_try.U_row.at(boundary);
                                                }
                                                if (st_try.U_col.at(c_pick) > 0) {
                                                    --st_try.U_col.at(c_pick);
                                                }

                                                const std::size_t d0 = ::crsce::decompress::detail::calc_d(boundary, c_pick);
                                                const std::size_t x0 = ::crsce::decompress::detail::calc_x(boundary, c_pick);

                                                if (st_try.U_diag.at(d0) > 0) {
                                                    --st_try.U_diag.at(d0);
                                                }
                                                if (st_try.U_xdiag.at(x0) > 0) {
                                                    --st_try.U_xdiag.at(x0);
                                                }
                                                if (assume_one) {
                                                    if (st_try.R_row.at(boundary) > 0) {
                                                        --st_try.R_row.at(boundary);
                                                    }
                                                    if (st_try.R_col.at(c_pick) > 0) {
                                                        --st_try.R_col.at(c_pick);
                                                    }
                                                    if (st_try.R_diag.at(d0) > 0) {
                                                        --st_try.R_diag.at(d0);
                                                    }
                                                    if (st_try.R_xdiag.at(x0) > 0) {
                                                        --st_try.R_xdiag.at(x0);
                                                    }
                                                }
                                                DeterministicElimination det_bt{c_try, st_try};
                                                for (int it0 = 0; it0 < kFocusBtIters; ++it0) {
                                                    const std::size_t prog0 = det_bt.solve_step();
                                                    if (st_try.U_row.at(boundary) == 0) { break; }
                                                    if (prog0 == 0) { break; }
                                                }
                                                if (st_try.U_row.at(boundary) == 0 || st_try.U_row.at(boundary) < before) {
                                                    // If boundary completed in the try-state, verify prefix immediately on c_try
                                                    if (st_try.U_row.at(boundary) == 0) {
                                                        const RowHashVerifier verifier_try;
                                                        // Only extend the valid prefix by one row for the try-state
                                                        const std::size_t check_rows = std::min<std::size_t>(valid_now + 1, S);

                                                        if (check_rows > 0) {
                                                            const auto t0vv = std::chrono::steady_clock::now();
                                                            const bool okvv = verifier_try.verify_rows(c_try, lh, check_rows);
                                                            const auto t1vv = std::chrono::steady_clock::now();
                                                            snap.time_lh_ms += static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1vv - t0vv).count());
                                                            if (okvv) {
                                                                // Commit the try-state and baseline; this grows the verified prefix
                                                                csm_out = c_try;
                                                                st = st_try;
                                                                adopted_step = true;
                                                                ++snap.focus_boundary_prefix_locks;
                                                                std::size_t sumU2 = 0;
                                                                for (const auto u : st.U_row) {
                                                                    sumU2 += static_cast<std::size_t>(u);
                                                                }
                                                                ::crsce::o11y::metric(
                                                                    "prefix_locked_in",
                                                                    static_cast<std::int64_t>(check_rows),
                                                                    {{"unknown", std::to_string(sumU2)}, {"rs", std::to_string(rs)}});
                                                                baseline_csm = csm_out;
                                                                baseline_st = st;
                                                                BlockSolveSnapshot::RestartEvent ev{};
                                                                ev.restart_index = rs;
                                                                ev.prefix_rows = check_rows;
                                                                ev.unknown_total = sumU2;
                                                                ev.action = BlockSolveSnapshot::RestartAction::lockIn;
                                                                snap.restarts.push_back(ev);
                                                                set_block_solve_snapshot(snap);
                                                            } else {
                                                                // Wrong assumption for this cell; try the other value
                                                            }
                                                } else {
                                                    // Partial improvement (U reduced). Adopt to continue local search.
                                                    csm_out = c_try;
                                                    st = st_try;
                                                    adopted_step = true;
                                                    ++snap.focus_boundary_partials;
                                                }
                                            }
                                        }
                                        if (!adopted_step) {
                                            break;
                                        }
                                    }
                                    if (steps > 0) {
                                        // Quick DE settle after local improvements
                                        for (int it1 = 0; it1 < 50 && !det.solved(); ++it1) {
                                            if (det.solve_step() == 0) {
                                                break;
                                            }
                                        }
                                        // If boundary completed, immediately run prefix gating and update baseline/restart
                                        if (st.U_row.at(boundary) == 0) {
                                            std::size_t prefix_rows2 = 0;
                                            for (std::size_t r1 = 0; r1 < S; ++r1) {
                                                if (st.U_row.at(r1) == 0) {
                                                    ++prefix_rows2;
                                                } else {
                                                    break;
                                                }
                                            }
                                            const RowHashVerifier verifier;
                                            bool bad_prefix = false;
                                            if (prefix_rows2 > 0) {
                                                const auto t0vv2 = std::chrono::steady_clock::now();
                                                const bool okvv2 = verifier.verify_rows(csm_out, lh, prefix_rows2);
                                                const auto t1vv2 = std::chrono::steady_clock::now();
                                                snap.time_lh_ms += static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1vv2 - t0vv2).count());
                                                bad_prefix = !okvv2;
                                            }
                                            if (bad_prefix) {
                                                std::size_t sumU2 = 0; for (const auto u : st.U_row) {
                                                    sumU2 += static_cast<std::size_t>(u);
                                                }
                                                ::crsce::o11y::metric(
                                                    "prefix_restart",
                                                    static_cast<std::int64_t>(prefix_rows2),
                                                    {{"unknown", std::to_string(sumU2)}, {"rs", std::to_string(rs)}});
                                                csm_out = baseline_csm;
                                                st = baseline_st;
                                                BlockSolveSnapshot::RestartEvent ev{};
                                                ev.restart_index = rs;
                                                ev.prefix_rows = prefix_rows2;
                                                ev.unknown_total = sumU2;
                                                ev.action = BlockSolveSnapshot::RestartAction::restart;
                                                snap.restarts.push_back(ev);
                                                set_block_solve_snapshot(snap);
                                                restart_triggered = true;
                                            } else if (prefix_rows2 > 0) {
                                                std::size_t sumU2 = 0;
                                                for (const auto u : st.U_row) {
                                                    sumU2 += static_cast<std::size_t>(u);
                                                }
                                                ::crsce::o11y::metric(
                                                    "prefix_locked_in",
                                                    static_cast<std::int64_t>(prefix_rows2),
                                                    {{"unknown", std::to_string(sumU2)}, {"rs", std::to_string(rs)}});
                                                baseline_csm = csm_out;
                                                baseline_st = st;
                                                BlockSolveSnapshot::RestartEvent ev{};
                                                ev.restart_index = rs;
                                                ev.prefix_rows = prefix_rows2;
                                                ev.unknown_total = sumU2;
                                                ev.action = BlockSolveSnapshot::RestartAction::lockIn;
                                                snap.restarts.push_back(ev);
                                                set_block_solve_snapshot(snap);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        if (det.solved()) {
                            ::crsce::o11y::metric("gobp_converged", 1LL);
                            break;
                        }
                            if (gprog == 0 && plateau_flip_attempts >= 3) {
                                // On last phase (polish), try a few micro-shake attempts before giving up
                                if (polish_shakes_remaining > 0 && ph == (dampers.size() - 1)) {
                                    // Lower amplitude, ambiguity-focused polish shake
                                    const double step = 0.003;
                                    const double jitter = std::min(0.012, step * static_cast<double>(kPolishShakes - polish_shakes_remaining + 1));
                                    std::uniform_real_distribution<double> delta(-jitter, jitter);
                                    for (std::size_t r0 = 0; r0 < S; ++r0) {
                                        for (std::size_t c0 = 0; c0 < S; ++c0) {
                                            if (csm_out.is_locked(r0, c0)) { continue; }
                                            const double v = csm_out.get_data(r0, c0);
                                            if (std::fabs(v - 0.5) < amb_eps) {
                                                const double j = delta(rng);
                                                const double nv = std::clamp(v + j, 0.0, 1.0);
                                                csm_out.set_data(r0, c0, nv);
                                            }
                                        }
                                    }
                                    // Log event and continue without breaking the phase loop
                                    std::size_t prefix_rows = 0;
                                    for (std::size_t r0 = 0; r0 < S; ++r0) {
                                        if (st.U_row.at(r0) == 0) {
                                            ++prefix_rows;
                                        } else {
                                            break;
                                        }
                                    }
                                    std::size_t sumU = 0;
                                    for (const auto u : st.U_row) {
                                        sumU += static_cast<std::size_t>(u);
                                    }
                                    ::crsce::o11y::event(
                                        "polish_shake",
                                        {
                                            {"jitter", std::to_string(jitter)},
                                            {"shakes_left", std::to_string(polish_shakes_remaining - 1)},
                                            {"prefix_rows", std::to_string(prefix_rows)},
                                            {"unknown", std::to_string(sumU)},
                                            {"rs", std::to_string(rs)}
                                        }
                                    );
                                    {
                                        BlockSolveSnapshot::RestartEvent ev{};
                                        ev.restart_index = rs;
                                        ev.prefix_rows = prefix_rows;
                                        ev.unknown_total = sumU;
                                        ev.action = BlockSolveSnapshot::RestartAction::polishShake;
                                        snap.restarts.push_back(ev);
                                    }
                                    set_block_solve_snapshot(snap);
                                    // Try finishers after polish shake as well
                                    bool any2 = false;
                                    any2 = ::crsce::decompress::detail::commit_any_verified_rows(
                                        csm_out, st, lh, baseline_csm, baseline_st, snap, rs
                                    ) || any2;
                                    (void)::crsce::decompress::detail::finish_near_complete_top_rows(
                                        csm_out, st, lh, baseline_csm, baseline_st, snap, rs, 64, 256
                                    );
                                    (void)::crsce::decompress::detail::finish_near_complete_top_columns(
                                        csm_out, st, lh, baseline_csm, baseline_st, snap, rs, 32, 160
                                    );
                                    (void)::crsce::decompress::detail::finish_boundary_row_adaptive(
                                        csm_out, st, lh, baseline_csm, baseline_st, snap, rs, since_restart
                                    );
                                    if (any2) {
                                        for (int it2 = 0; it2 < 100 && !det.solved(); ++it2) {
                                            if (det.solve_step() == 0) {
                                                break;
                                            }
                                        }
                                    }
                                    --polish_shakes_remaining;
                                    continue; // try again within the polish phase
                                }
                                ::crsce::o11y::event("gobp_no_progress");
                                ::crsce::o11y::metric("gobp_no_progress", 1LL);
                                break;
                            }
                        }
                        {
                            std::size_t sumU_end = 0;
                            for (const auto u : st.U_row) {
                                sumU_end += static_cast<std::size_t>(u);
                            }
                            ::crsce::o11y::event(
                                "gobp_phase_end",
                                {
                                    {"phase", std::to_string(ph)},
                                    {"sumU", std::to_string(sumU_end)}
                                }
                            );
                        }
                        if (restart_triggered) {
                            break;
                        }
                    }
                } // Close restart loop
            }

            // Final escalation: multi-candidate boundary backtrack with prefix verification
            if (!det.solved()) {
                const auto t0cp = std::chrono::steady_clock::now();
                const std::size_t valid_now = ::crsce::decompress::detail::compute_prefix(pc_ver_seen, pc_ok, pc_prefix_len, csm_out, st, lh, snap);
                const auto t1cp = std::chrono::steady_clock::now();
                snap.time_lh_ms += static_cast<std::size_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(t1cp - t0cp).count()
                );
                // Identify boundary row (first with unknowns)
                std::size_t boundary = 0;
                for (; boundary < S; ++boundary) {
                    if (st.U_row.at(boundary) > 0) {
                        break;
                    }
                }
                if (boundary < S) {
                    // Build scored candidate columns for the boundary row
                    std::vector<std::pair<double,std::size_t>> cand_cols; cand_cols.reserve(S);
                    for (std::size_t c0 = 0; c0 < S; ++c0) {
                        if (csm_out.is_locked(boundary, c0)) {
                            continue;
                        }
                        const std::size_t d0 = ::crsce::decompress::detail::calc_d(boundary, c0);
                        const std::size_t x0 = ::crsce::decompress::detail::calc_x(boundary, c0);
                        const double u_c = std::max(1.0, static_cast<double>(st.U_col.at(c0)));
                        const double u_d = std::max(1.0, static_cast<double>(st.U_diag.at(d0)));
                        const double u_x = std::max(1.0, static_cast<double>(st.U_xdiag.at(x0)));
                        const double rneed_c = std::fabs((static_cast<double>(st.R_col.at(c0)) / u_c) - 0.5);
                        const double rneed_d = std::fabs((static_cast<double>(st.R_diag.at(d0)) / u_d) - 0.5);
                        const double rneed_x = std::fabs((static_cast<double>(st.R_xdiag.at(x0)) / u_x) - 0.5);
                        const double score = (0.6 * rneed_c) + (0.2 * rneed_d) + (0.2 * rneed_x);
                        cand_cols.emplace_back(score, c0);
                    }
                    std::ranges::sort(
                        cand_cols,
                        std::greater<double>{},
                        &std::pair<double,std::size_t>::first
                    );

                    bool modeB = false;
                    if (const char *m = std::getenv("CRSCE_BT_MODE") /* NOLINT(concurrency-mt-unsafe) */) {
                        modeB = (*m=='B' || *m=='b');
                    }

                    const bool modeA = !modeB; // default A

                    // Top-K candidates for final single-cell boundary backtrack.
                    // Default 12, but allow override via env `CRSCE_BT_K1` for experimentation.
                    std::size_t K = 12;

                    if (const char *k1 = std::getenv("CRSCE_BT_K1") /* NOLINT(concurrency-mt-unsafe) */; k1 && *k1) {
                        const int kv = std::atoi(k1);
                        if (kv > 0) {
                            K = static_cast<std::size_t>(kv);
                        }
                    }
                    K = std::min<std::size_t>(K, cand_cols.size());
                    bool adopted_prefix = false;
                    if (modeA && K > 0) {
                        ++snap.final_backtrack1_attempts;
                        // Prepare tasks: for each of top-K columns, try v in {0,1}
                        std::vector<BTTaskPair> tasks; tasks.reserve(K * 2U);
                        for (std::size_t i = 0; i < K; ++i) {
                            const std::size_t c_pick = cand_cols[i].second;
                            tasks.emplace_back(c_pick, false);
                            tasks.emplace_back(c_pick, true);
                        }
                        std::atomic<bool> found{false};
                        std::mutex adopt_mu;
                        Csm c_winner = csm_out; ConstraintState st_winner = st;
                        const std::size_t workers = std::max<std::size_t>(
                            1,
                            std::min<std::size_t>(
                                tasks.size(),
                                std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 4
                            )
                        );
                        // Pre-allocate per-worker thread events to avoid concurrent push_backs
                        std::vector<BlockSolveSnapshot::ThreadEvent> worker_events(workers);
                        std::atomic<std::size_t> next_idx{0};
                        std::vector<std::thread> pool; pool.reserve(workers);
                        for (std::size_t wi = 0; wi < workers; ++wi) {
                            pool.emplace_back(
                                run_final_backtrack_worker,
                                wi,
                                std::cref(tasks),
                                std::ref(next_idx),
                                std::ref(found),
                                lh,
                                boundary,
                                valid_now,
                                S,
                                std::cref(csm_out),
                                std::cref(st),
                                std::ref(adopt_mu),
                                std::ref(c_winner),
                                std::ref(st_winner),
                                std::ref(worker_events.at(wi))
                            );
                        }
                        // Wait for workers
                        for (auto &t : pool) {
                            if (t.joinable()) {
                                t.join();
                            }
                        }
                        // Append worker events to snapshot (single-threaded)
                        for (const auto &ev : worker_events) {
                            snap.thread_events.push_back(ev);
                        }
                        if (found.load(std::memory_order_acquire)) {
                            csm_out = c_winner;
                            st = st_winner;
                            adopted_prefix = true;
                            std::size_t sumU2 = 0;
                            for (const auto u : st.U_row) {
                                sumU2 += static_cast<std::size_t>(u);
                            }
                            baseline_csm = csm_out;
                            baseline_st = st;
                            BlockSolveSnapshot::RestartEvent ev{};
                            ev.restart_index = kRestarts + 1;
                            ev.prefix_rows = std::min<std::size_t>(valid_now + 1, S);
                            ev.unknown_total = sumU2;
                            ev.action = BlockSolveSnapshot::RestartAction::lockInFinal;
                            snap.restarts.push_back(ev);
                            ++snap.final_backtrack1_prefix_locks;
                            set_block_solve_snapshot(snap);
                        }
                    }
                    if (adopted_prefix) {
                        for (int it = 0; it < 100 && !det.solved(); ++it) {
                            if (det.solve_step() == 0) {
                                break;
                            }
                        }
                    }
                    // Limited two-cell lookahead on boundary if still not adopted
                    if (!adopted_prefix && K >= 2 && modeB) {

                        ++snap.final_backtrack2_attempts;

                        const std::size_t K2 = std::min<std::size_t>(
                            (modeB ? 4U : 6U),
                            cand_cols.size()
                        );

                        for (std::size_t a = 0; a + 1 < K2 && !adopted_prefix; ++a) {
                            for (std::size_t b = a + 1; b < K2 && !adopted_prefix; ++b) {
                                const std::size_t c_i = cand_cols[a].second;
                                const std::size_t c_j = cand_cols[b].second;
                                for (int mask = 0; mask < 4 && !adopted_prefix; ++mask) {
                                    const bool vi = ((mask & 1) != 0);
                                    const bool vj = ((mask & 2) != 0);
                                    Csm c_try = csm_out;
                                    ConstraintState st_try = st;
                                    // apply i
                                    c_try.put(boundary, c_i, vi);
                                    c_try.lock(boundary, c_i);
                                    if (st_try.U_row.at(boundary) > 0) {
                                        --st_try.U_row.at(boundary);
                                    }
                                    if (st_try.U_col.at(c_i) > 0) {
                                        --st_try.U_col.at(c_i);
                                    }

                                    const std::size_t d_i = (c_i >= boundary) ? (c_i - boundary) : (c_i + S - boundary);
                                    const std::size_t x_i = (boundary + c_i) % S;

                                    if (st_try.U_diag.at(d_i) > 0) {
                                        --st_try.U_diag.at(d_i);
                                    }
                                    if (st_try.U_xdiag.at(x_i) > 0) {
                                        --st_try.U_xdiag.at(x_i);
                                    }
                                    if (vi) {
                                        if (st_try.R_row.at(boundary) > 0) {
                                            --st_try.R_row.at(boundary);
                                        }
                                        if (st_try.R_col.at(c_i) > 0) {
                                            --st_try.R_col.at(c_i);
                                        }
                                        if (st_try.R_diag.at(d_i) > 0) {
                                            --st_try.R_diag.at(d_i);
                                        }
                                        if (st_try.R_xdiag.at(x_i) > 0) {
                                            --st_try.R_xdiag.at(x_i);
                                        }
                                    }
                                    // apply j
                                    c_try.put(boundary, c_j, vj);
                                    c_try.lock(boundary, c_j);
                                    if (st_try.U_row.at(boundary) > 0) {
                                        --st_try.U_row.at(boundary);
                                    }
                                    if (st_try.U_col.at(c_j) > 0) {
                                        --st_try.U_col.at(c_j);
                                    }

                                    const std::size_t d_j = (c_j >= boundary) ? (c_j - boundary) : (c_j + S - boundary);
                                    const std::size_t x_j = (boundary + c_j) % S;

                                    if (st_try.U_diag.at(d_j) > 0) {
                                        --st_try.U_diag.at(d_j);
                                    }
                                    if (st_try.U_xdiag.at(x_j) > 0) {
                                        --st_try.U_xdiag.at(x_j);
                                    }
                                    if (vj) {
                                        if (st_try.R_row.at(boundary) > 0) {
                                            --st_try.R_row.at(boundary);
                                        }
                                        if (st_try.R_col.at(c_j) > 0) {
                                            --st_try.R_col.at(c_j);
                                        }
                                        if (st_try.R_diag.at(d_j) > 0) {
                                            --st_try.R_diag.at(d_j);
                                        }
                                        if (st_try.R_xdiag.at(x_j) > 0) {
                                            --st_try.R_xdiag.at(x_j);
                                        }
                                    }
                                    DeterministicElimination det_bt{c_try, st_try};

                                    const int bt_iters2 = 2000;
                                    for (int it = 0; it < bt_iters2; ++it) {
                                        const std::size_t prog = det_bt.solve_step();
                                        if (st_try.U_row.at(boundary) == 0) {
                                            break;
                                        }
                                        if (prog == 0) {
                                            break;
                                        }
                                    }

                                    if (st_try.U_row.at(boundary) == 0) {
                                        const std::size_t check_rows = std::min<std::size_t>(valid_now + 1, S);
                                        const RowHashVerifier verifier_try;
                                        if (check_rows > 0 && verifier_try.verify_rows(c_try, lh, check_rows)) {
                                            csm_out = c_try;
                                            st = st_try;
                                            adopted_prefix = true;
                                            std::size_t sumU2 = 0;
                                            for (const auto u : st.U_row) {
                                                sumU2 += static_cast<std::size_t>(u);
                                            }
                                            ::crsce::o11y::metric(
                                                "prefix_locked_in_final",
                                                static_cast<std::int64_t>(check_rows),
                                                {
                                                    {"unknown", std::to_string(sumU2)},
                                                    {"ci", std::to_string(c_i)},
                                                    {"cj", std::to_string(c_j)},
                                                    {"mask", std::to_string(mask)}
                                                }
                                            );
                                            baseline_csm = csm_out; baseline_st = st;
                                            BlockSolveSnapshot::RestartEvent ev{};
                                            ev.restart_index = kRestarts + 2;
                                            ev.prefix_rows = check_rows;
                                            ev.unknown_total = sumU2;
                                            ev.action = BlockSolveSnapshot::RestartAction::lockInPair;
                                            snap.restarts.push_back(ev);
                                            ++snap.final_backtrack2_prefix_locks;
                                            ::crsce::o11y::counter("final_backtrack2_prefix_locks");
                                            set_block_solve_snapshot(snap);
                                        }
                                    }
                                }
                            }
                        }
                            if (adopted_prefix) {
                                for (int it = 0; it < 100 && !det.solved(); ++it) {
                                    if (det.solve_step() == 0) {
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                // Backtracking: try a single high-uncertainty cell (always on)
                if (!det.solved()) {
                    {
                        // Pick row with the most unknowns
                        std::size_t r_pick = 0;
                        std::uint16_t maxU = 0;
                        for (std::size_t r = 0; r < S; ++r) {
                            if (st.U_row.at(r) > maxU) {
                                maxU = st.U_row.at(r);
                                r_pick = r;
                            }
                        }

                        // Find a high-pressure unlocked column in that row (avoid obvious sinks)
                        bool found = false;
                        std::size_t c_pick = 0;
                        double best_score = -1.0;
                        for (std::size_t c = 0; c < S; ++c) {
                            if (csm_out.is_locked(r_pick, c)) {
                                continue;
                            }

                            const std::size_t d = ::crsce::decompress::detail::calc_d(r_pick, c);
                            const std::size_t x = ::crsce::decompress::detail::calc_x(r_pick, c);

                            const double u_c = std::max(1.0, static_cast<double>(st.U_col.at(c)));
                            const double u_d = std::max(1.0, static_cast<double>(st.U_diag.at(d)));
                            const double u_x = std::max(1.0, static_cast<double>(st.U_xdiag.at(x)));
                            const double rneed_c = std::fabs((static_cast<double>(st.R_col.at(c)) / u_c) - 0.5);
                            const double rneed_d = std::fabs((static_cast<double>(st.R_diag.at(d)) / u_d) - 0.5);
                            const double rneed_x = std::fabs((static_cast<double>(st.R_xdiag.at(x)) / u_x) - 0.5);
                            const double score = (0.5 * rneed_c) + (0.25 * rneed_d) + (0.25 * rneed_x);

                            if (score > best_score) {
                                best_score = score;
                                c_pick = c; found = true;
                            }
                        }
                        if (found && maxU > 0) {
                            bool adopted = false;
                            for (int vv = 0; vv < 2 && !adopted; ++vv) {
                                const bool assume_one = (vv == 1);
                                Csm c_try = csm_out;
                                ConstraintState st_try = st;
                                c_try.put(r_pick, c_pick, assume_one);
                                c_try.lock(r_pick, c_pick);

                                if (st_try.U_row.at(r_pick) > 0) {
                                    --st_try.U_row.at(r_pick);
                                }

                                if (st_try.U_col.at(c_pick) > 0) {
                                    --st_try.U_col.at(c_pick);
                                }

                                const std::size_t d = ::crsce::decompress::detail::calc_d(r_pick, c_pick);
                                const std::size_t x = ::crsce::decompress::detail::calc_x(r_pick, c_pick);

                                if (st_try.U_diag.at(d) > 0) {
                                    --st_try.U_diag.at(d);
                                }

                                if (st_try.U_xdiag.at(x) > 0) {
                                    --st_try.U_xdiag.at(x);
                                }

                                if (assume_one) {

                                    if (st_try.R_row.at(r_pick) > 0) {
                                        --st_try.R_row.at(r_pick);
                                    }

                                    if (st_try.R_col.at(c_pick) > 0) {
                                        --st_try.R_col.at(c_pick);
                                    }

                                    if (st_try.R_diag.at(d) > 0) {
                                        --st_try.R_diag.at(d);
                                    }

                                    if (st_try.R_xdiag.at(x) > 0) {
                                        --st_try.R_xdiag.at(x);
                                    }

                                }

                                DeterministicElimination det_bt{c_try, st_try};

                                static constexpr int bt_iters = 1200;

                                for (int it = 0; it < bt_iters; ++it) {
                                    const std::size_t prog = det_bt.solve_step();
                                    if (det_bt.solved()) {
                                        break;
                                    }
                                    if (prog == 0) {
                                        break;
                                    }
                                }
                                if (det_bt.solved()) {
                                    csm_out = c_try;
                                    st = st_try;
                                    adopted = true;
                                }
                            }
                            if (adopted) {
                                // After adoption, run a quick DE sweep to stabilize
                                for (int it = 0; it < 50 && !det.solved(); ++it) {
                                    if (det.solve_step() == 0) {
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                if (!det.solved()) {
                    ::crsce::o11y::event("block_end_failed");
                    snap.phase = BlockSolveSnapshot::Phase::endOfIterations;
                    set_block_solve_snapshot(snap);
                    return false;
                }
            }
            {
                const auto t0cs = std::chrono::steady_clock::now();
                const bool okcs = verify_cross_sums(csm_out, lsm, vsm, dsm, xsm);
                const auto t1cs = std::chrono::steady_clock::now();
                snap.time_cross_verify_ms += static_cast<std::size_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(t1cs - t0cs).count()
                );
                if (!okcs) {
                    ::crsce::o11y::event("block_end_verify_fail");
                    ::crsce::o11y::metric("block_end_verify_fail", 1LL);
                    snap.phase = BlockSolveSnapshot::Phase::verify;
                    snap.message = "cross-sum verification failed";
                    set_block_solve_snapshot(snap);
                    return false;
                }
            }
            const RowHashVerifier verifier;
            const auto t0va = std::chrono::steady_clock::now();
            const bool result = verifier.verify_all(csm_out, lh);
            const auto t1va = std::chrono::steady_clock::now();
            {
                const auto ms = static_cast<std::size_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(t1va - t0va).count()
                );
                snap.time_verify_all_ms += ms;
                snap.time_lh_ms += ms;
            }
            if (result) {
                ::crsce::o11y::event("block_end_ok");
            } else {
                ::crsce::o11y::event("block_end_verify_fail");
            }
            return result;
        }
    }
}
#ifdef __clang__
#  pragma clang diagnostic pop
#endif
