/**
 * @file pre_polish_boundary_commit__finish_boundary_row_micro_solver.cpp
 * @brief Definition of finish_boundary_row_micro_solver.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
// Implementation file intentionally omits its own header to satisfy include-cleaner.

#include <algorithm>
#include <cstddef>
#include <cstdint> // NOLINT
#include <utility>
#include <vector>
#include <span>
#include <functional>
#include <string>
#include <cstdlib>
#include <chrono>
#include <ranges>
#include <cmath>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Utils/detail/calc_d.h"
#include "decompress/Utils/detail/calc_x.h"
#include "micro_solver_helpers.h"

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
    bool finish_boundary_row_micro_solver(Csm &csm_out, // NOLINT(misc-use-internal-linkage,readability-function-size)
                                          ConstraintState &st,
                                          std::span<const std::uint8_t> lh,
                                          Csm &baseline_csm,
                                          ConstraintState &baseline_st,
                                          BlockSolveSnapshot &snap,
                                          int rs) {
        const auto t0 = std::chrono::steady_clock::now();
        ++snap.micro_solver_attempts;
        constexpr std::size_t S = Csm::kS;
        // Boundary row index = first row with unknown cells in current state; LH is verified before adoption
        std::size_t r = 0;
        for (; r < S; ++r) { if (st.U_row.at(r) > 0) { break; } }
        if (r >= S) { return false; }

        // Work copies based on current state
        Csm c = csm_out;
        ConstraintState w = st;
        const std::uint16_t u_initial = w.U_row.at(r);

        if (w.U_row.at(r) == 0) { ++snap.micro_solver_gate_row_full; return false; }
        // Gate by near-threshold unknowns (default ~70%; overridable via CRSCE_FOCUS_NEAR_THRESH)
        auto near_thresh = static_cast<std::uint16_t>(((S * 7U) + 9U) / 10U);
        if (const char *e = std::getenv("CRSCE_FOCUS_NEAR_THRESH") /* NOLINT(concurrency-mt-unsafe) */; e && *e) {
            int pct = std::atoi(e);
            pct = std::max(pct, 5);
            pct = std::min(pct, 70);
            near_thresh = static_cast<std::uint16_t>(
                (static_cast<unsigned>(S) * static_cast<unsigned>(pct) + 99U) / 100U);
        }
        if (w.U_row.at(r) > near_thresh) { ++snap.micro_solver_gate_near_thresh; return false; }
        // Window cap for heavier micro-solvers (DP / BnB). Default W=64, overridable via CRSCE_MS_WINDOW.
        // Note: this cap limits only DP/BnB candidate windows; we still allow the
        // greedy/feasibility path to operate on larger rows so we don't bail out early.
        std::size_t window_cap = 96;
        if (const char *wcap = std::getenv("CRSCE_MS_WINDOW") /* NOLINT(concurrency-mt-unsafe) */; wcap && *wcap) {
            const int v = std::atoi(wcap);
            if (v > 0) { window_cap = static_cast<std::size_t>(v); }
            window_cap = std::max<std::size_t>(window_cap, 32);
            window_cap = std::min<std::size_t>(window_cap, 128);
        }
        // Do not hard-fail when remaining ones exceed the window; DP/BnB will be gated by
        // the window, and we will still attempt a greedy + DE settle path for the row.

        // Collect unknown cells in a row
        std::vector<std::size_t> cols;
        cols.reserve(S);
        for (std::size_t c0 = 0; c0 < S; ++c0) {
            if (!c.is_locked(r, c0)) { cols.push_back(c0); }
        }
        if (cols.empty()) {
            ++snap.micro_solver_gate_cols_empty;
            return false;
        }

        // Quick forced propagation loop using local feasibility

        bool changed = true;
        bool infeasible_detected = false;
        int guard = 0;
        while (changed && guard++ < 4096 && w.U_row.at(r) > 0) {
            changed = false;
            for (std::size_t idx = 0; idx < cols.size() && w.U_row.at(r) > 0; ++idx) {
                const std::size_t cc = cols[idx];
                if (c.is_locked(r, cc)) {
                    continue;
                }
                const bool can1 = local_feasible(w, r, cc, true);
                const bool can0 = local_feasible(w, r, cc, false);
                if (!can1 && !can0) {
                    // Infeasible cell under current residuals; break out to try DP/BnB fallback
                    infeasible_detected = true;
                    break;
                }
                if (w.R_row.at(r) == 0 && can0) {
                    assign_cell(c, w, r, cc, false);
                    changed = true;
                    continue;
                }
                if (w.R_row.at(r) == w.U_row.at(r) && can1) {
                    assign_cell(c, w, r, cc, true);
                    changed = true;
                    continue;
                }
                if (can1 && !can0) {
                    assign_cell(c, w, r, cc, true);
                    changed = true;
                    continue;
                }
                if (!can1 && can0) {
                    assign_cell(c, w, r, cc, false);
                    changed = true;
                    continue;
                }
            }
            if (infeasible_detected) { break; }
        }

        if (w.U_row.at(r) == 0) {
            const std::size_t check_rows = std::min<std::size_t>(r + 1, S);
            if (check_rows > 0) { ++snap.micro_solver_candidates; }
            if (constexpr RowHashVerifier ver_try{}; check_rows > 0 && ver_try.verify_rows(c, lh, check_rows)) {
                // Optional acceptance threshold: require minimum cells changed to accept (env: CRSCE_MS_ACCEPT_MIN_CELLS)
                std::size_t min_cells = 0;
                if (const char *p = std::getenv("CRSCE_MS_ACCEPT_MIN_CELLS") /* NOLINT(concurrency-mt-unsafe) */; p && *p) {
                    const auto v = std::strtoll(p, nullptr, 10); if (v > 0) { min_cells = static_cast<std::size_t>(v); }
                }
                const auto delta_cells = static_cast<std::size_t>(u_initial);
                if (min_cells > 0 && delta_cells < min_cells) {
                    ++snap.micro_solver_reject_low_benefit;
                } else {
                csm_out = c;
                st = w;
                baseline_csm = csm_out;
                baseline_st = st;
                BlockSolveSnapshot::RestartEvent ev{};
                ev.restart_index = rs;
                ev.prefix_rows = check_rows;
                ev.unknown_total = snap.unknown_total;
                ev.action = BlockSolveSnapshot::RestartAction::lockInMicro;
                snap.restarts.push_back(ev);
                return true;
                }
            }
            else { ++snap.micro_solver_verify_failures; ++snap.verify_rows_failures; }
            // else fall through to heuristic fill
        }

        // Exact selection via DP: pick exactly T ones from top-W free candidates
        if (w.U_row.at(r) > 0) {
            ++snap.micro_solver_entered_dp_stage;
            const std::uint16_t target_ones0 = w.R_row.at(r);
            // Partition into must1, must0, free
            std::vector<std::size_t> must1;
            must1.reserve(cols.size());
            std::vector<std::size_t> must0;
            must0.reserve(cols.size());
            std::vector<std::pair<double, std::size_t> > free_cand;
            free_cand.reserve(cols.size());
            // Optional prior weight from env (soft heuristic; default 0)
            double prior_w = 0.0;
            if (const char *p = std::getenv("CRSCE_HW_PRIOR") /* NOLINT(concurrency-mt-unsafe) */; p && *p) {
                try {
                    prior_w = std::stod(p);
                } catch (...) {
                    prior_w = 0.0;
                }
                prior_w = std::max(0.0, prior_w);
                prior_w = std::min(2.0, prior_w);
            }
            for (const auto cc_all: cols) {
                if (c.is_locked(r, cc_all)) {
                    continue;
                }
                const bool can1 = local_feasible(w, r, cc_all, true);
                const bool can0 = local_feasible(w, r, cc_all, false);
                if (!can1 && !can0) {
                    /* skip this variable for DP window consideration */
                    continue;
                }
                if (can1 && !can0) {
                    must1.push_back(cc_all);
                    continue;
                }
                if (!can1 && can0) {
                    must0.push_back(cc_all);
                    continue;
                }
                // free cell: compute score
                const double belief = c.get_data(r, cc_all);
                const double pc = static_cast<double>(w.R_col.at(cc_all)) / static_cast<double>(std::max<std::uint16_t>(
                                      1, w.U_col.at(cc_all)));
                const std::size_t d = ::crsce::decompress::detail::calc_d(r, cc_all);
                const std::size_t x = ::crsce::decompress::detail::calc_x(r, cc_all);
                const double pd = static_cast<double>(w.R_diag.at(d)) / static_cast<double>(std::max<std::uint16_t>(
                                      1, w.U_diag.at(d)));
                const double px = static_cast<double>(w.R_xdiag.at(x)) / static_cast<double>(std::max<std::uint16_t>(
                                      1, w.U_xdiag.at(x)));
                const double pressure = (0.5 * pc) + (0.25 * pd) + (0.25 * px);
                const double prior = prior_w * hw_prior_score(r, cc_all);
                const double score = (0.5 * belief) + (0.5 * pressure) + prior;
                free_cand.emplace_back(score, cc_all);
            }
            if (must1.size() > static_cast<std::size_t>(target_ones0)) {
                return false;
            }
            const std::size_t remaining = static_cast<std::size_t>(target_ones0) - must1.size();
            // Apply must1 now
            for (const auto cc: must1) {
                assign_cell(c, w, r, cc, true);
            }
            // If already satisfied target, assign all others to zero where feasible
            if (remaining == 0) {
                ++snap.micro_solver_remaining_zero_cases;
                for (const auto cc_all: cols) {
                    if (c.is_locked(r, cc_all)) {
                        continue;
                    }
                    if (local_feasible(w, r, cc_all, false)) {
                        assign_cell(c, w, r, cc_all, false);
                    }
                }
            } else if (!free_cand.empty()) {
                // Sort descending by score
                std::ranges::sort(free_cand, std::greater<double>{}, &std::pair<double, std::size_t>::first);
                // Determine DP/BnB window size and picks inside the window
                const std::size_t capN = std::min<std::size_t>(free_cand.size(), std::max<std::size_t>(window_cap, 32));
                const std::size_t target_k = std::min<std::size_t>(remaining, capN);
                const bool dp_gate_ok = (capN > 0) && (target_k > 0);
                bool dp_succeeded = false;
                if (dp_gate_ok) {
                    ++snap.micro_solver_dp_attempts;
                    // Prepare DP arrays
                    static constexpr double kNegInf = -1.0e300;
                    std::vector<double> dp((capN + 1U) * (target_k + 1U), kNegInf);
                    std::vector<unsigned char> take((capN + 1U) * (target_k + 1U), 0);
                    dp[dp_index(0, 0, target_k)] = 0.0;
                    for (std::size_t i = 0; i < capN; ++i) {
                        const double sc = free_cand[i].first;
                        for (std::size_t k = 0; k <= target_k; ++k) {
                            const double v0 = dp[dp_index(i, k, target_k)];
                            dp[dp_index(i + 1, k, target_k)] = std::max(v0, dp[dp_index(i + 1, k, target_k)]);
                            if (k + 1U <= target_k) {
                                if (const double cand = v0 + sc; cand > dp[dp_index(i + 1, k + 1U, target_k)]) {
                                    dp[dp_index(i + 1, k + 1U, target_k)] = cand;
                                    take[dp_index(i + 1, k + 1U, target_k)] = 1;
                                }
                            }
                        }
                    }
                    const bool dp_ok = (dp[dp_index(capN, target_k, target_k)] > kNegInf / 2.0);
                    if (dp_ok) {
                        ++snap.micro_solver_dp_feasible;
                    } else {
                        ++snap.micro_solver_dp_infeasible;
                    }
                    if (dp_ok) {
                        // Reconstruct chosen indices
                        std::vector<std::size_t> chosen;
                        chosen.reserve(target_k);
                        std::size_t i = capN;
                        std::size_t k = target_k;
                        while (i > 0) {
                            if (take[dp_index(i, k, target_k)] != 0U) {
                                chosen.push_back(i - 1U);
                                --k;
                            }
                            --i;
                        }
                        // Try k-best sets via exclusion (K-variants around the DP optimum)
                        // Candidate sets are tried via helper (try_apply_set)
                        // Try the best set first
                        {
                            ConstraintState w_best_local;
                            bool found_local = false;
                            std::size_t nodes_dummy = 0;
                            if (Csm c_best_local;
                                try_apply_set(chosen, free_cand, capN, c_best_local,
                                              w_best_local, found_local,
                                              nodes_dummy, c, w, r, lh, snap)
                            ) {
                                // Optional acceptance threshold
                                std::size_t min_cells = 0;
                                if (const char *p = std::getenv("CRSCE_MS_ACCEPT_MIN_CELLS") /* NOLINT(concurrency-mt-unsafe) */; p && *p) {
                                    const auto v = std::strtoll(p, nullptr, 10); if (v > 0) { min_cells = static_cast<std::size_t>(v); }
                                }
                                const std::size_t after_u = w_best_local.U_row.at(r);
                                const auto delta_cells = static_cast<std::size_t>(u_initial) - after_u;
                                if (min_cells > 0 && delta_cells < min_cells) {
                                    ++snap.micro_solver_reject_low_benefit;
                                } else {
                                    csm_out = c_best_local;
                                    st = w_best_local;
                                    baseline_csm = csm_out;
                                    baseline_st = st;
                                    BlockSolveSnapshot::RestartEvent ev{};
                                    ev.restart_index = rs;
                                    ev.prefix_rows = std::min<std::size_t>(r + 1, S);
                                    ev.unknown_total = snap.unknown_total;
                                    ev.action = BlockSolveSnapshot::RestartAction::lockInMicro;
                                    snap.restarts.push_back(ev);
                                    ++snap.micro_solver_successes;
                                    const auto t1 = std::chrono::steady_clock::now();
                                    snap.micro_solver_time_ms += static_cast<std::size_t>(
                                        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
                                    );
                                    return true;
                                }
                            }
                        }
                        // Single exclusions from early picks
                        {
                            std::size_t max_single = 16;
                            if (const char *e = std::getenv("CRSCE_MS_KVAR_SINGLE") /* NOLINT(concurrency-mt-unsafe) */; e && *e) {
                                const auto v = std::strtoll(e, nullptr, 10); if (v > 0) { max_single = static_cast<std::size_t>(v); }
                            }
                            max_single = std::min<std::size_t>(max_single, chosen.size());
                            for (std::size_t ex = 0; ex < max_single; ++ex) {
                                std::vector<std::size_t> variant = chosen;
                                variant.erase(variant.begin() + static_cast<std::ptrdiff_t>(ex));
                                ConstraintState w_best_local; bool found_local = false; std::size_t nodes_dummy = 0; Csm c_best_local;
                                if (try_apply_set(variant, free_cand, capN, c_best_local, w_best_local, found_local, nodes_dummy, c, w, r, lh, snap)) {
                                    csm_out = c_best_local; st = w_best_local; baseline_csm = csm_out; baseline_st = st;
                                    BlockSolveSnapshot::RestartEvent ev{}; ev.restart_index = rs; ev.prefix_rows = std::min<std::size_t>(r + 1, S); ev.unknown_total = snap.unknown_total; ev.action = BlockSolveSnapshot::RestartAction::lockInMicro; snap.restarts.push_back(ev);
                                    ++snap.micro_solver_successes; const auto t1s = std::chrono::steady_clock::now(); snap.micro_solver_time_ms += static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1s - t0).count());
                                    return true;
                                }
                            }
                        }
                        // Pairwise exclusions among early picks
                        {
                            std::size_t max_pair = 8;
                            if (const char *e = std::getenv("CRSCE_MS_KVAR_PAIR") /* NOLINT(concurrency-mt-unsafe) */; e && *e) {
                                const auto v = std::strtoll(e, nullptr, 10); if (v > 0) { max_pair = static_cast<std::size_t>(v); }
                            }
                            max_pair = std::min<std::size_t>(max_pair, chosen.size());
                            for (std::size_t a = 0; a < max_pair; ++a) {
                                for (std::size_t b = a + 1; b < max_pair; ++b) {
                                    std::vector<std::size_t> variant = chosen;
                                    variant.erase(variant.begin() + static_cast<std::ptrdiff_t>(b));
                                    variant.erase(variant.begin() + static_cast<std::ptrdiff_t>(a));
                                    ConstraintState w_best_local; bool found_local = false; std::size_t nodes_dummy = 0; Csm c_best_local;
                                    if (try_apply_set(variant, free_cand, capN, c_best_local, w_best_local, found_local, nodes_dummy, c, w, r, lh, snap)) {
                                        csm_out = c_best_local; st = w_best_local; baseline_csm = csm_out; baseline_st = st;
                                        BlockSolveSnapshot::RestartEvent ev{}; ev.restart_index = rs; ev.prefix_rows = std::min<std::size_t>(r + 1, S); ev.unknown_total = snap.unknown_total; ev.action = BlockSolveSnapshot::RestartAction::lockInMicro; snap.restarts.push_back(ev);
                                        ++snap.micro_solver_successes; const auto t1p = std::chrono::steady_clock::now(); snap.micro_solver_time_ms += static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1p - t0).count());
                                        return true;
                                    }
                                }
                            }
                        }
                        // Swap variants: swap early chosen indices with top not-chosen ambiguous candidates (within window)
                        {
                            // Build membership for chosen within [0, capN)
                            std::vector<char> in_chosen(capN, 0);
                            for (const auto idx : chosen) { if (idx < capN) { in_chosen[idx] = 1; } }
                            // Collect not-chosen indices within window, scored by ambiguity |belief-0.5|
                            std::vector<std::pair<double,std::size_t>> amb_pool; amb_pool.reserve(capN);
                            for (std::size_t idx = 0; idx < capN; ++idx) {
                                if (in_chosen[idx] != 0) { continue; }
                                const std::size_t cc = free_cand[idx].second;
                                const double v = c.get_data(r, cc);
                                const double ambv = std::fabs(v - 0.5);
                                amb_pool.emplace_back(ambv, idx);
                            }
                            if (!amb_pool.empty()) {
                                std::ranges::sort(amb_pool, std::less<double>{}, &std::pair<double,std::size_t>::first);
                                std::size_t swap_pool = 64;
                                if (const char *e = std::getenv("CRSCE_MS_KVAR_SWAP_POOL") /* NOLINT(concurrency-mt-unsafe) */; e && *e) {
                                    const auto v = std::strtoll(e, nullptr, 10); if (v > 0) { swap_pool = static_cast<std::size_t>(v); }
                                }
                                swap_pool = std::min<std::size_t>(swap_pool, amb_pool.size());
                                std::size_t max_swaps_per_pos = 16;
                                if (const char *e = std::getenv("CRSCE_MS_KVAR_SWAPS") /* NOLINT(concurrency-mt-unsafe) */; e && *e) {
                                    const auto v = std::strtoll(e, nullptr, 10); if (v > 0) { max_swaps_per_pos = static_cast<std::size_t>(v); }
                                }
                                const std::size_t pos_limit = std::min<std::size_t>(chosen.size(), 24);
                                for (std::size_t ex = 0; ex < pos_limit; ++ex) {
                                    for (std::size_t si = 0; si < swap_pool && si < max_swaps_per_pos; ++si) {
                                        const std::size_t alt_idx = amb_pool[si].second;
                                        if (alt_idx >= capN) { continue; }
                                        // Build variant: replace chosen[ex] with alt_idx
                                        std::vector<std::size_t> variant = chosen;
                                        variant[ex] = alt_idx;
                                        // Deduplicate variant indices to avoid duplicates
                                        {
                                            std::ranges::sort(variant);
                                            auto sub = std::ranges::unique(variant);
                                            variant.erase(sub.begin(), sub.end());
                                            if (variant.size() != chosen.size()) {
                                                // If size changed due to duplicate, skip this swap
                                                continue;
                                            }
                                        }
                                        ConstraintState w_best_local; bool found_local = false; std::size_t nodes_dummy = 0; Csm c_best_local;
                                        if (try_apply_set(variant, free_cand, capN, c_best_local, w_best_local, found_local, nodes_dummy, c, w, r, lh, snap)) {
                                            csm_out = c_best_local; st = w_best_local; baseline_csm = csm_out; baseline_st = st;
                                            BlockSolveSnapshot::RestartEvent ev{}; ev.restart_index = rs; ev.prefix_rows = std::min<std::size_t>(r + 1, S); ev.unknown_total = snap.unknown_total; ev.action = BlockSolveSnapshot::RestartAction::lockInMicro; snap.restarts.push_back(ev);
                                            ++snap.micro_solver_successes; const auto t1sw = std::chrono::steady_clock::now(); snap.micro_solver_time_ms += static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1sw - t0).count());
                                            return true;
                                        }
                                    }
                                }
                            }
                        }
                        // Try a few exclusions to get alternative sets
                        constexpr std::size_t k_best = 6;
                        const std::size_t exclude_trials = std::min<std::size_t>(k_best - 1U, chosen.size());
                        for (std::size_t ex = 0; ex < exclude_trials; ++ex) {
                            // Recompute DP excluding chosen[ex]
                            const std::size_t ex_idx = chosen[ex];
                            std::vector<double> dp2((capN + 1U) * (remaining + 1U), kNegInf);
                            std::vector<unsigned char> take2((capN + 1U) * (remaining + 1U), 0);
                            dp2[dp_index(0, 0, remaining)] = 0.0;
                            for (std::size_t i2 = 0; i2 < capN; ++i2) {
                                if (i2 == ex_idx) {
                                    for (std::size_t k2 = 0; k2 <= remaining; ++k2) {
                                        const double v0 = dp2[dp_index(i2, k2, remaining)];
                                        dp2[dp_index(i2 + 1, k2, remaining)] = std::max(
                                            v0, dp2[dp_index(i2 + 1, k2, remaining)]);
                                    }
                                    continue;
                                }
                                const double sc2 = free_cand[i2].first;
                                for (std::size_t k2 = 0; k2 <= remaining; ++k2) {
                                    const double v0 = dp2[dp_index(i2, k2, remaining)];
                                    dp2[dp_index(i2 + 1, k2, remaining)] = std::max(
                                        v0, dp2[dp_index(i2 + 1, k2, remaining)]);
                                    if (k2 + 1U <= remaining) {
                                        const double cand = v0 + sc2;
                                        if (cand > dp2[dp_index(i2 + 1, k2 + 1U, remaining)]) {
                                            dp2[dp_index(i2 + 1, k2 + 1U, remaining)] = cand;
                                            take2[dp_index(i2 + 1, k2 + 1U, remaining)] = 1;
                                        }
                                    }
                                }
                            }
                            if (dp2[dp_index(capN, remaining, remaining)] > kNegInf / 2.0) {
                                std::vector<std::size_t> chosen2;
                                chosen2.reserve(remaining);
                                std::size_t ii = capN;
                                std::size_t kk = remaining;
                                while (ii > 0) {
                                    if (take2[dp_index(ii, kk, remaining)] != 0U) {
                                        chosen2.push_back(ii - 1U);
                                        --kk;
                                    }
                                    --ii;
                                }
                                {
                                    Csm c_best_local;
                                    ConstraintState w_best_local;
                                    bool found_local = false;
                                    if (std::size_t nodes_dummy = 0; try_apply_set(
                                            chosen2, free_cand, capN, c_best_local,
                                            w_best_local, found_local,
                                            nodes_dummy, c, w, r, lh, snap)
                                    ) {
                                        csm_out = c_best_local;
                                        st = w_best_local;
                                        baseline_csm = csm_out;
                                        baseline_st = st;
                                        BlockSolveSnapshot::RestartEvent ev{};
                                        ev.restart_index = rs;
                                        ev.prefix_rows = std::min<std::size_t>(r + 1, S);
                                        ev.unknown_total = snap.unknown_total;
                                        ev.action = BlockSolveSnapshot::RestartAction::lockInMicro;
                                        snap.restarts.push_back(ev);
                                        ++snap.micro_solver_successes;
                                        const auto t1 = std::chrono::steady_clock::now();
                                        snap.micro_solver_time_ms += static_cast<std::size_t>(
                                            std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
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
                // (two-chunk DP aggregator removed in this iteration to maintain stability)
                // If DP gated out or did not succeed, try an alternative ambiguous DP over most-ambiguous cells
                if ((!dp_gate_ok || !dp_succeeded)) {
                    ++snap.micro_solver_amb_fallback_attempts;
                    std::vector<std::pair<double,std::size_t>> amb; amb.reserve(S);
                    for (const auto cc_all : cols) {
                        if (c.is_locked(r, cc_all)) { continue; }
                        const double belief = c.get_data(r, cc_all);
                        const double ambv = std::fabs(belief - 0.5);
                        amb.emplace_back(ambv, cc_all);
                    }
                    if (!amb.empty()) {
                        std::ranges::sort(amb, std::less<double>{}, &std::pair<double,std::size_t>::first);
                        std::size_t amb_cap = 128; // widened ambiguity window
                        if (const char *e = std::getenv("CRSCE_MS_AMB_FALLBACK") /* NOLINT(concurrency-mt-unsafe) */; e && *e) {
                            const auto v = std::strtoll(e, nullptr, 10); if (v > 0) { amb_cap = static_cast<std::size_t>(v); }
                        }
                        amb_cap = std::max<std::size_t>(amb_cap, 16);
                        amb_cap = std::min<std::size_t>(amb_cap, 160);
                        std::vector<std::pair<double,std::size_t>> fb_cand; fb_cand.reserve(std::min<std::size_t>(amb.size(), amb_cap));
                        for (std::size_t i0 = 0; i0 < amb.size() && i0 < amb_cap; ++i0) {
                            const auto &pr = amb[i0];
                            const double score = 1.0 - pr.first;
                            fb_cand.emplace_back(score, pr.second);
                        }
                        if (!fb_cand.empty()) {
                            const std::size_t capN2 = fb_cand.size();
                            const std::size_t target_k2 = std::min<std::size_t>(remaining, capN2);
                            if (target_k2 > 0) {
                                ++snap.micro_solver_dp_attempts;
                                static constexpr double kNegInf2 = -1.0e300;
                                std::vector<double> dp3((capN2 + 1U) * (target_k2 + 1U), kNegInf2);
                                std::vector<unsigned char> take3((capN2 + 1U) * (target_k2 + 1U), 0);
                                dp3[dp_index(0,0,target_k2)] = 0.0;
                                for (std::size_t i3 = 0; i3 < capN2; ++i3) {
                                    const double sc = fb_cand[i3].first;
                                    for (std::size_t k3 = 0; k3 <= target_k2; ++k3) {
                                        const double v0 = dp3[dp_index(i3, k3, target_k2)];
                                        dp3[dp_index(i3+1, k3, target_k2)] = std::max(v0, dp3[dp_index(i3+1, k3, target_k2)]);
                                        if (k3 + 1U <= target_k2) {
                                            const double cand = v0 + sc;
                                            if (cand > dp3[dp_index(i3+1, k3+1U, target_k2)]) {
                                                dp3[dp_index(i3+1, k3+1U, target_k2)] = cand;
                                                take3[dp_index(i3+1, k3+1U, target_k2)] = 1;
                                            }
                                        }
                                    }
                                }
                                if (dp3[dp_index(capN2, target_k2, target_k2)] > kNegInf2/2.0) {
                                    ++snap.micro_solver_dp_feasible;
                                    std::vector<std::size_t> chosen3; chosen3.reserve(target_k2);
                                    std::size_t i4 = capN2; std::size_t k4 = target_k2;
                                    while (i4 > 0) { if (take3[dp_index(i4, k4, target_k2)] != 0U) { chosen3.push_back(i4 - 1U); --k4; } --i4; }
                                    Csm c_best_local; ConstraintState w_best_local; bool found_local = false; std::size_t nodes_dummy = 0;
                                    if (try_apply_set(chosen3, fb_cand, capN2, c_best_local, w_best_local, found_local, nodes_dummy, c, w, r, lh, snap)) {
                                        csm_out = c_best_local; st = w_best_local; baseline_csm = csm_out; baseline_st = st;
                                        BlockSolveSnapshot::RestartEvent ev{}; ev.restart_index = rs; ev.prefix_rows = std::min<std::size_t>(r + 1, S); ev.unknown_total = snap.unknown_total; ev.action = BlockSolveSnapshot::RestartAction::lockInMicro; snap.restarts.push_back(ev);
                                        ++snap.micro_solver_successes; const auto t1loc = std::chrono::steady_clock::now(); snap.micro_solver_time_ms += static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1loc - t0).count());
                                        return true;
                                    }
                                    // Swap variants over ambiguous window
                                    {
                                        std::vector<char> in_chosen2(capN2, 0);
                                        for (const auto idx : chosen3) { if (idx < capN2) { in_chosen2[idx] = 1; } }
                                        // Ambiguity values are already implicit in fb_cand ordering (from amb sort)
                                        std::vector<std::pair<double,std::size_t>> amb_pool2; amb_pool2.reserve(capN2);
                                        for (std::size_t idx = 0; idx < capN2; ++idx) {
                                            if (in_chosen2[idx] != 0) { continue; }
                                            const std::size_t cc = fb_cand[idx].second;
                                            const double v = c.get_data(r, cc);
                                            const double ambv = std::fabs(v - 0.5);
                                            amb_pool2.emplace_back(ambv, idx);
                                        }
                                        if (!amb_pool2.empty()) {
                                            std::ranges::sort(amb_pool2, std::less<double>{}, &std::pair<double,std::size_t>::first);
                                            std::size_t swap_pool2 = 64;
                                            if (const char *e = std::getenv("CRSCE_MS_KVAR_SWAP_POOL") /* NOLINT(concurrency-mt-unsafe) */; e && *e) {
                                                const auto v = std::strtoll(e, nullptr, 10); if (v > 0) { swap_pool2 = static_cast<std::size_t>(v); }
                                            }
                                            swap_pool2 = std::min<std::size_t>(swap_pool2, amb_pool2.size());
                                            std::size_t max_swaps_pos2 = 16;
                                            if (const char *e = std::getenv("CRSCE_MS_KVAR_SWAPS") /* NOLINT(concurrency-mt-unsafe) */; e && *e) {
                                                const auto v = std::strtoll(e, nullptr, 10); if (v > 0) { max_swaps_pos2 = static_cast<std::size_t>(v); }
                                            }
                                            const std::size_t pos_limit2 = std::min<std::size_t>(chosen3.size(), 40);
                                            for (std::size_t ex = 0; ex < pos_limit2; ++ex) {
                                                for (std::size_t si = 0; si < swap_pool2 && si < max_swaps_pos2; ++si) {
                                                    const std::size_t alt_idx = amb_pool2[si].second;
                                                    if (alt_idx >= capN2) { continue; }
                                                    std::vector<std::size_t> variant2 = chosen3;
                                                    variant2[ex] = alt_idx;
                                                    std::ranges::sort(variant2);
                                                    auto sub2 = std::ranges::unique(variant2);
                                                    variant2.erase(sub2.begin(), sub2.end());
                                                    if (variant2.size() != chosen3.size()) { continue; }
                                                    Csm c_best2; ConstraintState w_best2; bool found2 = false; std::size_t nodes_d2 = 0;
                                                    if (try_apply_set(variant2, fb_cand, capN2, c_best2, w_best2, found2, nodes_d2, c, w, r, lh, snap)) {
                                                        csm_out = c_best2; st = w_best2; baseline_csm = csm_out; baseline_st = st;
                                                        BlockSolveSnapshot::RestartEvent ev{}; ev.restart_index = rs; ev.prefix_rows = std::min<std::size_t>(r + 1, S); ev.unknown_total = snap.unknown_total; ev.action = BlockSolveSnapshot::RestartAction::lockInMicro; snap.restarts.push_back(ev);
                                                        ++snap.micro_solver_successes; const auto t1sw2 = std::chrono::steady_clock::now(); snap.micro_solver_time_ms += static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1sw2 - t0).count());
                                                        return true;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                } else { ++snap.micro_solver_dp_infeasible; }
                            }
                            // If DP did not adopt, try BnB over the ambiguous window as well
                            if (target_k2 > 0) {
                                ++snap.micro_solver_bnb_attempts;
                                Csm c_best2 = c; ConstraintState w_best2 = w; bool found2 = false; std::size_t nodes2 = 0;
                                std::size_t max_nodes2 = 800000; std::size_t max_ms2 = 800;
                                if (const char *s = std::getenv("CRSCE_MS_BNB_AMB_MAX_NODES") /* NOLINT(concurrency-mt-unsafe) */; s && *s) { const auto v = std::strtoll(s, nullptr, 10); if (v > 0) { max_nodes2 = static_cast<std::size_t>(v); } }
                                if (const char *s2 = std::getenv("CRSCE_MS_BNB_AMB_MS") /* NOLINT(concurrency-mt-unsafe) */; s2 && *s2) { const auto v = std::strtoll(s2, nullptr, 10); if (v > 0) { max_ms2 = static_cast<std::size_t>(v); } }
                                const auto t0bnb2 = std::chrono::steady_clock::now();
                                dfs_bnb_limited(0U, 0U, fb_cand, capN2, target_k2, r, lh, c_best2, w_best2, found2, nodes2, max_nodes2, t0bnb2, max_ms2, c, w, snap);
                                if (found2) {
                                    // Optional acceptance threshold (estimate delta by comparing initial unknowns to current state)
                                    std::size_t min_cells = 0;
                                    if (const char *p = std::getenv("CRSCE_MS_ACCEPT_MIN_CELLS") /* NOLINT(concurrency-mt-unsafe) */; p && *p) {
                                        const auto v = std::strtoll(p, nullptr, 10); if (v > 0) { min_cells = static_cast<std::size_t>(v); }
                                    }
                                    const std::size_t after_u = w_best2.U_row.at(r);
                                    const auto delta_cells = static_cast<std::size_t>(u_initial) - after_u;
                                    if (min_cells > 0 && delta_cells < min_cells) {
                                        ++snap.micro_solver_reject_low_benefit;
                                    } else {
                                        csm_out = c_best2; st = w_best2; baseline_csm = csm_out; baseline_st = st;
                                        BlockSolveSnapshot::RestartEvent ev{}; ev.restart_index = rs; ev.prefix_rows = std::min<std::size_t>(r + 1, S); ev.unknown_total = snap.unknown_total; ev.action = BlockSolveSnapshot::RestartAction::lockInMicro; snap.restarts.push_back(ev);
                                        ++snap.micro_solver_successes; ++snap.micro_solver_bnb_successes; snap.micro_solver_bnb_nodes += nodes2;
                                        const auto t1loc2 = std::chrono::steady_clock::now(); snap.micro_solver_time_ms += static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1loc2 - t0).count());
                                        return true;
                                    }
                                }
                                snap.micro_solver_bnb_nodes += nodes2;
                            }
                        }
                    }
                }
                // If DP still did not succeed, try small-window branch-and-bound over top cap
                // ReSharper disable once CppDFAConstantConditions
                if ((!dp_gate_ok || !dp_succeeded) && !free_cand.empty()) {
                    // Run BnB even if remaining > window, using a cap window and greedily filling beyond-cap ones later
                    ++snap.micro_solver_bnb_attempts;
                    // Depth-first BnB: choose exactly 'k_bnb' ones among the first cap candidates
                    const std::size_t cap_for_bnb = std::min<std::size_t>(free_cand.size(), window_cap);
                    const std::size_t k_bnb = std::min<std::size_t>(remaining, cap_for_bnb);
                    // Work copies seeded from the current c / w
                    Csm c_best = c;
                    ConstraintState w_best = w;
                    bool found = false;
                    std::size_t nodes = 0;
                    // Budgeted search (tunable via env)
                    std::size_t max_nodes = 1500000;
                    std::size_t max_ms = 1000;
                    if (const char *s = std::getenv("CRSCE_MS_BNB_MAX_NODES") /* NOLINT(concurrency-mt-unsafe) */; s && *s) {
                        if (const auto v = std::strtoll(s, nullptr, 10); v > 0) {
                            max_nodes = static_cast<std::size_t>(v);
                        }
                    }
                    if (const char *s2 = std::getenv("CRSCE_MS_BNB_MS") /* NOLINT(concurrency-mt-unsafe) */; s2 && *s2) {
                        if (const auto v = std::strtoll(s2, nullptr, 10); v > 0) {
                            max_ms = static_cast<std::size_t>(v);
                        }
                    }
                    const auto t0bnb = std::chrono::steady_clock::now();
                    dfs_bnb_limited(0U, 0U, free_cand, cap_for_bnb, k_bnb, r, lh,
                                    c_best, w_best, found, nodes, max_nodes, t0bnb, max_ms,
                                    c, w, snap);
                    if (found) {
                        // Optional acceptance threshold
                        std::size_t min_cells = 0;
                        if (const char *p = std::getenv("CRSCE_MS_ACCEPT_MIN_CELLS") /* NOLINT(concurrency-mt-unsafe) */; p && *p) {
                            const auto v = std::strtoll(p, nullptr, 10); if (v > 0) { min_cells = static_cast<std::size_t>(v); }
                        }
                        const std::size_t after_u = w_best.U_row.at(r);
                        const auto delta_cells = static_cast<std::size_t>(u_initial) - after_u;
                        if (min_cells > 0 && delta_cells < min_cells) {
                            ++snap.micro_solver_reject_low_benefit;
                        } else {
                            csm_out = c_best;
                            st = w_best;
                            baseline_csm = csm_out;
                            baseline_st = st;
                            BlockSolveSnapshot::RestartEvent ev{};
                            ev.restart_index = rs;
                            ev.prefix_rows = std::min<std::size_t>(r + 1, S);
                            ev.unknown_total = snap.unknown_total;
                            ev.action = BlockSolveSnapshot::RestartAction::lockInMicro;
                            snap.restarts.push_back(ev);
                            ++snap.micro_solver_successes;
                            ++snap.micro_solver_bnb_successes;
                            snap.micro_solver_bnb_nodes += nodes;
                            const auto t1 = std::chrono::steady_clock::now();
                            snap.micro_solver_time_ms += static_cast<std::size_t>(std::chrono::duration_cast<
                                std::chrono::milliseconds>(t1 - t0).count());
                            return true;
                        }
                    }
                    snap.micro_solver_bnb_nodes += nodes;
                }
                // Fallback: greedy like before
                std::size_t picked = 0; // NOLINT(misc-const-correctness)
                for (const auto &val: free_cand | std::views::values) {
                    if (picked >= remaining) {
                        break;
                    }
                    if (const std::size_t cc = val; !c.is_locked(r, cc) && local_feasible(w, r, cc, true)) {
                        assign_cell(c, w, r, cc, true);
                        ++picked;
                    }
                }
                for (const auto &val: free_cand | std::views::values) {
                    const std::size_t cc = val;
                    if (c.is_locked(r, cc)) {
                        continue;
                    }
                    if (w.U_row.at(r) == 0) {
                        break;
                    }
                    if (local_feasible(w, r, cc, false)) {
                        assign_cell(c, w, r, cc, false);
                    }
                }
                // Last-chance ambiguous fallback even when free set existed, if row still not finished
                if (w.U_row.at(r) > 0) {
                    ++snap.micro_solver_amb_fallback_attempts;
                    std::vector<std::pair<double,std::size_t>> amb; amb.reserve(S);
                    for (const auto cc_all : cols) {
                        if (c.is_locked(r, cc_all)) { continue; }
                        const double belief = c.get_data(r, cc_all);
                        const double ambv = std::fabs(belief - 0.5);
                        amb.emplace_back(ambv, cc_all);
                    }
                    if (!amb.empty()) {
                        std::ranges::sort(amb, std::less<double>{}, &std::pair<double,std::size_t>::first);
                        std::size_t amb_cap = 128; // widened ambiguity window for last-chance
                        if (const char *e = std::getenv("CRSCE_MS_AMB_FALLBACK") /* NOLINT(concurrency-mt-unsafe) */; e && *e) {
                            const auto v = std::strtoll(e, nullptr, 10); if (v > 0) { amb_cap = static_cast<std::size_t>(v); }
                        }
                        amb_cap = std::max<std::size_t>(amb_cap, 16);
                        amb_cap = std::min<std::size_t>(amb_cap, 192);
                        std::vector<std::pair<double,std::size_t>> fb_cand; fb_cand.reserve(std::min<std::size_t>(amb.size(), amb_cap));
                        for (std::size_t i0 = 0; i0 < amb.size() && i0 < amb_cap; ++i0) {
                            const auto &pr = amb[i0];
                            const double score = 1.0 - pr.first;
                            fb_cand.emplace_back(score, pr.second);
                        }
                        if (!fb_cand.empty()) {
                            const std::size_t capN2 = fb_cand.size();
                            const std::size_t target_k2 = std::min<std::size_t>(remaining, capN2);
                            if (target_k2 > 0) {
                                ++snap.micro_solver_dp_attempts;
                                static constexpr double kNegInf2 = -1.0e300;
                                std::vector<double> dp3((capN2 + 1U) * (target_k2 + 1U), kNegInf2);
                                std::vector<unsigned char> take3((capN2 + 1U) * (target_k2 + 1U), 0);
                                dp3[dp_index(0,0,target_k2)] = 0.0;
                                for (std::size_t i3 = 0; i3 < capN2; ++i3) {
                                    const double sc = fb_cand[i3].first;
                                    for (std::size_t k3 = 0; k3 <= target_k2; ++k3) {
                                        const double v0 = dp3[dp_index(i3, k3, target_k2)];
                                        dp3[dp_index(i3+1, k3, target_k2)] = std::max(v0, dp3[dp_index(i3+1, k3, target_k2)]);
                                        if (k3 + 1U <= target_k2) {
                                            const double cand = v0 + sc;
                                            if (cand > dp3[dp_index(i3+1, k3+1U, target_k2)]) {
                                                dp3[dp_index(i3+1, k3+1U, target_k2)] = cand;
                                                take3[dp_index(i3+1, k3+1U, target_k2)] = 1;
                                            }
                                        }
                                    }
                                }
                                if (dp3[dp_index(capN2, target_k2, target_k2)] > kNegInf2/2.0) {
                                    std::vector<std::size_t> chosen3; chosen3.reserve(target_k2);
                                    std::size_t i4 = capN2; std::size_t k4 = target_k2;
                                    while (i4 > 0) { if (take3[dp_index(i4, k4, target_k2)] != 0U) { chosen3.push_back(i4 - 1U); --k4; } --i4; }
                                    Csm c_best_local; ConstraintState w_best_local; bool found_local = false; std::size_t nodes_dummy = 0;
                                    if (try_apply_set(chosen3, fb_cand, capN2, c_best_local, w_best_local, found_local, nodes_dummy, c, w, r, lh, snap)) {
                                        csm_out = c_best_local; st = w_best_local; baseline_csm = csm_out; baseline_st = st;
                                        BlockSolveSnapshot::RestartEvent ev{}; ev.restart_index = rs; ev.prefix_rows = std::min<std::size_t>(r + 1, S); ev.unknown_total = snap.unknown_total; ev.action = BlockSolveSnapshot::RestartAction::lockInMicro; snap.restarts.push_back(ev);
                                        ++snap.micro_solver_successes;
                                        const auto t1loc = std::chrono::steady_clock::now(); snap.micro_solver_time_ms += static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1loc - t0).count());
                                        return true;
                                    }
                                } else { ++snap.micro_solver_dp_infeasible; }
                            }
                            if (remaining > 0) {
                                ++snap.micro_solver_bnb_attempts;
                                const std::size_t cap_for_bnb2 = fb_cand.size();
                                const std::size_t k_bnb2 = std::min<std::size_t>(remaining, cap_for_bnb2);
                                Csm c_best2 = c; ConstraintState w_best2 = w; bool found2 = false; std::size_t nodes2 = 0;
                                std::size_t max_nodes2 = 200000; std::size_t max_ms2 = 200;
                                if (const char *s = std::getenv("CRSCE_MS_BNB_MAX_NODES") /* NOLINT(concurrency-mt-unsafe) */; s && *s) { const auto v = std::strtoll(s, nullptr, 10); if (v > 0) { max_nodes2 = static_cast<std::size_t>(v); } }
                                if (const char *s2 = std::getenv("CRSCE_MS_BNB_MS") /* NOLINT(concurrency-mt-unsafe) */; s2 && *s2) { const auto v = std::strtoll(s2, nullptr, 10); if (v > 0) { max_ms2 = static_cast<std::size_t>(v); } }
                                const auto t0bnb2 = std::chrono::steady_clock::now();
                                dfs_bnb_limited(0U, 0U, fb_cand, cap_for_bnb2, k_bnb2, r, lh, c_best2, w_best2, found2, nodes2, max_nodes2, t0bnb2, max_ms2, c, w, snap);
                                if (found2) {
                                    csm_out = c_best2; st = w_best2; baseline_csm = csm_out; baseline_st = st;
                                    BlockSolveSnapshot::RestartEvent ev{}; ev.restart_index = rs; ev.prefix_rows = std::min<std::size_t>(r + 1, S); ev.unknown_total = snap.unknown_total; ev.action = BlockSolveSnapshot::RestartAction::lockInMicro; snap.restarts.push_back(ev);
                                    ++snap.micro_solver_successes; ++snap.micro_solver_bnb_successes; snap.micro_solver_bnb_nodes += nodes2;
                                    const auto t1loc = std::chrono::steady_clock::now(); snap.micro_solver_time_ms += static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1loc - t0).count());
                                    return true;
                                }
                                snap.micro_solver_bnb_nodes += nodes2;
                            }
                        }
                        // Near-lock contradiction restart (tuned threshold)
                        std::uint16_t kNearLockU2 = 12U;
                        if (const char *e = std::getenv("CRSCE_NEARLOCK_U") /* NOLINT(concurrency-mt-unsafe) */; e && *e) {
                            const auto v = std::strtoll(e, nullptr, 10); if (v > 0 && v < 64) { kNearLockU2 = static_cast<std::uint16_t>(v); }
                        }
                        if (w.U_row.at(r) > 0 && w.U_row.at(r) <= kNearLockU2) {
                            const RowHashVerifier ver_now{};
                            if (!ver_now.verify_row(c, lh, r)) {
                                csm_out = baseline_csm; st = baseline_st; BlockSolveSnapshot::RestartEvent ev{}; ev.restart_index = rs; ev.prefix_rows = r; ev.unknown_total = snap.unknown_total; ev.action = BlockSolveSnapshot::RestartAction::restartContradiction; snap.restarts.push_back(ev); ++snap.restart_contradiction_count;
                            }
                        }
                    }
                }
                if (free_cand.empty()) { ++snap.micro_solver_free_cand_empty_cases; }
            }
        }

        // Run a localized DE settle; stop if no progress
        {
            DeterministicElimination det{c, w};
            for (int it = 0; it < 8000; ++it) {
                if (const std::size_t prog = det.solve_step(); prog == 0 || w.U_row.at(r) == 0) {
                    break;
                }
            }
        }

        if (w.U_row.at(r) == 0) {
            const std::size_t check_rows = std::min<std::size_t>(r + 1, S);
            if (check_rows > 0) { ++snap.micro_solver_lh_verifications; ++snap.micro_solver_candidates; }
            if (constexpr RowHashVerifier ver_try{}; check_rows > 0 && ver_try.verify_rows(c, lh, check_rows)) {
                std::size_t min_cells = 0;
                if (const char *p = std::getenv("CRSCE_MS_ACCEPT_MIN_CELLS") /* NOLINT(concurrency-mt-unsafe) */; p && *p) {
                    const auto v = std::strtoll(p, nullptr, 10); if (v > 0) { min_cells = static_cast<std::size_t>(v); }
                }
                const auto delta_cells = static_cast<std::size_t>(u_initial);
                if (min_cells > 0 && delta_cells < min_cells) {
                    ++snap.micro_solver_reject_low_benefit;
                } else {
                csm_out = c;
                st = w;
                baseline_csm = csm_out;
                baseline_st = st;
                BlockSolveSnapshot::RestartEvent ev{};
                ev.restart_index = rs;
                ev.prefix_rows = check_rows;
                ev.unknown_total = snap.unknown_total;
                ev.action = BlockSolveSnapshot::RestartAction::lockInMicro;
                snap.restarts.push_back(ev);
                ++snap.micro_solver_successes;
                const auto t1 = std::chrono::steady_clock::now();
                snap.micro_solver_time_ms += static_cast<std::size_t>(std::chrono::duration_cast<
                    std::chrono::milliseconds>(t1 - t0).count());
                return true;
                }
            }
            else { ++snap.micro_solver_verify_failures; ++snap.verify_rows_failures; }
        }
        // If we are nearly locked on the boundary but LH still fails, trigger a contradiction restart (tuned threshold)
        {
            std::uint16_t kNearLockU = 12U;
            if (const char *e = std::getenv("CRSCE_NEARLOCK_U") /* NOLINT(concurrency-mt-unsafe) */; e && *e) {
                const auto v = std::strtoll(e, nullptr, 10); if (v > 0 && v < 64) { kNearLockU = static_cast<std::uint16_t>(v); }
            }
            if (w.U_row.at(r) > 0 && w.U_row.at(r) <= kNearLockU) {
                constexpr RowHashVerifier ver_now{};
                if (!ver_now.verify_row(c, lh, r)) {
                    csm_out = baseline_csm;
                    st = baseline_st;
                    BlockSolveSnapshot::RestartEvent ev{};
                    ev.restart_index = rs;
                    ev.prefix_rows = r;
                    ev.unknown_total = snap.unknown_total;
                    ev.action = BlockSolveSnapshot::RestartAction::restartContradiction;
                    snap.restarts.push_back(ev);
                    ++snap.restart_contradiction_count;
                }
            }
        }
        const auto t1 = std::chrono::steady_clock::now();
        snap.micro_solver_time_ms += static_cast<std::size_t>(std::chrono::duration_cast<
            std::chrono::milliseconds>(t1 - t0).count());
        return false;
    }
}
