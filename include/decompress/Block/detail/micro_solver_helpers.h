/**
 * @file micro_solver_helpers.h
 * @brief Inline helpers for boundary-row micro-solver to avoid local constructs in .cpp.
 * @author Sam Caldwell
 */
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>
#include <chrono>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Utils/detail/index_calc.h"

namespace crsce::decompress::detail {

/**
 * @name local_feasible
 * @brief Check if assigning v to (r,c) is locally feasible given residuals (R, U).
 */
inline bool local_feasible(const ConstraintState &st, std::size_t r, std::size_t c, bool v) {
    constexpr std::size_t S = Csm::kS;
    const std::size_t d = calc_d(r, c);
    const std::size_t x = calc_x(r, c);
    const std::uint16_t Urow = st.U_row.at(r);
    const std::uint16_t Ucol = st.U_col.at(c);
    const std::uint16_t Udiag = st.U_diag.at(d);
    const std::uint16_t Ux = st.U_xdiag.at(x);
    const std::uint16_t Rrow = st.R_row.at(r);
    const std::uint16_t Rcol = st.R_col.at(c);
    const std::uint16_t Rdiag = st.R_diag.at(d);
    const std::uint16_t Rx = st.R_xdiag.at(x);
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

/**
 * @name hw_prior_score
 * @brief Optional soft prior based on chunk-local heuristics (currently disabled; returns 0.0).
 */
inline double hw_prior_score(std::size_t /*r*/, std::size_t /*c*/) {
    return 0.0;
}

/**
 * @name assign_cell
 * @brief Apply assignment v to (r,cc), updating unknown/residual counts and locking the cell.
 */
inline void assign_cell(Csm &c, ConstraintState &w, std::size_t r, std::size_t cc, bool v) {
    constexpr std::size_t S = Csm::kS;
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
}

/**
 * @name dp_index
 * @brief 2D-to-1D DP index mapping.
 */
inline std::size_t dp_index(std::size_t i, std::size_t k, std::size_t remaining) {
    return (i * (remaining + 1U)) + k;
}

/**
 * @name try_apply_set
 * @brief Try applying a DP-selected set of ones to the current row, settle locally, and LH-verify.
 */
inline bool try_apply_set(const std::vector<std::size_t> &chosen_idx,
                          const std::vector<std::pair<double,std::size_t>> &free_cand,
                          std::size_t capN,
                          Csm &c_best,
                          ConstraintState &w_best,
                          bool &found,
                          std::size_t &nodes,
                          const Csm &c,
                          const ConstraintState &w,
                          std::size_t r,
                          std::span<const std::uint8_t> lh,
                          BlockSolveSnapshot &snap) {
    ++snap.micro_solver_dp_solutions_tested;
    Csm c_try = c; ConstraintState w_try = w;
    constexpr std::size_t S = Csm::kS;
    // Assign chosen ones within cap
    for (std::size_t j = 0; j < chosen_idx.size(); ++j) {
        const std::size_t i = chosen_idx[j];
        const std::size_t cc = free_cand.at(i).second;
        if (!c_try.is_locked(r, cc) && w_try.U_row.at(r) > 0 && local_feasible(w_try, r, cc, true)) {
            assign_cell(c_try, w_try, r, cc, true);
        } else if (c_try.is_locked(r, cc)) {
            // skip
        } else {
            return false;
        }
    }
    // Assign remaining within-cap free as zeros where feasible
    for (std::size_t i = 0; i < capN; ++i) {
        bool is_chosen = false;
        for (const auto v : chosen_idx) { if (v == i) { is_chosen = true; break; } }
        if (is_chosen) { continue; }
        const std::size_t cc = free_cand.at(i).second;
        if (!c_try.is_locked(r, cc) && w_try.U_row.at(r) > 0 && local_feasible(w_try, r, cc, false)) {
            assign_cell(c_try, w_try, r, cc, false);
        }
    }
    // Assign beyond-cap ones greedily if still needed
    if (w_try.U_row.at(r) > 0 && w_try.R_row.at(r) > 0) {
        for (std::size_t j = capN; j < free_cand.size() && w_try.R_row.at(r) > 0; ++j) {
            const std::size_t cc = free_cand.at(j).second;
            if (!c_try.is_locked(r, cc) && local_feasible(w_try, r, cc, true)) {
                assign_cell(c_try, w_try, r, cc, true);
            }
        }
    }
    // Settle locally
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
            c_best = c_try; w_best = w_try; found = true; return true;
        }
    }
    (void)nodes; // nodes accounting is managed by caller
    return false;
}

