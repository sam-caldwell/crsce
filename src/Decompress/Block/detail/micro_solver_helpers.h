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
#include "decompress/Utils/detail/calc_d.h"
#include "decompress/Utils/detail/calc_x.h"

namespace crsce::decompress::detail {

inline bool finish_row_greedy(Csm &c, ConstraintState &w, std::size_t r);

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

inline double hw_prior_score(std::size_t /*r*/, std::size_t /*c*/) { return 0.0; }

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

inline std::size_t dp_index(std::size_t i, std::size_t k, std::size_t remaining) {
    return (i * (remaining + 1U)) + k;
}

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
                          BlockSolveSnapshot & /*snap*/) {
    ++nodes;
    found = false;
    Csm c_try = c;
    ConstraintState w_try = w;
    for (const std::size_t idx : chosen_idx) {
        if (idx >= capN) { continue; }
        const std::size_t cc = free_cand[idx].second;
        if (c_try.is_locked(r, cc)) { continue; }
        if (!local_feasible(w_try, r, cc, true)) { return false; }
        assign_cell(c_try, w_try, r, cc, true);
    }
    (void)finish_row_greedy(c_try, w_try, r);
    if (w_try.U_row.at(r) == 0) {
        const RowHashVerifier ver{};
        const std::size_t check_rows = std::min<std::size_t>(r + 1U, Csm::kS);
        if (check_rows > 0 && ver.verify_rows(c_try, lh, check_rows)) {
            c_best = c_try;
            w_best = w_try;
            found = true;
            return true;
        }
    }
    return false;
}

inline void dfs_bnb(std::size_t i, /* NOLINT(misc-no-recursion) */
                    std::size_t ones,
                    const std::vector<std::pair<double,std::size_t>> &free_cand,
                    std::size_t capN,
                    std::size_t target_k,
                    std::size_t r,
                    std::span<const std::uint8_t> lh,
                    Csm &c_best,
                    ConstraintState &w_best,
                    bool &found,
                    std::size_t &nodes,
                    const Csm &c_cur,
                    const ConstraintState &w_cur,
                    BlockSolveSnapshot & snap) {
    if (found) { return; }
    if (ones > target_k) { return; }
    if (i >= capN) {
        Csm c_try = c_cur;
        ConstraintState w_try = w_cur;
        (void)finish_row_greedy(c_try, w_try, r);
        if (w_try.U_row.at(r) == 0) {
            const RowHashVerifier ver{};
            const std::size_t check_rows = std::min<std::size_t>(r + 1U, Csm::kS);
            if (check_rows > 0 && ver.verify_rows(c_try, lh, check_rows)) {
                c_best = c_try;
                w_best = w_try;
                found = true;
            }
        }
        return;
    }
    if ((ones + (capN - i)) < target_k) { return; }
    ++nodes;
    const std::size_t cc = free_cand[i].second;
    if (!c_cur.is_locked(r, cc) && local_feasible(w_cur, r, cc, true)) {
        Csm c_next = c_cur;
        ConstraintState w_next = w_cur;
        assign_cell(c_next, w_next, r, cc, true);
        dfs_bnb(i + 1U, ones + 1U, free_cand, capN, target_k, r, lh,
                c_best, w_best, found, nodes, c_next, w_next, snap);
        if (found) { return; }
    }
    dfs_bnb(i + 1U, ones, free_cand, capN, target_k, r, lh,
            c_best, w_best, found, nodes, c_cur, w_cur, snap);
}

inline void dfs_bnb_limited(std::size_t i, /* NOLINT(misc-no-recursion) */
                            std::size_t ones,
                            const std::vector<std::pair<double,std::size_t>> &free_cand,
                            std::size_t capN,
                            std::size_t target_k,
                            std::size_t r,
                            std::span<const std::uint8_t> lh,
                            Csm &c_best,
                            ConstraintState &w_best,
                            bool &found,
                            std::size_t &nodes,
                            std::size_t max_nodes,
                            std::chrono::steady_clock::time_point t0,
                            std::size_t max_ms,
                            const Csm &c_cur,
                            const ConstraintState &w_cur,
                            BlockSolveSnapshot & snap) {
    if (found) { return; }
    if (max_nodes > 0 && nodes >= max_nodes) { return; }
    if (max_ms > 0) {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count();
        const auto max_ms_signed = static_cast<std::int64_t>(max_ms);
        if (elapsed_ms >= max_ms_signed) { return; }
    }
    if (ones > target_k) { return; }
    if (i >= capN) {
        Csm c_try = c_cur;
        ConstraintState w_try = w_cur;
        (void)finish_row_greedy(c_try, w_try, r);
        if (w_try.U_row.at(r) == 0) {
            const RowHashVerifier ver{};
            const std::size_t check_rows = std::min<std::size_t>(r + 1U, Csm::kS);
            if (check_rows > 0 && ver.verify_rows(c_try, lh, check_rows)) {
                c_best = c_try;
                w_best = w_try;
                found = true;
            }
        }
        return;
    }
    if ((ones + (capN - i)) < target_k) { return; }
    ++nodes;
    if (max_nodes > 0 && nodes >= max_nodes) { return; }
    const std::size_t cc = free_cand[i].second;
    if (!c_cur.is_locked(r, cc) && local_feasible(w_cur, r, cc, true)) {
        Csm c_next = c_cur; ConstraintState w_next = w_cur;
        assign_cell(c_next, w_next, r, cc, true);
        dfs_bnb_limited(i + 1U, ones + 1U, free_cand, capN, target_k, r, lh,
                        c_best, w_best, found, nodes, max_nodes, t0, max_ms,
                        c_next, w_next, snap);
        if (found) { return; }
    }
    dfs_bnb_limited(i + 1U, ones, free_cand, capN, target_k, r, lh,
                    c_best, w_best, found, nodes, max_nodes, t0, max_ms,
                    c_cur, w_cur, snap);
}

