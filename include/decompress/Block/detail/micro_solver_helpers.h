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
        }
    }
    // Assign remaining free as zeros where feasible
    for (std::size_t i = 0; i < capN; ++i) {
        const std::size_t cc = free_cand.at(i).second;
        if (!c_try.is_locked(r, cc) && w_try.U_row.at(r) > 0 && local_feasible(w_try, r, cc, false)) {
            assign_cell(c_try, w_try, r, cc, false);
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
inline void dfs_bnb(std::size_t i,
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
    if (i == capN) {
        // Fill remaining as zeros and settle
        Csm c2 = c_cur; ConstraintState w2 = w_cur;
        for (std::size_t j = 0; j < capN; ++j) {
            const std::size_t cc = free_cand.at(j).second;
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
            const std::size_t check_rows = std::min<std::size_t>(r + 1, S);
            const RowHashVerifier ver_try{};
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
        dfs_bnb(i + 1U, ones, free_cand, capN, remaining, r, lh, c0, w0, found, nodes, c0, w0, snap);
    }
}

} // namespace crsce::decompress::detail