/**
 * @name dfs_bnb
 * @brief Backtracking DFS over free candidates with feasibility filters.
 */
inline void dfs_bnb(std::size_t i, /* NOLINT(misc-no-recursion) */
                    std::size_t ones,
                    const std::vector<std::pair<double,std::size_t>> &free_cand,
                    const std::size_t capN,
                    const std::size_t remaining,
                    const std::size_t r,
                    const std::span<const std::uint8_t> lh,
                    Csm &c_best,
                    ConstraintState &w_best,
                    bool &found,
                    std::size_t &nodes,
                    const Csm &c_cur,
                    const ConstraintState &w_cur,
                    BlockSolveSnapshot &snap) {
    if (found) { return; }
    ++nodes;
    constexpr std::size_t S = Csm::kS;
    if (ones > remaining || (capN - i) < (remaining - ones)) { return; }
    if (i >= capN) {
        // Greedily add ones beyond cap if still needed
        Csm c2 = c_cur; ConstraintState w2 = w_cur;
        if (w2.U_row.at(r) > 0 && w2.R_row.at(r) > 0) {
            for (std::size_t j = capN; j < free_cand.size() && w2.R_row.at(r) > 0; ++j) {
                const std::size_t cc2 = free_cand.at(j).second;
                if (!c2.is_locked(r, cc2) && local_feasible(w2, r, cc2, true)) {
                    assign_cell(c2, w2, r, cc2, true);
                }
            }
        }
        // Fill rest as zeros if feasible
        for (std::size_t j = capN; j < free_cand.size(); ++j) {
            const std::size_t cc2 = free_cand.at(j).second;
            if (!c2.is_locked(r, cc2) && w2.U_row.at(r) > 0 && local_feasible(w2, r, cc2, false)) {
                assign_cell(c2, w2, r, cc2, false);
            }
        }
        // Settle locally
        DeterministicElimination det_bt{c2, w2};
        for (int it = 0; it < 4000; ++it) {
            const std::size_t prog = det_bt.solve_step();
            if (prog == 0 || w2.U_row.at(r) == 0) { break; }
        }
        if (w2.U_row.at(r) == 0) {
            const std::size_t check_rows = std::min<std::size_t>(r + 1, S);
            const RowHashVerifier ver_try{};
            if (check_rows > 0) { ++snap.micro_solver_lh_verifications; }
            if (check_rows > 0 && ver_try.verify_rows(c2, lh, check_rows)) {
                c_best = c2; w_best = w2; found = true; return;
            }
        }
        return;
    }
    const std::size_t cc = free_cand.at(i).second;
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
        dfs_bnb(i + 1U, ones + 1U, free_cand, capN, remaining, r, lh, c_best, w_best, found, nodes, c1, w1, snap);
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
        dfs_bnb(i + 1U, ones, free_cand, capN, remaining, r, lh, c_best, w_best, found, nodes, c0, w0, snap);
    }
}

/**
 * @name segment_dfs_bnb
 * @brief DFS for segment solver with node/time caps and greedy outside-cap settling.
 */
