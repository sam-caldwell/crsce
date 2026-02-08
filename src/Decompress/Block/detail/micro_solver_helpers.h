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
                          BlockSolveSnapshot &snap);

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
                    BlockSolveSnapshot &snap);

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
                            BlockSolveSnapshot &snap);

inline bool finish_row_greedy(Csm &c, ConstraintState &w, std::size_t r);

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
                            BlockSolveSnapshot &snap);

} // namespace crsce::decompress::detail