inline void segment_dfs_bnb(std::size_t i, /* NOLINT(misc-no-recursion) */
                            const std::vector<std::pair<double,std::size_t>> &cells,
                            std::size_t capN,
                            std::size_t r,
                            std::size_t /*valid_now*/,
                            std::span<const std::uint8_t> lh,
                            Csm &c_best,
                            ConstraintState &w_best,
                            bool &adopted,
                            std::size_t &nodes,
                            std::size_t /*max_nodes*/,
                            std::chrono::steady_clock::time_point /*t0*/,
                            std::size_t /*max_ms*/,
                            const Csm &c_cur,
                            const ConstraintState &w_cur,
                            BlockSolveSnapshot & snap) {
    if (adopted) { return; }
    if (i >= capN) {
        Csm c_try = c_cur;
        ConstraintState w_try = w_cur;
        (void)finish_row_greedy(c_try, w_try, r);
        if (w_try.U_row.at(r) == 0) {
            const RowHashVerifier ver{};
            const std::size_t check_rows = std::min<std::size_t>(r + 1U, Csm::kS);
            if (check_rows > 0 && ver.verify_rows(c_try, lh, check_rows)) {
                c_best = c_try;
                w_best = w_try;
                adopted = true;
            }
        }
        return;
    }
    ++nodes;
    const std::size_t cc = cells[i].second;
    if (!c_cur.is_locked(r, cc) && local_feasible(w_cur, r, cc, true)) {
        Csm c_next = c_cur; ConstraintState w_next = w_cur;
        assign_cell(c_next, w_next, r, cc, true);
        segment_dfs_bnb(i + 1U, cells, capN, r, 0U, lh,
                        c_best, w_best, adopted, nodes, 0U, std::chrono::steady_clock::now(), 0U,
                        c_next, w_next, snap);
        if (adopted) { return; }
    }
    segment_dfs_bnb(i + 1U, cells, capN, r, 0U, lh,
                    c_best, w_best, adopted, nodes, 0U, std::chrono::steady_clock::now(), 0U,
                    c_cur, w_cur, snap);
}

inline bool finish_row_greedy(Csm &c, ConstraintState &w, std::size_t r) {
    constexpr std::size_t S = Csm::kS;
    bool changed = true;
    int guard = 0;
    while (changed && guard++ < 4096 && w.U_row.at(r) > 0) {
        changed = false;
        for (std::size_t cc = 0; cc < S && w.U_row.at(r) > 0; ++cc) {
            if (c.is_locked(r, cc)) { continue; }
            const bool can1 = local_feasible(w, r, cc, true);
            const bool can0 = local_feasible(w, r, cc, false);
            if (!can1 && !can0) { return false; }
            if (w.R_row.at(r) == 0 && can0) { assign_cell(c, w, r, cc, false); changed = true; continue; }
            if (w.R_row.at(r) == w.U_row.at(r) && can1) { assign_cell(c, w, r, cc, true); changed = true; continue; }
            if (can1 && !can0) { assign_cell(c, w, r, cc, true); changed = true; continue; }
            if (!can1 && can0) { assign_cell(c, w, r, cc, false); changed = true; continue; }
        }
    }
    return (w.U_row.at(r) == 0);
}

inline void two_row_dfs_bnb(std::size_t /*i*/, /* NOLINT(misc-no-recursion) */
                            std::size_t /*ones*/,
                            const std::vector<std::pair<double,std::size_t>> & /*free_cand*/,
                            std::size_t /*capN*/,
                            std::size_t /*remaining0*/,
                            std::size_t r0,
                            std::size_t r1,
                            std::span<const std::uint8_t> lh,
                            Csm &c_out,
                            ConstraintState &w_out,
                            bool &found,
                            std::size_t &nodes,
                            std::size_t /*max_nodes*/,
                            std::chrono::steady_clock::time_point /*t0*/,
                            std::size_t /*max_ms*/,
                            const Csm &c_cur,
                            const ConstraintState &w_cur,
                            BlockSolveSnapshot & /*snap*/) {
    ++nodes;
    Csm c_try = c_cur;
    ConstraintState w_try = w_cur;
    (void)finish_row_greedy(c_try, w_try, r0);
    (void)finish_row_greedy(c_try, w_try, r1);
    const RowHashVerifier ver{};
    const std::size_t check_rows = std::min<std::size_t>(r1 + 1U, Csm::kS);
    if (w_try.U_row.at(r0) == 0 && w_try.U_row.at(r1) == 0 && ver.verify_rows(c_try, lh, check_rows)) {
        c_out = c_try;
        w_out = w_try;
        found = true;
    }
}

} // namespace crsce::decompress::detail