inline void segment_dfs_bnb(std::size_t i, /* NOLINT(misc-no-recursion) */
                            const std::vector<std::pair<double,std::size_t>> &cells,
                            std::size_t capN,
                            std::size_t r,
                            std::size_t valid_now,
                            std::span<const std::uint8_t> lh,
                            Csm &c_best,
                            ConstraintState &w_best,
                            bool &adopted,
                            std::size_t &nodes,
                            std::size_t max_nodes,
                            std::chrono::steady_clock::time_point t0,
                            std::size_t max_ms,
                            const Csm &c_cur,
                            const ConstraintState &w_cur,
                            BlockSolveSnapshot &snap) {
    if (adopted) { return; }
    ++nodes;
    if (nodes > max_nodes) { return; }
    const auto now = std::chrono::steady_clock::now();
    const std::size_t elapsed_ms = static_cast<std::size_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count());
    if (elapsed_ms > max_ms) { return; }
    constexpr std::size_t S = Csm::kS;
    if (i >= capN) {
        // Greedy fill ones outside if needed
        Csm c2 = c_cur; ConstraintState w2 = w_cur;
        if (w2.U_row.at(r) > 0 && w2.R_row.at(r) > 0) {
            for (std::size_t j = capN; j < cells.size() && w2.R_row.at(r) > 0; ++j) {
                const std::size_t cc = cells.at(j).second;
                if (!c2.is_locked(r, cc) && local_feasible(w2, r, cc, true)) {
                    assign_cell(c2, w2, r, cc, true);
                }
            }
        }
        for (std::size_t j = capN; j < cells.size(); ++j) {
            const std::size_t cc = cells.at(j).second;
            if (!c2.is_locked(r, cc) && w2.U_row.at(r) > 0 && local_feasible(w2, r, cc, false)) {
                assign_cell(c2, w2, r, cc, false);
            }
        }
        DeterministicElimination det_bt{c2, w2};
        for (int it = 0; it < 4000; ++it) {
            const std::size_t prog = det_bt.solve_step();
            if (prog == 0 || w2.U_row.at(r) == 0) { break; }
        }
        if (w2.U_row.at(r) == 0) {
            const std::size_t check_rows = std::min<std::size_t>(valid_now + 1, S);
            const RowHashVerifier ver{};
            if (check_rows > 0) { ++snap.micro_solver_lh_verifications; }
            if (check_rows > 0 && ver.verify_rows(c2, lh, check_rows)) {
                c_best = c2; w_best = w2; adopted = true; return;
            }
        }
        return;
    }
    const std::size_t cc = cells.at(i).second;
    // Branch 1
    if (!adopted && local_feasible(w_cur, r, cc, true)) {
        Csm c1 = c_cur; ConstraintState w1 = w_cur;
        assign_cell(c1, w1, r, cc, true);
        segment_dfs_bnb(i + 1U, cells, capN, r, valid_now, lh, c_best, w_best, adopted, nodes, max_nodes, t0, max_ms, c1, w1, snap);
    }
    // Branch 0
    if (!adopted && local_feasible(w_cur, r, cc, false)) {
        Csm c0 = c_cur; ConstraintState w0 = w_cur;
        assign_cell(c0, w0, r, cc, false);
        segment_dfs_bnb(i + 1U, cells, capN, r, valid_now, lh, c_best, w_best, adopted, nodes, max_nodes, t0, max_ms, c0, w0, snap);
    }
}

/**
 * @name finish_row_greedy
 * @brief Greedy local completion of a single row using feasibility filters and assigns.
 */
inline bool finish_row_greedy(Csm &c, ConstraintState &w, std::size_t r) {
    constexpr std::size_t S = Csm::kS;
    if (w.U_row.at(r) == 0) { return true; }
    std::vector<std::size_t> cols; cols.reserve(S);
    for (std::size_t c0 = 0; c0 < S; ++c0) { if (!c.is_locked(r, c0)) { cols.push_back(c0); } }
    if (cols.empty()) { return false; }
    const std::uint16_t target = w.R_row.at(r);
    std::vector<std::size_t> must1; must1.reserve(cols.size());
    std::vector<std::size_t> must0; must0.reserve(cols.size());
    std::vector<std::pair<double, std::size_t>> free_cand; free_cand.reserve(cols.size());
    for (const auto cc : cols) {
        const bool can1 = local_feasible(w, r, cc, true);
        const bool can0 = local_feasible(w, r, cc, false);
        if (!can1 && !can0) { return false; }
        if (can1 && !can0) { must1.push_back(cc); continue; }
        if (!can1 && can0) { must0.push_back(cc); continue; }
        const double belief = c.get_data(r, cc);
        const double pc = static_cast<double>(w.R_col.at(cc)) / static_cast<double>(std::max<std::uint16_t>(1, w.U_col.at(cc)));
        const std::size_t d = calc_d(r, cc);
        const std::size_t x = calc_x(r, cc);
        const double pd = static_cast<double>(w.R_diag.at(d)) / static_cast<double>(std::max<std::uint16_t>(1, w.U_diag.at(d)));
        const double px = static_cast<double>(w.R_xdiag.at(x)) / static_cast<double>(std::max<std::uint16_t>(1, w.U_xdiag.at(x)));
        const double score = (0.5 * belief) + (0.5 * ((0.5 * pc) + (0.25 * pd) + (0.25 * px)));
        free_cand.emplace_back(score, cc);
    }
    if (must1.size() > static_cast<std::size_t>(target)) { return false; }
    const std::size_t remaining = static_cast<std::size_t>(target) - must1.size();
    for (const auto cc : must1) { assign_cell(c, w, r, cc, true); }
    for (const auto cc : must0) { assign_cell(c, w, r, cc, false); }
    if (remaining == 0) {
        for (const auto &p : free_cand) { const std::size_t cc = p.second; if (!c.is_locked(r, cc) && local_feasible(w, r, cc, false)) { assign_cell(c, w, r, cc, false); } }
        return true;
    }
    std::ranges::sort(free_cand, std::greater<double>{}, &std::pair<double,std::size_t>::first);
    std::size_t picked = 0;
    for (const auto &p : free_cand) { if (picked >= remaining) { break; } const std::size_t cc = p.second; if (!c.is_locked(r, cc) && local_feasible(w, r, cc, true)) { assign_cell(c, w, r, cc, true); ++picked; } }
    for (const auto &p : free_cand) { const std::size_t cc = p.second; if (w.U_row.at(r) == 0) { break; } if (!c.is_locked(r, cc) && local_feasible(w, r, cc, false)) { assign_cell(c, w, r, cc, false); } }
    return true;
}

