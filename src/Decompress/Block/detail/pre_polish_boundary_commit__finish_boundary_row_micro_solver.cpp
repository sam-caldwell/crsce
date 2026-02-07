/**
 * @file pre_polish_boundary_commit__finish_boundary_row_micro_solver.cpp
 * @brief Definition of finish_boundary_row_micro_solver.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/Block/detail/pre_polish_finish_boundary_row_micro_solver.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>
#include <span>
#include <functional>
#include <string>
#include <cstdlib>
#include <chrono>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Utils/detail/index_calc.h"
#include "decompress/Block/detail/micro_solver_helpers.h"

namespace crsce::decompress::detail {
    using crsce::decompress::RowHashVerifier;
    using crsce::decompress::DeterministicElimination;

    // Helpers are declared inline in micro_solver_helpers.h to avoid extra constructs here.

    /**
     * @name finish_boundary_row_micro_solver
     * @brief Focused micro-solver to complete a boundary row via ranked single-cell trials and DE.
     * @param csm_out In/out CSM under construction.
     * @param st In/out constraint state.
     * @param lh LH digest span.
     * @param baseline_csm Out: updated baseline CSM if adoption occurs.
     * @param baseline_st Out: updated baseline state if adoption occurs.
     * @param snap In/out snapshot for metrics and events.
     * @param rs Current restart index for event attribution.
     * @return bool True if boundary completion adopted; false otherwise.
     */
    bool finish_boundary_row_micro_solver(Csm &csm_out,
                                          ConstraintState &st,
                                          std::span<const std::uint8_t> lh,
                                          Csm &baseline_csm,
                                          ConstraintState &baseline_st,
                                          BlockSolveSnapshot &snap,
                                          int rs) {
        const auto t0 = std::chrono::steady_clock::now();
        ++snap.micro_solver_attempts;
        constexpr std::size_t S = Csm::kS;
        // Boundary row index = first row with unknown cells; LH is verified before adoption
        std::size_t r = 0;
        for (; r < S; ++r) { if (baseline_st.U_row.at(r) > 0) { break; } }
        if (r >= S) { return false; }

        // Work copies
        Csm c = baseline_csm;
        ConstraintState w = baseline_st;

        if (w.U_row.at(r) == 0) { return false; }
        // Gate by near-threshold unknowns (default 20%; overridable via CRSCE_FOCUS_NEAR_THRESH)
        auto near_thresh = static_cast<std::uint16_t>((S + 4U) / 5U);
        if (const char *e = std::getenv("CRSCE_FOCUS_NEAR_THRESH") /* NOLINT(concurrency-mt-unsafe) */; e && *e) {
            int pct = std::atoi(e);
            pct = std::max(pct, 5);
            pct = std::min(pct, 70);
            near_thresh = static_cast<std::uint16_t>((static_cast<unsigned>(S) * static_cast<unsigned>(pct) + 99U) / 100U);
        }
        if (w.U_row.at(r) > near_thresh) { return false; }
        // Window cap for heavier micro-solvers (DP / BnB). Default W=64, overridable via CRSCE_MS_WINDOW.
        std::size_t window_cap = 64;
        if (const char *wcap = std::getenv("CRSCE_MS_WINDOW") /* NOLINT(concurrency-mt-unsafe) */; wcap && *wcap) {
            const int v = std::atoi(wcap);
            if (v > 0) { window_cap = static_cast<std::size_t>(v); }
            window_cap = std::max<std::size_t>(window_cap, 32);
            window_cap = std::min<std::size_t>(window_cap, 128);
        }
        // Additional hard gate: remaining ones must fit the window to limit branching
        if (w.R_row.at(r) > window_cap) { return false; }

        // Collect unknown cells in row
        std::vector<std::size_t> cols; cols.reserve(S);
        for (std::size_t c0 = 0; c0 < S; ++c0) {
            if (!c.is_locked(r, c0)) { cols.push_back(c0); }
        }
        if (cols.empty()) { return false; }

        // Quick forced propagation loop using local feasibility

        bool changed = true;
        int guard = 0;
        while (changed && guard++ < 4096 && w.U_row.at(r) > 0) {
            changed = false;
            for (std::size_t idx = 0; idx < cols.size() && w.U_row.at(r) > 0; ++idx) {
                const std::size_t cc = cols[idx];
                if (c.is_locked(r, cc)) { continue; }
                const bool can1 = local_feasible(w, r, cc, true);
                const bool can0 = local_feasible(w, r, cc, false);
                if (!can1 && !can0) {
                    // Infeasible state; give up
                    return false;
                }
                if (w.R_row.at(r) == 0 && can0) { assign_cell(c, w, r, cc, false); changed = true; continue; }
                if (w.R_row.at(r) == w.U_row.at(r) && can1) { assign_cell(c, w, r, cc, true); changed = true; continue; }
                if (can1 && !can0) { assign_cell(c, w, r, cc, true); changed = true; continue; }
                if (!can1 && can0) { assign_cell(c, w, r, cc, false); changed = true; continue; }
            }
        }

        if (w.U_row.at(r) == 0) {
            const std::size_t check_rows = std::min<std::size_t>(r + 1, S);
            const RowHashVerifier ver_try{};
            if (check_rows > 0 && ver_try.verify_rows(c, lh, check_rows)) {
                csm_out = c; st = w; baseline_csm = csm_out; baseline_st = st;
                BlockSolveSnapshot::RestartEvent ev{};
            ev.restart_index = rs; ev.prefix_rows = check_rows; ev.unknown_total = snap.unknown_total; ev.action = BlockSolveSnapshot::RestartAction::lockInMicro;
                snap.restarts.push_back(ev);
                return true;
            }
            // else fall through to heuristic fill
        }

        // Exact selection via DP: pick exactly T ones from top-W free candidates
        if (w.U_row.at(r) > 0) {
            const std::uint16_t target_ones0 = w.R_row.at(r);
            // Partition into must1, must0, free
            std::vector<std::size_t> must1; must1.reserve(cols.size());
            std::vector<std::size_t> must0; must0.reserve(cols.size());
            std::vector<std::pair<double, std::size_t>> free_cand; free_cand.reserve(cols.size());
            // Optional prior weight from env (soft heuristic; default 0)
            double prior_w = 0.0;
            if (const char *p = std::getenv("CRSCE_HW_PRIOR") /* NOLINT(concurrency-mt-unsafe) */; p && *p) {
                try { prior_w = std::stod(p); } catch (...) { prior_w = 0.0; }
                prior_w = std::max(0.0, prior_w);
                prior_w = std::min(2.0, prior_w);
            }
            for (const auto cc_all : cols) {
                if (c.is_locked(r, cc_all)) { continue; }
                const bool can1 = local_feasible(w, r, cc_all, true);
                const bool can0 = local_feasible(w, r, cc_all, false);
                if (!can1 && !can0) { return false; }
                if (can1 && !can0) { must1.push_back(cc_all); continue; }
                if (!can1 && can0) { must0.push_back(cc_all); continue; }
                // free cell: compute score
                const double belief = c.get_data(r, cc_all);
                const double pc = static_cast<double>(w.R_col.at(cc_all)) / static_cast<double>(std::max<std::uint16_t>(1, w.U_col.at(cc_all)));
                const std::size_t d = ::crsce::decompress::detail::calc_d(r, cc_all);
                const std::size_t x = ::crsce::decompress::detail::calc_x(r, cc_all);
                const double pd = static_cast<double>(w.R_diag.at(d)) / static_cast<double>(std::max<std::uint16_t>(1, w.U_diag.at(d)));
                const double px = static_cast<double>(w.R_xdiag.at(x)) / static_cast<double>(std::max<std::uint16_t>(1, w.U_xdiag.at(x)));
                const double pressure = (0.5 * pc) + (0.25 * pd) + (0.25 * px);
                const double prior = prior_w * hw_prior_score(r, cc_all);
                const double score = (0.5 * belief) + (0.5 * pressure) + prior;
                free_cand.emplace_back(score, cc_all);
            }
            if (must1.size() > static_cast<std::size_t>(target_ones0)) { return false; }
            const std::size_t remaining = static_cast<std::size_t>(target_ones0) - must1.size();
            // Apply must1 now
            for (const auto cc : must1) { assign_cell(c, w, r, cc, true); }
            // If already satisfied target, assign all others to zero where feasible
            if (remaining == 0) {
                for (const auto cc_all : cols) {
                    if (c.is_locked(r, cc_all)) { continue; }
                    if (local_feasible(w, r, cc_all, false)) { assign_cell(c, w, r, cc_all, false); }
                }
            } else if (!free_cand.empty()) {
                // Sort descending by score
                std::ranges::sort(free_cand, std::greater<double>{}, &std::pair<double,std::size_t>::first);
                // Determine DP cap within window; allow boost when remaining is small
                std::size_t capN = std::min<std::size_t>(free_cand.size(), window_cap);
                if (remaining <= 96 && free_cand.size() >= std::min<std::size_t>(window_cap, 96)) { capN = std::min<std::size_t>(window_cap, 96); }
                if (remaining <= 128 && free_cand.size() >= std::min<std::size_t>(window_cap, 128)) { capN = std::min<std::size_t>(window_cap, 128); }
                // Estimate how many of the first cap cells can be 1 by local feasibility
                std::size_t cap_can1 = 0; for (std::size_t j = 0; j < capN; ++j) { if (local_feasible(w, r, free_cand[j].second, true)) { ++cap_can1; } }
                // DP is worth trying when the boundary row is near-threshold and the cap can supply remaining ones
                const bool dp_gate_ok = (w.U_row.at(r) <= near_thresh) && (capN > 0) && (remaining <= capN) && (cap_can1 >= remaining);
                bool dp_succeeded = false;
                if (dp_gate_ok) {
                    ++snap.micro_solver_dp_attempts;
                    // Prepare DP arrays
                    static constexpr double kNegInf = -1.0e300;
                    std::vector<double> dp((capN + 1U) * (remaining + 1U), kNegInf);
                    std::vector<unsigned char> take((capN + 1U) * (remaining + 1U), 0);
                    dp[dp_index(0,0,remaining)] = 0.0;
                    for (std::size_t i = 0; i < capN; ++i) {
                        const double sc = free_cand[i].first;
                        for (std::size_t k = 0; k <= remaining; ++k) {
                            const double v0 = dp[dp_index(i, k, remaining)];
                            dp[dp_index(i+1, k, remaining)] = std::max(v0, dp[dp_index(i+1, k, remaining)]);
                            if (k + 1U <= remaining) {
                                const double cand = v0 + sc;
                                if (cand > dp[dp_index(i+1, k+1U, remaining)]) {
                                    dp[dp_index(i+1, k+1U, remaining)] = cand;
                                    take[dp_index(i+1, k+1U, remaining)] = 1;
                                }
                            }
                        }
                    }
                    const bool dp_ok = (dp[dp_index(capN, remaining, remaining)] > kNegInf/2.0);
                    if (dp_ok) { ++snap.micro_solver_dp_feasible; } else { ++snap.micro_solver_dp_infeasible; }
                    if (dp_ok) {
                        // Reconstruct chosen indices
                        std::vector<std::size_t> chosen; chosen.reserve(remaining);
                        std::size_t i = capN; std::size_t k = remaining;
                        while (i > 0) {
                            if (take[dp_index(i, k, remaining)] != 0U) { chosen.push_back(i - 1U); --k; }
                            --i;
                        }
                        // Try k-best sets via exclusion
                        // Candidate sets are tried via helper (try_apply_set)
                        // Try best set first
                        {
                            Csm c_best_local; ConstraintState w_best_local; bool found_local = false; std::size_t nodes_dummy = 0;
                            if (try_apply_set(chosen, free_cand, capN, c_best_local, w_best_local, found_local, nodes_dummy, c, w, r, lh, snap)) {
                                csm_out = c_best_local; st = w_best_local; baseline_csm = csm_out; baseline_st = st;
                                BlockSolveSnapshot::RestartEvent ev{};
                                ev.restart_index = rs; ev.prefix_rows = std::min<std::size_t>(r + 1, S); ev.unknown_total = snap.unknown_total; ev.action = BlockSolveSnapshot::RestartAction::lockInMicro;
                                snap.restarts.push_back(ev);
                                ++snap.micro_solver_successes;
                                const auto t1 = std::chrono::steady_clock::now();
                                snap.micro_solver_time_ms += static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
                                return true;
                            }
                        }
                        // Try a few exclusions to get alternative sets
                        const std::size_t k_best = 6;
                        const std::size_t exclude_trials = std::min<std::size_t>(k_best - 1U, chosen.size());
                        for (std::size_t ex = 0; ex < exclude_trials; ++ex) {
                            // Recompute DP excluding chosen[ex]
                            const std::size_t ex_idx = chosen[ex];
                            std::vector<double> dp2((capN + 1U) * (remaining + 1U), kNegInf);
                            std::vector<unsigned char> take2((capN + 1U) * (remaining + 1U), 0);
                            dp2[dp_index(0,0,remaining)] = 0.0;
                            for (std::size_t i2 = 0; i2 < capN; ++i2) {
                                if (i2 == ex_idx) {
                                    for (std::size_t k2 = 0; k2 <= remaining; ++k2) {
                                        const double v0 = dp2[dp_index(i2, k2, remaining)];
                                        dp2[dp_index(i2+1, k2, remaining)] = std::max(v0, dp2[dp_index(i2+1, k2, remaining)]);
                                    }
                                    continue;
                                }
                                const double sc2 = free_cand[i2].first;
                                for (std::size_t k2 = 0; k2 <= remaining; ++k2) {
                                    const double v0 = dp2[dp_index(i2, k2, remaining)];
                                    dp2[dp_index(i2+1, k2, remaining)] = std::max(v0, dp2[dp_index(i2+1, k2, remaining)]);
                                    if (k2 + 1U <= remaining) {
                                        const double cand = v0 + sc2;
                                        if (cand > dp2[dp_index(i2+1, k2+1U, remaining)]) {
                                            dp2[dp_index(i2+1, k2+1U, remaining)] = cand;
                                            take2[dp_index(i2+1, k2+1U, remaining)] = 1;
                                        }
                                    }
                                }
                            }
                            if (dp2[dp_index(capN, remaining, remaining)] > kNegInf/2.0) {
                                std::vector<std::size_t> chosen2; chosen2.reserve(remaining);
                                std::size_t ii = capN; std::size_t kk = remaining;
                                while (ii > 0) {
                                    if (take2[dp_index(ii, kk, remaining)] != 0U) { chosen2.push_back(ii - 1U); --kk; }
                                    --ii;
                                }
                                {
                                    Csm c_best_local; ConstraintState w_best_local; bool found_local = false; std::size_t nodes_dummy = 0;
                                    if (try_apply_set(chosen2, free_cand, capN, c_best_local, w_best_local, found_local, nodes_dummy, c, w, r, lh, snap)) {
                                        csm_out = c_best_local; st = w_best_local; baseline_csm = csm_out; baseline_st = st;
                                        BlockSolveSnapshot::RestartEvent ev{};
                                        ev.restart_index = rs; ev.prefix_rows = std::min<std::size_t>(r + 1, S); ev.unknown_total = snap.unknown_total; ev.action = BlockSolveSnapshot::RestartAction::lockInMicro;
                                        snap.restarts.push_back(ev);
                                        ++snap.micro_solver_successes;
                                        const auto t1 = std::chrono::steady_clock::now();
                                        snap.micro_solver_time_ms += static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
                                        return true;
                                    }
                                }
                            } else {
                                ++snap.micro_solver_dp_infeasible;
                            }
                        }
                        dp_succeeded = false; // fall through to other finishers
                    }
                }
                // If DP gated out or did not succeed, try small-window branch-and-bound over top cap
                if (!dp_gate_ok || !dp_succeeded) {
                    // Run BnB even if remaining > window, using cap window and greedily filling beyond-cap ones later
                    if (!free_cand.empty()) {
                        ++snap.micro_solver_bnb_attempts;
                        // Depth-first BnB: choose exactly 'remaining' ones among the first capN candidates
                        const std::size_t cap_for_bnb = std::min<std::size_t>(free_cand.size(), window_cap);
                        // Work copies seeded from current c/w
                        Csm c_best = c; ConstraintState w_best = w; bool found = false; std::size_t nodes = 0;
                        dfs_bnb(0U, 0U, free_cand, cap_for_bnb, remaining, r, lh, c_best, w_best, found, nodes, c, w, snap);
                        if (found) {
                            csm_out = c_best; st = w_best; baseline_csm = csm_out; baseline_st = st;
                            BlockSolveSnapshot::RestartEvent ev{};
                            ev.restart_index = rs; ev.prefix_rows = std::min<std::size_t>(r + 1, S); ev.unknown_total = snap.unknown_total; ev.action = BlockSolveSnapshot::RestartAction::lockInMicro;
                            snap.restarts.push_back(ev);
                            ++snap.micro_solver_successes;
                            ++snap.micro_solver_bnb_successes;
                            snap.micro_solver_bnb_nodes += nodes;
                            const auto t1 = std::chrono::steady_clock::now();
                            snap.micro_solver_time_ms += static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
                            return true;
                        }
                        snap.micro_solver_bnb_nodes += nodes;
                    }
                }
                // Fallback: greedy like before
                std::size_t picked = 0;
                for (const auto &p : free_cand) {
                    if (picked >= remaining) { break; }
                    const std::size_t cc = p.second;
                    if (!c.is_locked(r, cc) && local_feasible(w, r, cc, true)) { assign_cell(c, w, r, cc, true); ++picked; }
                }
                for (const auto &p : free_cand) {
                    const std::size_t cc = p.second;
                    if (c.is_locked(r, cc)) { continue; }
                    if (w.U_row.at(r) == 0) { break; }
                    if (local_feasible(w, r, cc, false)) { assign_cell(c, w, r, cc, false); }
                }
            }
        }

        // Run a localized DE settle; stop if no progress
        {
            DeterministicElimination det{c, w};
            for (int it = 0; it < 8000; ++it) {
                const std::size_t prog = det.solve_step();
                if (prog == 0 || w.U_row.at(r) == 0) { break; }
            }
        }

        if (w.U_row.at(r) == 0) {
            const std::size_t check_rows = std::min<std::size_t>(r + 1, S);
            const RowHashVerifier ver_try{};
            if (check_rows > 0) { ++snap.micro_solver_lh_verifications; }
            if (check_rows > 0 && ver_try.verify_rows(c, lh, check_rows)) {
                csm_out = c; st = w; baseline_csm = csm_out; baseline_st = st;
                BlockSolveSnapshot::RestartEvent ev{};
            ev.restart_index = rs; ev.prefix_rows = check_rows; ev.unknown_total = snap.unknown_total; ev.action = BlockSolveSnapshot::RestartAction::lockInMicro;
                snap.restarts.push_back(ev);
                ++snap.micro_solver_successes;
                const auto t1 = std::chrono::steady_clock::now();
                snap.micro_solver_time_ms += static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
                return true;
            }
        }
        const auto t1 = std::chrono::steady_clock::now();
        snap.micro_solver_time_ms += static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
        return false;
    }
}
