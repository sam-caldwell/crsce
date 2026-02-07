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

namespace crsce::decompress::detail {
    using crsce::decompress::RowHashVerifier;
    using crsce::decompress::DeterministicElimination;

    namespace {
    // Helper to check if assigning v to (r,c) is locally feasible given residuals (R, U)
    inline bool local_feasible(const ConstraintState &st, const std::size_t r, const std::size_t c, const bool v) {
        constexpr std::size_t S = Csm::kS;
        const std::size_t d = ::crsce::decompress::detail::calc_d(r, c);
        const std::size_t x = ::crsce::decompress::detail::calc_x(r, c);
        const std::uint16_t Urow = st.U_row.at(r);
        const std::uint16_t Ucol = st.U_col.at(c);
        const std::uint16_t Udiag = st.U_diag.at(d);
        const std::uint16_t Ux = st.U_xdiag.at(x);
        const std::uint16_t Rrow = st.R_row.at(r);
        const std::uint16_t Rcol = st.R_col.at(c);
        const std::uint16_t Rdiag = st.R_diag.at(d);
        const std::uint16_t Rx = st.R_xdiag.at(x);

        // After assignment, unknowns decrease by 1 on each family; residual decreases by v.
        // Feasible iff 0 <= R' <= U' across all families.
        if (v) {
            if (Rrow == 0 || Rcol == 0 || Rdiag == 0 || Rx == 0) { return false; }
            if ((Rrow - 1) > static_cast<std::uint16_t>(Urow - 1)) { return false; }
            if ((Rcol - 1) > static_cast<std::uint16_t>(Ucol - 1)) { return false; }
            if ((Rdiag - 1) > static_cast<std::uint16_t>(Udiag - 1)) { return false; }
            if ((Rx - 1) > static_cast<std::uint16_t>(Ux - 1)) { return false; }
        } else {
            if (Rrow > static_cast<std::uint16_t>(Urow - 1)) { return false; }
            if (Rcol > static_cast<std::uint16_t>(Ucol - 1)) { return false; }
            if (Rdiag > static_cast<std::uint16_t>(Udiag - 1)) { return false; }
            if (Rx > static_cast<std::uint16_t>(Ux - 1)) { return false; }
        }
        return true;
    }
    // Optional soft prior based on chunk-local heuristics (disabled unless env opts in).
    inline double hw_prior_score(const std::size_t /*r*/, const std::size_t /*c*/) {
        // Placeholder: return 0.0 unless specific format priors are enabled.
        return 0.0;
    }
    } // anonymous namespace

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
        auto assign = [&](const std::size_t cc, const bool v){
            c.put(r, cc, v);
            c.lock(r, cc);
            if (w.U_row.at(r) > 0) { --w.U_row.at(r); }
            if (w.U_col.at(cc) > 0) { --w.U_col.at(cc); }
            const std::size_t d = (cc >= r) ? (cc - r) : (cc + S - r);
            const std::size_t x = (r + cc) % S;
            if (w.U_diag.at(d) > 0) { --w.U_diag.at(d); }
            if (w.U_xdiag.at(x) > 0) { --w.U_xdiag.at(x); }
            if (v) {
                if (w.R_row.at(r) > 0) { --w.R_row.at(r); }
                if (w.R_col.at(cc) > 0) { --w.R_col.at(cc); }
                if (w.R_diag.at(d) > 0) { --w.R_diag.at(d); }
                if (w.R_xdiag.at(x) > 0) { --w.R_xdiag.at(x); }
            }
        };

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
                if (w.R_row.at(r) == 0 && can0) { assign(cc, false); changed = true; continue; }
                if (w.R_row.at(r) == w.U_row.at(r) && can1) { assign(cc, true); changed = true; continue; }
                if (can1 && !can0) { assign(cc, true); changed = true; continue; }
                if (!can1 && can0) { assign(cc, false); changed = true; continue; }
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
            for (const auto cc : must1) { assign(cc, true); }
            // If already satisfied target, assign all others to zero where feasible
            if (remaining == 0) {
                for (const auto cc_all : cols) {
                    if (c.is_locked(r, cc_all)) { continue; }
                    if (local_feasible(w, r, cc_all, false)) { assign(cc_all, false); }
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
                    auto idx = [&](std::size_t i, std::size_t k) { return (i * (remaining + 1U)) + k; };
                    dp[idx(0,0)] = 0.0;
                    for (std::size_t i = 0; i < capN; ++i) {
                        const double sc = free_cand[i].first;
                        for (std::size_t k = 0; k <= remaining; ++k) {
                            const double v0 = dp[idx(i, k)];
                            dp[idx(i+1, k)] = std::max(v0, dp[idx(i+1, k)]);
                            if (k + 1U <= remaining) {
                                const double cand = v0 + sc;
                                if (cand > dp[idx(i+1, k+1U)]) {
                                    dp[idx(i+1, k+1U)] = cand;
                                    take[idx(i+1, k+1U)] = 1;
                                }
                            }
                        }
                    }
                    const bool dp_ok = (dp[idx(capN, remaining)] > kNegInf/2.0);
                    if (dp_ok) { ++snap.micro_solver_dp_feasible; } else { ++snap.micro_solver_dp_infeasible; }
                    if (dp_ok) {
                        // Reconstruct chosen indices
                        std::vector<std::size_t> chosen; chosen.reserve(remaining);
                        std::size_t i = capN; std::size_t k = remaining;
                        while (i > 0) {
                            if (take[idx(i, k)] != 0U) { chosen.push_back(i - 1U); --k; }
                            --i;
                        }
                        // Try k-best sets via exclusion
                        auto try_apply_set = [&](const std::vector<std::size_t> &chosen_idx)->bool{
                            ++snap.micro_solver_dp_solutions_tested;
                            Csm c_try = c; ConstraintState w_try = w;
                        // Assign chosen ones within cap
                        for (std::size_t j = 0; j < capN; ++j) {
                            const bool is_chosen = (std::ranges::find(chosen_idx, j) != chosen_idx.end());
                            const std::size_t cc = free_cand[j].second;
                            if (c_try.is_locked(r, cc)) { continue; }
                            if (is_chosen) {
                                    if (local_feasible(w_try, r, cc, true)) {
                                        const std::size_t d = (cc >= r) ? (cc - r) : (cc + S - r);
                                        const std::size_t x = (r + cc) % S;
                                        c_try.put(r, cc, true); c_try.lock(r, cc);
                                        if (w_try.U_row.at(r) > 0) { --w_try.U_row.at(r); }
                                        if (w_try.U_col.at(cc) > 0) { --w_try.U_col.at(cc); }
                                        if (w_try.U_diag.at(d) > 0) { --w_try.U_diag.at(d); }
                                        if (w_try.U_xdiag.at(x) > 0) { --w_try.U_xdiag.at(x); }
                                        if (w_try.R_row.at(r) > 0) { --w_try.R_row.at(r); }
                                        if (w_try.R_col.at(cc) > 0) { --w_try.R_col.at(cc); }
                                        if (w_try.R_diag.at(d) > 0) { --w_try.R_diag.at(d); }
                                        if (w_try.R_xdiag.at(x) > 0) { --w_try.R_xdiag.at(x); }
                                    } else { return false; }
                            }
                        }
                            // Assign zeros for non-chosen within cap where feasible
                            for (std::size_t j = 0; j < capN; ++j) {
                                const bool is_chosen = (std::ranges::find(chosen_idx, j) != chosen_idx.end());
                                if (is_chosen) { continue; }
                                const std::size_t cc = free_cand[j].second;
                                if (c_try.is_locked(r, cc)) { continue; }
                                if (w_try.U_row.at(r) == 0) { break; }
                                if (local_feasible(w_try, r, cc, false)) {
                                    c_try.put(r, cc, false); c_try.lock(r, cc);
                                    if (w_try.U_row.at(r) > 0) { --w_try.U_row.at(r); }
                                    if (w_try.U_col.at(cc) > 0) { --w_try.U_col.at(cc); }
                                    const std::size_t d = (cc >= r) ? (cc - r) : (cc + S - r);
                                    const std::size_t x = (r + cc) % S;
                                    if (w_try.U_diag.at(d) > 0) { --w_try.U_diag.at(d); }
                                    if (w_try.U_xdiag.at(x) > 0) { --w_try.U_xdiag.at(x); }
                                }
                            }
                            // Assign beyond-cap ones greedily if needed to meet remaining R_row
                            if (w_try.U_row.at(r) > 0 && w_try.R_row.at(r) > 0) {
                                for (std::size_t j = capN; j < free_cand.size() && w_try.R_row.at(r) > 0; ++j) {
                                    const std::size_t cc = free_cand[j].second;
                                    if (!c_try.is_locked(r, cc) && local_feasible(w_try, r, cc, true)) {
                                        const std::size_t d = (cc >= r) ? (cc - r) : (cc + S - r);
                                        const std::size_t x = (r + cc) % S;
                                        c_try.put(r, cc, true); c_try.lock(r, cc);
                                        if (w_try.U_row.at(r) > 0) { --w_try.U_row.at(r); }
                                        if (w_try.U_col.at(cc) > 0) { --w_try.U_col.at(cc); }
                                        if (w_try.U_diag.at(d) > 0) { --w_try.U_diag.at(d); }
                                        if (w_try.U_xdiag.at(x) > 0) { --w_try.U_xdiag.at(x); }
                                        if (w_try.R_row.at(r) > 0) { --w_try.R_row.at(r); }
                                        if (w_try.R_col.at(cc) > 0) { --w_try.R_col.at(cc); }
                                        if (w_try.R_diag.at(d) > 0) { --w_try.R_diag.at(d); }
                                        if (w_try.R_xdiag.at(x) > 0) { --w_try.R_xdiag.at(x); }
                                    }
                                }
                            }
                            // Assign any remaining beyond-cap cells to zero where feasible
                            for (std::size_t j = capN; j < free_cand.size(); ++j) {
                                const std::size_t cc = free_cand[j].second;
                                if (!c_try.is_locked(r, cc) && w_try.U_row.at(r) > 0 && local_feasible(w_try, r, cc, false)) {
                                    c_try.put(r, cc, false); c_try.lock(r, cc);
                                    if (w_try.U_row.at(r) > 0) { --w_try.U_row.at(r); }
                                    if (w_try.U_col.at(cc) > 0) { --w_try.U_col.at(cc); }
                                    const std::size_t d = (cc >= r) ? (cc - r) : (cc + S - r);
                                    const std::size_t x = (r + cc) % S;
                                    if (w_try.U_diag.at(d) > 0) { --w_try.U_diag.at(d); }
                                    if (w_try.U_xdiag.at(x) > 0) { --w_try.U_xdiag.at(x); }
                                }
                            }
                            // Local DE settle
                            DeterministicElimination det_bt{c_try, w_try};
                            for (int it = 0; it < 4000; ++it) {
                                const std::size_t prog = det_bt.solve_step();
                                if (prog == 0 || w_try.U_row.at(r) == 0) { break; }
                            }
                            if (w_try.U_row.at(r) == 0) {
                                const std::size_t check_rows = std::min<std::size_t>(r + 1, S);
                                const RowHashVerifier ver_try{};
                                if (check_rows > 0) { ++snap.micro_solver_lh_verifications; }
                                if (check_rows > 0 && ver_try.verify_rows(c_try, lh, check_rows)) {
                                    csm_out = c_try; st = w_try; baseline_csm = csm_out; baseline_st = st;
                                    BlockSolveSnapshot::RestartEvent ev{};
                                    ev.restart_index = rs; ev.prefix_rows = check_rows; ev.unknown_total = snap.unknown_total; ev.action = BlockSolveSnapshot::RestartAction::lockInMicro;
                                    snap.restarts.push_back(ev);
                                    ++snap.micro_solver_successes;
                                    const auto t1 = std::chrono::steady_clock::now();
                                    snap.micro_solver_time_ms += static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
                                    return true;
                                }
                            }
                            return false;
                        };
                        // Try best set first
                        if (try_apply_set(chosen)) { return true; }
                        // Try a few exclusions to get alternative sets
                        const std::size_t k_best = 6;
                        const std::size_t exclude_trials = std::min<std::size_t>(k_best - 1U, chosen.size());
                        for (std::size_t ex = 0; ex < exclude_trials; ++ex) {
                            // Recompute DP excluding chosen[ex]
                            const std::size_t ex_idx = chosen[ex];
                            std::vector<double> dp2((capN + 1U) * (remaining + 1U), kNegInf);
                            std::vector<unsigned char> take2((capN + 1U) * (remaining + 1U), 0);
                            dp2[idx(0,0)] = 0.0;
                            for (std::size_t i2 = 0; i2 < capN; ++i2) {
                                if (i2 == ex_idx) {
                                    for (std::size_t k2 = 0; k2 <= remaining; ++k2) {
                                        const double v0 = dp2[idx(i2, k2)];
                                        dp2[idx(i2+1, k2)] = std::max(v0, dp2[idx(i2+1, k2)]);
                                    }
                                    continue;
                                }
                                const double sc2 = free_cand[i2].first;
                                for (std::size_t k2 = 0; k2 <= remaining; ++k2) {
                                    const double v0 = dp2[idx(i2, k2)];
                                    dp2[idx(i2+1, k2)] = std::max(v0, dp2[idx(i2+1, k2)]);
                                    if (k2 + 1U <= remaining) {
                                        const double cand = v0 + sc2;
                                        if (cand > dp2[idx(i2+1, k2+1U)]) {
                                            dp2[idx(i2+1, k2+1U)] = cand;
                                            take2[idx(i2+1, k2+1U)] = 1;
                                        }
                                    }
                                }
                            }
                            if (dp2[idx(capN, remaining)] > kNegInf/2.0) {
                                std::vector<std::size_t> chosen2; chosen2.reserve(remaining);
                                std::size_t ii = capN; std::size_t kk = remaining;
                                while (ii > 0) {
                                    if (take2[idx(ii, kk)] != 0U) { chosen2.push_back(ii - 1U); --kk; }
                                    --ii;
                                }
                                if (try_apply_set(chosen2)) { return true; }
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
                        std::size_t cap_for_bnb = std::min<std::size_t>(free_cand.size(), window_cap);
                        struct Node { std::size_t idx; std::size_t ones; };
                        // Work copies seeded from current c/w
                        Csm c_best = c; ConstraintState w_best = w;
                        bool found = false;
                        std::size_t nodes = 0;
                        std::function<void(std::size_t,std::size_t,Csm&,ConstraintState&)> dfs;
                        dfs = [&](std::size_t i, std::size_t ones, Csm &c_cur, ConstraintState &w_cur) {
                            ++nodes;
                            if (found) { return; }
                            const std::size_t left = (i < cap_for_bnb) ? (cap_for_bnb - i) : 0U;
                            if (ones > remaining) { return; }
                            if (ones + left < remaining) { return; }
                            if (i >= cap_for_bnb) {
                                // Greedily add ones beyond cap if still needed
                                if (w_cur.U_row.at(r) > 0 && w_cur.R_row.at(r) > 0) {
                                    for (std::size_t j = cap_for_bnb; j < free_cand.size() && w_cur.R_row.at(r) > 0; ++j) {
                                        const std::size_t cc = free_cand[j].second;
                                        if (!c_cur.is_locked(r, cc) && local_feasible(w_cur, r, cc, true)) {
                                            const std::size_t d = (cc >= r) ? (cc - r) : (cc + S - r);
                                            const std::size_t x = (r + cc) % S;
                                            c_cur.put(r, cc, true); c_cur.lock(r, cc);
                                            if (w_cur.U_row.at(r) > 0) { --w_cur.U_row.at(r); }
                                            if (w_cur.U_col.at(cc) > 0) { --w_cur.U_col.at(cc); }
                                            if (w_cur.U_diag.at(d) > 0) { --w_cur.U_diag.at(d); }
                                            if (w_cur.U_xdiag.at(x) > 0) { --w_cur.U_xdiag.at(x); }
                                            if (w_cur.R_row.at(r) > 0) { --w_cur.R_row.at(r); }
                                            if (w_cur.R_col.at(cc) > 0) { --w_cur.R_col.at(cc); }
                                            if (w_cur.R_diag.at(d) > 0) { --w_cur.R_diag.at(d); }
                                            if (w_cur.R_xdiag.at(x) > 0) { --w_cur.R_xdiag.at(x); }
                                        }
                                    }
                                }
                                // Fill rest as zeros if feasible
                                for (std::size_t j = cap_for_bnb; j < free_cand.size(); ++j) {
                                    const std::size_t cc = free_cand[j].second;
                                    if (!c_cur.is_locked(r, cc) && w_cur.U_row.at(r) > 0 && local_feasible(w_cur, r, cc, false)) {
                                        c_cur.put(r, cc, false); c_cur.lock(r, cc);
                                        if (w_cur.U_row.at(r) > 0) { --w_cur.U_row.at(r); }
                                        if (w_cur.U_col.at(cc) > 0) { --w_cur.U_col.at(cc); }
                                        const std::size_t d = (cc >= r) ? (cc - r) : (cc + S - r);
                                        const std::size_t x = (r + cc) % S;
                                        if (w_cur.U_diag.at(d) > 0) { --w_cur.U_diag.at(d); }
                                        if (w_cur.U_xdiag.at(x) > 0) { --w_cur.U_xdiag.at(x); }
                                    }
                                }
                                // Settle locally
                                DeterministicElimination det_bt{c_cur, w_cur};
                                for (int it = 0; it < 4000; ++it) {
                                    const std::size_t prog = det_bt.solve_step();
                                    if (prog == 0 || w_cur.U_row.at(r) == 0) { break; }
                                }
                                if (w_cur.U_row.at(r) == 0) {
                                    const std::size_t check_rows = std::min<std::size_t>(r + 1, S);
                                    const RowHashVerifier ver_try{};
                                    if (check_rows > 0) { ++snap.micro_solver_lh_verifications; }
                                    if (check_rows > 0 && ver_try.verify_rows(c_cur, lh, check_rows)) {
                                        c_best = c_cur; w_best = w_cur; found = true; return;
                                    }
                                }
                                return;
                            }
                            const std::size_t cc = free_cand[i].second;
                            // Branch 1: take as one if feasible
                            if (!found && local_feasible(w_cur, r, cc, true)) {
                                Csm c1 = c_cur; ConstraintState w1 = w_cur;
                                const std::size_t d = (cc >= r) ? (cc - r) : (cc + S - r);
                                const std::size_t x = (r + cc) % S;
                                c1.put(r, cc, true); c1.lock(r, cc);
                                if (w1.U_row.at(r) > 0) { --w1.U_row.at(r); }
                                if (w1.U_col.at(cc) > 0) { --w1.U_col.at(cc); }
                                if (w1.U_diag.at(d) > 0) { --w1.U_diag.at(d); }
                                if (w1.U_xdiag.at(x) > 0) { --w1.U_xdiag.at(x); }
                                if (w1.R_row.at(r) > 0) { --w1.R_row.at(r); }
                                if (w1.R_col.at(cc) > 0) { --w1.R_col.at(cc); }
                                if (w1.R_diag.at(d) > 0) { --w1.R_diag.at(d); }
                                if (w1.R_xdiag.at(x) > 0) { --w1.R_xdiag.at(x); }
                                dfs(i + 1U, ones + 1U, c1, w1);
                            }
                            // Branch 0: take as zero if feasible
                            if (!found && local_feasible(w_cur, r, cc, false)) {
                                Csm c0 = c_cur; ConstraintState w0 = w_cur;
                                c0.put(r, cc, false); c0.lock(r, cc);
                                if (w0.U_row.at(r) > 0) { --w0.U_row.at(r); }
                                if (w0.U_col.at(cc) > 0) { --w0.U_col.at(cc); }
                                const std::size_t d = (cc >= r) ? (cc - r) : (cc + S - r);
                                const std::size_t x = (r + cc) % S;
                                if (w0.U_diag.at(d) > 0) { --w0.U_diag.at(d); }
                                if (w0.U_xdiag.at(x) > 0) { --w0.U_xdiag.at(x); }
                                dfs(i + 1U, ones, c0, w0);
                            }
                        };
                        Csm c0 = c; ConstraintState w0 = w; dfs(0U, 0U, c0, w0);
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
                    if (!c.is_locked(r, cc) && local_feasible(w, r, cc, true)) { assign(cc, true); ++picked; }
                }
                for (const auto &p : free_cand) {
                    const std::size_t cc = p.second;
                    if (c.is_locked(r, cc)) { continue; }
                    if (w.U_row.at(r) == 0) { break; }
                    if (local_feasible(w, r, cc, false)) { assign(cc, false); }
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
