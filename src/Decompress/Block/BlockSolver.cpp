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
#include "decompress/Block/detail/pre_polish_boundary_commit.h"
#include "decompress/Block/detail/solver_env.h"

#include <cstddef>
#include <cstdint> //NOLINT
#include <cstdlib>
#include <iostream>
#include <array>
#include <algorithm>
#include <random>
#include <span>
#include <string>
#include <exception>
#include <vector>
#include <utility>
#include <functional>
#include <cmath>

namespace {
    using crsce::decompress::Csm;
    using crsce::decompress::ConstraintState;
    using crsce::decompress::DeterministicElimination;
    using crsce::decompress::RowHashVerifier;
    using crsce::decompress::BlockSolveSnapshot;
    using crsce::decompress::detail::read_seed_or_default;
    using crsce::decompress::detail::trace_sumu_enabled;
    using crsce::decompress::detail::read_env_double;
    using crsce::decompress::detail::read_env_int;
} // anonymous namespace

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

        // Pre-lock padded cells (beyond valid_bits) as zeros and decrement unknown counts.
        if (constexpr std::uint64_t total_bits = static_cast<std::uint64_t>(S) * static_cast<std::uint64_t>(S);
            valid_bits < total_bits) {
            // Optional debug: emit one-line summary when enabled
            static int prelock_dbg = -1; // -1 unset, 0 off, 1 on
            if (prelock_dbg < 0) {
                const char *e = std::getenv("CRSCE_PRELOCK_DEBUG"); // NOLINT(concurrency-mt-unsafe)
                prelock_dbg = (e && *e) ? 1 : 0;
            }
            if (prelock_dbg == 1) {
                const auto start_r = static_cast<std::size_t>(valid_bits / S);
                const auto start_c = static_cast<std::size_t>(valid_bits % S);
                std::cerr << "Pre-locking padded tail from bit " << valid_bits
                        << " (r=" << start_r << ", c=" << start_c << ") count="
                        << (total_bits - valid_bits) << "\n";
            }
            for (std::uint64_t idx = valid_bits; idx < total_bits; ++idx) {
                const auto r = static_cast<std::size_t>(idx / S);
                const auto c = static_cast<std::size_t>(idx % S);
                // Set and lock the padded cell
                csm_out.put(r, c, false);
                csm_out.lock(r, c);
                // Update unknown counts for affected lines
                if (st.U_row.at(r) > 0) { --st.U_row.at(r); }
                if (st.U_col.at(c) > 0) { --st.U_col.at(c); }
                const std::size_t d = (c >= r) ? (c - r) : (c + S - r);
                const std::size_t x = (r + c) % S;
                if (st.U_diag.at(d) > 0) { --st.U_diag.at(d); }
                if (st.U_xdiag.at(x) > 0) { --st.U_xdiag.at(x); }
            }
        }
        DeterministicElimination det{csm_out, st};

        // Seed initial beliefs from LSM, balanced by VSM/DSM/XSM residual pressure and add small jitter (±0.02)
        std::uint64_t belief_seed_used = 0;
        {
            belief_seed_used = read_seed_or_default(0x0BADC0FFEEULL);
            std::mt19937_64 rng(belief_seed_used);
            std::uniform_real_distribution<double> noise(-0.02, 0.02);
            constexpr double high = 0.90;
            constexpr double low = 0.10;
            for (std::size_t r = 0; r < S; ++r) {
                if (st.U_row.at(r) == 0) { continue; } // already fully locked (e.g., padded row)
                const auto ones = static_cast<std::size_t>(st.R_row.at(r));
                // Build candidates scored by (residual/unknown) across VSM/DSM/XSM
                std::vector<std::pair<double, std::size_t>> cand; cand.reserve(S);
                for (std::size_t c = 0; c < S; ++c) {
                    if (csm_out.is_locked(r, c)) { continue; }
                    const std::size_t d = (c >= r) ? (c - r) : (c + S - r);
                    const std::size_t x = (r + c) % S;
                    const double col_need = static_cast<double>(st.R_col.at(c)) / static_cast<double>(std::max<std::uint16_t>(1, st.U_col.at(c)));
                    const double diag_need = static_cast<double>(st.R_diag.at(d)) / static_cast<double>(std::max<std::uint16_t>(1, st.U_diag.at(d)));
                    const double x_need = static_cast<double>(st.R_xdiag.at(x)) / static_cast<double>(std::max<std::uint16_t>(1, st.U_xdiag.at(x)));
                    const double score = col_need + diag_need + x_need;
                    cand.emplace_back(score, c);
                }
                if (cand.empty()) { continue; }
                std::ranges::sort(cand, std::greater<>{});
                const std::size_t pick = std::min<std::size_t>(ones, cand.size());
                // Mark chosen columns as high belief; rest as low
                std::vector<char> chosen(S, 0);
                for (std::size_t i = 0; i < pick; ++i) { chosen[cand[i].second] = 1; }
                for (std::size_t c = 0; c < S; ++c) {
                    if (csm_out.is_locked(r, c)) { continue; }
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
        snap.phase = "init";
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

        // Hard-coded deterministic elimination iteration cap
        static constexpr int kMaxIters = 60000;

        for (int iter = 0; iter < kMaxIters; ++iter) {
            std::size_t progress = 0;
            try {
                snap.phase = "de";
                progress += det.solve_step();
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
                    std::cerr << "DE iter=" << iter << " sumU=" << sumU << " progress=" << progress << "\n";
                }
            }
            set_block_solve_snapshot(snap);
            // Prefix gating: lock any contiguous valid prefix after improvements
            {
                if (::crsce::decompress::detail::commit_valid_prefix(csm_out, st, lh, baseline_csm, baseline_st, snap, /*rs*/0)) {
                    for (int it = 0; it < 50 && !det.solved(); ++it) { if (det.solve_step() == 0) { break; } }
                }
            }
            if (det.solved()) {
                std::cerr << "Block Solver terminating with possible solution\n";
                break;
            }
            // Optional early stop when no additional progress is made.
            if (progress == 0) {
            std::cerr << "Block Solver reached steady state (no progress)\n";
            // Force early finisher attempts before GOBP
            try {
                (void)::crsce::decompress::detail::finish_boundary_row_adaptive(csm_out, st, lh, /*baseline*/baseline_csm, /*baseline*/baseline_st, snap, /*rs*/0, /*stall_ticks*/600);
                (void)::crsce::decompress::detail::finish_near_complete_top_rows(csm_out, st, lh, baseline_csm, baseline_st, snap, /*rs*/0, /*top_n*/32, /*top_k_cells*/256);
                (void)::crsce::decompress::detail::finish_near_complete_top_columns(csm_out, st, lh, baseline_csm, baseline_st, snap, /*rs*/0, /*top_n_cols*/16, /*top_k_cells*/160);
            } catch (...) { /* ignore */ }
            break;
        }
            std::cerr << "Block Solver iteration ended at " << iter << " of " << kMaxIters
                    << " iterations (with " << progress << " progress)\n";
        }
        std::cerr << "Block Solver terminating...\n";
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
                    std::uniform_real_distribution<double> delta(-jitter, jitter);
                    for (std::size_t r0 = 0; r0 < S; ++r0) {
                        for (std::size_t c0 = 0; c0 < S; ++c0) {
                            if (csm_out.is_locked(r0, c0)) { continue; }
                            const double v = csm_out.get_data(r0, c0);
                            const double j = delta(rng);
                            const double nv = std::clamp(v + j, 0.0, 1.0);
                            csm_out.set_data(r0, c0, nv);
                        }
                    }
                }

                if (multiphase) {
                // Four-phase schedule (adaptive copies)
                // 0: bootstrap; 1: smoothing; 2: REHEAT; 3: POLISH
                std::array<double, 4> dampers{{0.50, 0.10, 0.35, 0.01}};
                std::array<double, 4> confs{{0.995, 0.80, 0.90, 0.48}};
                std::array<int, 4> iters_arr{{8000, 12000, 220000, 1200000}}; // increased effort
                // Optional per-phase env overrides
                confs[0] = read_env_double("CRSCE_GOBP_PHASE1_CONF", confs[0]);
                confs[1] = read_env_double("CRSCE_GOBP_PHASE2_CONF", confs[1]);
                confs[2] = read_env_double("CRSCE_GOBP_PHASE3_CONF", confs[2]);
                confs[3] = read_env_double("CRSCE_GOBP_PHASE4_CONF", confs[3]);
                dampers[0] = read_env_double("CRSCE_GOBP_PHASE1_DAMP", dampers[0]);
                dampers[1] = read_env_double("CRSCE_GOBP_PHASE2_DAMP", dampers[1]);
                dampers[2] = read_env_double("CRSCE_GOBP_PHASE3_DAMP", dampers[2]);
                dampers[3] = read_env_double("CRSCE_GOBP_PHASE4_DAMP", dampers[3]);
                iters_arr[0] = read_env_int("CRSCE_GOBP_PHASE1_ITERS", iters_arr[0]);
                iters_arr[1] = read_env_int("CRSCE_GOBP_PHASE2_ITERS", iters_arr[1]);
                iters_arr[2] = read_env_int("CRSCE_GOBP_PHASE3_ITERS", iters_arr[2]);
                iters_arr[3] = read_env_int("CRSCE_GOBP_PHASE4_ITERS", iters_arr[3]);
                // Polish-stage micro-shake attempts on plateau before giving up
                static constexpr int kPolishShakes = 6;
                static constexpr double kPolishShakeJitter = 0.008;

                // Per-row pre-anneal commit and finisher
                (void)::crsce::decompress::detail::commit_valid_prefix(csm_out, st, lh, baseline_csm, baseline_st, snap, rs);
                while (::crsce::decompress::detail::commit_any_verified_rows(csm_out, st, lh, baseline_csm, baseline_st, snap, rs)) {
                    for (int it = 0; it < 25 && !det.solved(); ++it) { if (det.solve_step() == 0) { break; } }
                }
                (void)::crsce::decompress::detail::finish_near_complete_any_row(csm_out, st, lh, baseline_csm, baseline_st, snap, rs);
                    int since_restart = 0;
                    const int dynamic_cooldown_base = 150; // reduced cooldown for exploration
                    std::vector<std::size_t> recent_restart_rows;
                    for (std::size_t ph = 0; ph < dampers.size() && !det.solved(); ++ph) {
                        // Adaptive anneal tweaks based on stall
                        if (since_restart > 400 && ph == 1) { confs.at(ph) = std::max(0.60, confs.at(ph) - 0.10); dampers.at(ph) = std::max(0.05, dampers.at(ph) - 0.05); }
                        if (since_restart > 800 && ph == 2) { confs.at(ph) = std::max(0.70, confs.at(ph) - 0.05); dampers.at(ph) = std::max(0.10, dampers.at(ph) - 0.05); }
                        GobpSolver gobp(csm_out, st, dampers.at(ph), confs.at(ph));
                        int polish_shakes_remaining = (ph == (dampers.size() - 1)) ? kPolishShakes : 0;
                        int no_gprog_streak = 0;
                        bool scan_flipped = false;
                        int plateau_flip_attempts = 0;
                        for (int gi = 0; gi < iters_arr.at(ph); ++gi) {
                            std::size_t gprog = 0;
                            try {
                            snap.phase = "gobp";
                            {
                                const std::size_t gobp_only = gobp.solve_step();
                                gprog += gobp_only;
                                snap.gobp_cells_solved_total += gobp_only;
                            }
                            // Follow with a DE sweep to propagate any newly forced moves
                            gprog += det.solve_step();
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
                                for (const auto u: st.U_row) { sumU += static_cast<std::size_t>(u); }
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
                            for (const auto u: st.U_row) { sumU += static_cast<std::size_t>(u); }
                            snap.unknown_total = sumU;
                            snap.solved = (static_cast<std::size_t>(S) * static_cast<std::size_t>(S)) - sumU;
                            snap.unknown_history.push_back(sumU);
                            if (trace_sumu_enabled()) {
                                std::cerr << "GOBP ph=" << ph << " gi=" << gi << " sumU=" << sumU << " gprog=" << gprog << "\n";
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
                                continue; // retry immediately with flipped (or restored) perspective
                            }
                        } else {
                            plateau_flip_attempts = 0; // reset when progress is made
                        }
                        // Per-row/col gating and finishers (high cadence) with sink-avoidance
                        {
                            ++snap.gating_calls;
                            bool any_commit = false;
                            if (::crsce::decompress::detail::commit_valid_prefix(csm_out, st, lh, baseline_csm, baseline_st, snap, rs)) { any_commit = true; }
                            if (::crsce::decompress::detail::commit_any_verified_rows(csm_out, st, lh, baseline_csm, baseline_st, snap, rs)) { any_commit = true; }
                            (void)::crsce::decompress::detail::finish_boundary_row_adaptive(csm_out, st, lh, baseline_csm, baseline_st, snap, rs, since_restart);
                            // Build sink-averse scored candidates for rows/cols
                            std::vector<std::pair<double,std::size_t>> row_scores; row_scores.reserve(S);
                            std::vector<std::pair<double,std::size_t>> col_scores; col_scores.reserve(S);
                            const double w1 = 0.6; const double w2 = 0.4;
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
                            for (std::size_t i = 0; i < row_scores.size() && i < 24; ++i) { best_rows.push_back(row_scores[i].second); }
                            for (std::size_t i = 0; i < col_scores.size() && i < 16; ++i) { best_cols.push_back(col_scores[i].second); }
                            // Try scored rows/cols first (avoid sinks); fall back to top_* helpers
                            const bool r_try = ::crsce::decompress::detail::finish_near_complete_rows_scored(csm_out, st, lh, baseline_csm, baseline_st, snap, rs, best_rows, 256);
                            const bool c_try = ::crsce::decompress::detail::finish_near_complete_columns_scored(csm_out, st, lh, baseline_csm, baseline_st, snap, rs, best_cols, 160);
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
                            // Always-on micro-shake on plateau to perturb beliefs slightly (after flip attempts)
                            if (gprog == 0 && plateau_flip_attempts >= 3) {
                                std::uniform_real_distribution<double> delta(-0.002, 0.002);
                                for (std::size_t r0 = 0; r0 < S; ++r0) {
                                    for (std::size_t c0 = 0; c0 < S; ++c0) {
                                        if (csm_out.is_locked(r0, c0)) { continue; }
                                        const double v = csm_out.get_data(r0, c0);
                                        const double nv = std::clamp(v + delta(rng), 0.0, 1.0);
                                        csm_out.set_data(r0, c0, nv);
                                    }
                                }
                            }
                            // Track consecutive no-progress cycles; allow up to 10, then terminate this phase
                            if (gprog == 0 && plateau_flip_attempts >= 3) {
                                ++no_gprog_streak;
                                if (no_gprog_streak >= 10) {
                                    std::cerr << "GOBP no-progress streak >= 10; terminating phase\n";
                                    break;
                                }
                            } else {
                                no_gprog_streak = 0;
                            }
                            int repeats = 0;
                            if (!snap.restarts.empty()) {
                                const auto last_r = snap.restarts.back().prefix_rows;
                                int cnt = 0; for (const auto r0 : recent_restart_rows) { if (r0 == last_r) { ++cnt; } }
                                repeats = cnt;
                            }
                            const int cooldown_ticks = std::clamp(dynamic_cooldown_base + (since_restart / 2) - (repeats * 150), 100, 2000);
                            if (::crsce::decompress::detail::audit_and_restart_on_contradiction(csm_out, st, lh, baseline_csm, baseline_st, snap, rs, valid_bits, cooldown_ticks, since_restart)) {
                                if (!snap.restarts.empty()) {
                                    const auto r = snap.restarts.back().prefix_rows;
                                    recent_restart_rows.push_back(r);
                                    if (recent_restart_rows.size() > 8U) { recent_restart_rows.erase(recent_restart_rows.begin()); }
                                }
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
                            for (; boundary < S; ++boundary) { if (st.U_row.at(boundary) > 0) { break; } }
                            if (boundary < S) {
                                const RowHashVerifier verifier_now;
                                const std::size_t valid_now = verifier_now.longest_valid_prefix_up_to(csm_out, lh, S);
        const auto near_thresh = static_cast<std::uint16_t>((S + 4U) / 5U); // ~20%
                                if (st.U_row.at(boundary) > 0 && st.U_row.at(boundary) <= near_thresh) {
                                    int steps = 0;
                                    static constexpr int kFocusMaxSteps = 48;
                                    static constexpr int kFocusBtIters = 8000;
                                    while (st.U_row.at(boundary) > 0 && steps < kFocusMaxSteps) {
                                        ++steps;
                                        // Pick first unlocked cell in boundary row
                                        bool found = false; std::size_t c_pick = 0;
                                        for (std::size_t c0 = 0; c0 < S; ++c0) { if (!csm_out.is_locked(boundary, c0)) { c_pick = c0; found = true; break; } }
                                        if (!found) { break; }
                                        bool adopted_step = false;
                                        const std::uint16_t before = st.U_row.at(boundary);
                                        for (int vv = 0; vv < 2 && !adopted_step; ++vv) {
                                            const bool assume_one = (vv == 1);
                                            Csm c_try = csm_out;
                                            ConstraintState st_try = st;
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
                                                    if (check_rows > 0 && verifier_try.verify_rows(c_try, lh, check_rows)) {
                                                        // Commit the try-state and baseline; this grows the verified prefix
                                                        csm_out = c_try;
                                                        st = st_try;
                                                        adopted_step = true;
                                                        std::size_t sumU2 = 0; for (const auto u : st.U_row) { sumU2 += static_cast<std::size_t>(u); }
                                                        std::cerr << "PREFIX LOCKED-IN: rows=" << check_rows << ", unknown=" << sumU2 << ", rs=" << rs << "\n";
                                                        baseline_csm = csm_out;
                                                        baseline_st = st;
                                                        BlockSolveSnapshot::RestartEvent ev{};
                                                        ev.restart_index = rs;
                                                        ev.prefix_rows = check_rows;
                                                        ev.unknown_total = sumU2;
                                                        ev.action = "lock-in";
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
                                                }
                                            }
                                        }
                                        if (!adopted_step) { break; }
                                    }
                                    if (steps > 0) {
                                        // Quick DE settle after local improvements
                                        for (int it1 = 0; it1 < 50 && !det.solved(); ++it1) { if (det.solve_step() == 0) { break; } }
                                        // If boundary completed, immediately run prefix gating and update baseline/restart
                                        if (st.U_row.at(boundary) == 0) {
                                            std::size_t prefix_rows2 = 0;
                                            for (std::size_t r1 = 0; r1 < S; ++r1) {
                                                if (st.U_row.at(r1) == 0) { ++prefix_rows2; } else { break; }
                                            }
                                            const RowHashVerifier verifier;
                                            if (prefix_rows2 > 0 && !verifier.verify_rows(csm_out, lh, prefix_rows2)) {
                                                std::size_t sumU2 = 0; for (const auto u : st.U_row) { sumU2 += static_cast<std::size_t>(u); }
                                                std::cerr << "PREFIX RESTART: rows=" << prefix_rows2 << ", unknown=" << sumU2 << ", rs=" << rs << "\n";
                                                csm_out = baseline_csm;
                                                st = baseline_st;
                                                BlockSolveSnapshot::RestartEvent ev{};
                                                ev.restart_index = rs;
                                                ev.prefix_rows = prefix_rows2;
                                                ev.unknown_total = sumU2;
                                                ev.action = "restart";
                                                snap.restarts.push_back(ev);
                                                set_block_solve_snapshot(snap);
                                                restart_triggered = true;
                                            } else if (prefix_rows2 > 0) {
                                                std::size_t sumU2 = 0; for (const auto u : st.U_row) { sumU2 += static_cast<std::size_t>(u); }
                                                std::cerr << "PREFIX LOCKED-IN: rows=" << prefix_rows2 << ", unknown=" << sumU2 << ", rs=" << rs << "\n";
                                                baseline_csm = csm_out;
                                                baseline_st = st;
                                                BlockSolveSnapshot::RestartEvent ev{};
                                                ev.restart_index = rs;
                                                ev.prefix_rows = prefix_rows2;
                                                ev.unknown_total = sumU2;
                                                ev.action = "lock-in";
                                                snap.restarts.push_back(ev);
                                                set_block_solve_snapshot(snap);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        if (det.solved()) {
                            std::cerr << "GOBP converged to a solution\n";
                            break;
                        }
                        if (gprog == 0 && plateau_flip_attempts >= 3) {
                            // On last phase (polish), try a few micro-shake attempts before giving up
                            if (polish_shakes_remaining > 0 && ph == (dampers.size() - 1)) {
                                const double jitter = kPolishShakeJitter * (static_cast<double>(kPolishShakes - polish_shakes_remaining + 1));
                                std::uniform_real_distribution<double> delta(-jitter, jitter);
                                for (std::size_t r0 = 0; r0 < S; ++r0) {
                                    for (std::size_t c0 = 0; c0 < S; ++c0) {
                                        if (csm_out.is_locked(r0, c0)) { continue; }
                                        const double v = csm_out.get_data(r0, c0);
                                        const double j = delta(rng);
                                        const double nv = std::clamp(v + j, 0.0, 1.0);
                                        csm_out.set_data(r0, c0, nv);
                                    }
                                }
                                // Log event and continue without breaking the phase loop
                                std::size_t prefix_rows = 0; for (std::size_t r0 = 0; r0 < S; ++r0) { if (st.U_row.at(r0) == 0) { ++prefix_rows; } else { break; } }
                                std::size_t sumU = 0; for (const auto u : st.U_row) { sumU += static_cast<std::size_t>(u); }
                                std::cerr << "POLISH SHAKE: jitter=" << jitter << ", shakes_left=" << (polish_shakes_remaining - 1)
                                          << ", prefix_rows=" << prefix_rows << ", unknown=" << sumU << ", rs=" << rs << "\n";
                                {
                                    BlockSolveSnapshot::RestartEvent ev{};
                                    ev.restart_index = rs;
                                    ev.prefix_rows = prefix_rows;
                                    ev.unknown_total = sumU;
                                    ev.action = "polish-shake";
                                    snap.restarts.push_back(ev);
                                }
                                set_block_solve_snapshot(snap);
                                --polish_shakes_remaining;
                                continue; // try again within the polish phase
                            }
                            std::cerr << "GOBP reached steady state (no progress)\n";
                            break;
                        }
                    }
                    if (restart_triggered) { break; }
                }
            }
            // Close restart loop
            }

            // Backtracking: try a single high-uncertainty cell (always on)
            if (!det.solved()) {
                {
                    // Pick row with the most unknowns
                    std::size_t r_pick = 0;
                    std::uint16_t maxU = 0;
                    for (std::size_t r = 0; r < S; ++r) {
                        if (st.U_row.at(r) > maxU) { maxU = st.U_row.at(r); r_pick = r; }
                    }
                    // Find an unlocked column in that row
                    bool found = false; std::size_t c_pick = 0;
                    for (std::size_t c = 0; c < S; ++c) {
                        if (!csm_out.is_locked(r_pick, c)) { c_pick = c; found = true; break; }
                    }
                    if (found && maxU > 0) {
                        bool adopted = false;
                        for (int vv = 0; vv < 2 && !adopted; ++vv) {
                            const bool assume_one = (vv == 1);
                            Csm c_try = csm_out;
                            ConstraintState st_try = st;
                            c_try.put(r_pick, c_pick, assume_one);
                            c_try.lock(r_pick, c_pick);
                            if (st_try.U_row.at(r_pick) > 0) { --st_try.U_row.at(r_pick); }
                            if (st_try.U_col.at(c_pick) > 0) { --st_try.U_col.at(c_pick); }
                            const std::size_t d = (c_pick >= r_pick) ? (c_pick - r_pick) : (c_pick + S - r_pick);
                            const std::size_t x = (r_pick + c_pick) % S;
                            if (st_try.U_diag.at(d) > 0) { --st_try.U_diag.at(d); }
                            if (st_try.U_xdiag.at(x) > 0) { --st_try.U_xdiag.at(x); }
                            if (assume_one) {
                                if (st_try.R_row.at(r_pick) > 0) { --st_try.R_row.at(r_pick); }
                                if (st_try.R_col.at(c_pick) > 0) { --st_try.R_col.at(c_pick); }
                                if (st_try.R_diag.at(d) > 0) { --st_try.R_diag.at(d); }
                                if (st_try.R_xdiag.at(x) > 0) { --st_try.R_xdiag.at(x); }
                            }
                            DeterministicElimination det_bt{c_try, st_try};
                            static constexpr int bt_iters = 1200;
                            for (int it = 0; it < bt_iters; ++it) {
                                const std::size_t prog = det_bt.solve_step();
                                if (det_bt.solved()) { break; }
                                if (prog == 0) { break; }
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
                                if (det.solve_step() == 0) { break; }
                            }
                        }
                    }
                }
            }

            if (!det.solved()) {
                std::cerr << "Block Solver failed\n";
                snap.phase = "end-of-iterations";
                set_block_solve_snapshot(snap);
                return false;
            }
        }
        if (!verify_cross_sums(csm_out, lsm, vsm, dsm, xsm)) {
            std::cerr << "Block Solver failed to verify cross sums\n";
            snap.phase = "verify";
            snap.message = "cross-sum verification failed";
            set_block_solve_snapshot(snap);
            return false;
        }
        std::cerr << "Block Solver finished successfully (LH verification pending)\n";
        const RowHashVerifier verifier;
        const bool result = verifier.verify_all(csm_out, lh);
        if (result) {
            std::cerr << "Block Solver finished successfully\n";
        } else {
            std::cerr << "Block Solver failed to verify cross sums\n";
        }
        return result;
    }
}