/**
 * @name two_row_dfs_bnb
 * @brief DFS across top-K candidates of row r0 to place exactly remaining0 ones, then greedy-complete r1.
 */
inline void two_row_dfs_bnb(std::size_t i, /* NOLINT(misc-no-recursion) */
                            std::size_t ones,
                            const std::vector<std::pair<double,std::size_t>> &free_cand,
                            std::size_t capN,
                            std::size_t remaining0,
                            std::size_t r0,
                            std::size_t r1,
                            std::span<const std::uint8_t> lh,
                            Csm &c_out,
                            ConstraintState &w_out,
                            bool &found,
                            std::size_t &nodes,
                            std::size_t max_nodes,
                            std::chrono::steady_clock::time_point t0,
                            std::size_t max_ms,
                            const Csm &c_cur,
                            const ConstraintState &w_cur,
                            BlockSolveSnapshot &snap) {
    if (found) { return; }
    ++nodes;
    if (nodes > max_nodes) { return; }
    const auto now = std::chrono::steady_clock::now();
    const std::size_t elapsed_ms = static_cast<std::size_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count());
    if (elapsed_ms > max_ms) { return; }
    constexpr std::size_t S = Csm::kS;
    const std::size_t left = (i < capN) ? (capN - i) : 0U;
    if (ones > remaining0) { return; }
    if (ones + left < remaining0) { return; }
    if (i >= capN) {
        Csm c_try = c_cur; ConstraintState w_try = w_cur;
        if (finish_row_greedy(c_try, w_try, r1)) {
            const std::size_t check_rows = std::min<std::size_t>(r0 + 2U, S);
            const RowHashVerifier ver2{};
            if (check_rows > 0) { ++snap.micro_solver_lh_verifications; }
            if (check_rows > 0 && ver2.verify_rows(c_try, lh, check_rows)) {
                c_out = c_try; w_out = w_try; found = true; return;
            }
        }
        return;
    }
    const std::size_t cc = free_cand.at(i).second;
    if (!found && local_feasible(w_cur, r0, cc, true)) {
        Csm c1 = c_cur; ConstraintState w1 = w_cur;
        assign_cell(c1, w1, r0, cc, true);
        two_row_dfs_bnb(i + 1U, ones + 1U, free_cand, capN, remaining0, r0, r1, lh, c_out, w_out, found, nodes, max_nodes, t0, max_ms, c1, w1, snap);
    }
    if (!found && local_feasible(w_cur, r0, cc, false)) {
        Csm c0 = c_cur; ConstraintState w0 = w_cur;
        assign_cell(c0, w0, r0, cc, false);
        two_row_dfs_bnb(i + 1U, ones, free_cand, capN, remaining0, r0, r1, lh, c_out, w_out, found, nodes, max_nodes, t0, max_ms, c0, w0, snap);
    }
}

} // namespace crsce::decompress::detail
