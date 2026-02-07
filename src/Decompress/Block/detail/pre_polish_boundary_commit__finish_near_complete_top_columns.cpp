/**
 * @file pre_polish_boundary_commit__finish_near_complete_top_columns.cpp
 * @brief Definition of finish_near_complete_top_columns.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/Block/detail/pre_polish_finish_near_complete_top_columns.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Block/detail/commit_row_locked.h"
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Utils/detail/index_calc.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "common/O11y/metric.h"
#include <span>
#include <utility>
#include <vector>

namespace crsce::decompress::detail {
    using crsce::decompress::RowHashVerifier;
    using crsce::decompress::DeterministicElimination;

    bool finish_near_complete_top_columns(Csm &csm_out,
                                          ConstraintState &st,
                                          const std::span<const std::uint8_t> lh,
                                          Csm &baseline_csm,
                                          ConstraintState &baseline_st,
                                          BlockSolveSnapshot &snap,
                                          int rs,
                                          const std::size_t top_n_cols,
                                          const std::size_t top_k_cells) {
        constexpr std::size_t S = Csm::kS;
        // Collect candidate columns by smallest U_col
        std::vector<std::pair<std::uint16_t, std::size_t>> cols; cols.reserve(S);
        for (std::size_t c = 0; c < S; ++c) {
            const auto u = st.U_col.at(c);
            if (u > 0) { cols.emplace_back(u, c); }
        }
        if (cols.empty()) { return false; }
        std::ranges::sort(cols);
        const auto near_thresh = static_cast<std::uint16_t>((S + 4U) / 5U);
        const RowHashVerifier verifier{};
        const std::size_t N = std::min<std::size_t>(top_n_cols, cols.size());
        for (std::size_t ci = 0; ci < N; ++ci) {
            const std::size_t c = cols[ci].second;
            if (st.U_col.at(c) == 0 || st.U_col.at(c) > near_thresh) { continue; }
            // Build top-K ambiguous cells in column c
            std::vector<std::pair<double, std::size_t>> cells; cells.reserve(S);
            for (std::size_t r0 = 0; r0 < S; ++r0) {
                if (csm_out.is_locked(r0, c)) { continue; }
                const double p = csm_out.get_data(r0, c);
                const double amb = std::fabs(p - 0.5);
                cells.emplace_back(amb, r0);
            }
            if (cells.empty()) { continue; }
            std::ranges::sort(cells, [](const auto &a, const auto &b){ return a.first < b.first; });
            const std::size_t K = std::min<std::size_t>(cells.size(), top_k_cells);
            for (std::size_t i = 0; i < K; ++i) {
                const std::size_t r = cells[i].second;
                const std::uint16_t before_col = st.U_col.at(c);
                const std::uint16_t before_row = st.U_row.at(r);
                for (int vv = 0; vv < 2; ++vv) {
                    const bool assume_one = (vv == 1);
                    Csm c_try = csm_out;
                    ConstraintState st_try = st;
                    if (c_try.is_locked(r, c)) { continue; }
                    c_try.put(r, c, assume_one);
                    c_try.lock(r, c);
                    if (st_try.U_row.at(r) > 0) { --st_try.U_row.at(r); }
                    if (st_try.U_col.at(c) > 0) { --st_try.U_col.at(c); }
                    const std::size_t d0 = ::crsce::decompress::detail::calc_d(r, c);
                    const std::size_t x0 = ::crsce::decompress::detail::calc_x(r, c);
                    if (st_try.U_diag.at(d0) > 0) { --st_try.U_diag.at(d0); }
                    if (st_try.U_xdiag.at(x0) > 0) { --st_try.U_xdiag.at(x0); }
                    if (assume_one) {
                        if (st_try.R_row.at(r) > 0) { --st_try.R_row.at(r); }
                        if (st_try.R_col.at(c) > 0) { --st_try.R_col.at(c); }
                        if (st_try.R_diag.at(d0) > 0) { --st_try.R_diag.at(d0); }
                        if (st_try.R_xdiag.at(x0) > 0) { --st_try.R_xdiag.at(x0); }
                    }
                    DeterministicElimination det_bt{c_try, st_try};
                    static constexpr int kColBtIters = 12000;
                    for (int it0 = 0; it0 < kColBtIters; ++it0) {
                        const std::size_t p = det_bt.solve_step();
                        if (det_bt.solved()) { break; }
                        if (p == 0) { break; }
                    }
                    // Adopt improvements; verify and commit the affected row if digest matches
                    if (st_try.U_row.at(r) == 0 || st_try.U_row.at(r) < before_row || st_try.U_col.at(c) == 0 || st_try.U_col.at(c) < before_col) {
                        if (verifier.verify_row(c_try, lh, r)) {
                            csm_out = c_try; st = st_try;
                            commit_row_locked(csm_out, st, r);
                            baseline_csm = csm_out; baseline_st = st;
                            BlockSolveSnapshot::RestartEvent ev{};
                            ev.restart_index = rs; ev.prefix_rows = r; ev.unknown_total = snap.unknown_total; ev.action = BlockSolveSnapshot::RestartAction::lockInRow;
                            snap.restarts.push_back(ev);
                            ++snap.rows_committed;
                            ++snap.lock_in_row_count;
                            const RowHashVerifier ver_now{};
                            const std::size_t pnew = ver_now.longest_valid_prefix_up_to(csm_out, lh, S);
                            if (snap.prefix_progress.empty() || snap.prefix_progress.back().prefix_len != pnew) {
                                BlockSolveSnapshot::PrefixSample s{}; s.iter = snap.iter; s.prefix_len = pnew; snap.prefix_progress.push_back(s);
                            }
                            return true;
                        }
                        if (st_try.U_row.at(r) < before_row || st_try.U_col.at(c) < before_col) {
                            if (before_col > 0 && st_try.U_col.at(c) == 0) { ++snap.cols_finished; }
                            csm_out = c_try; st = st_try; ++snap.partial_adoptions; ::crsce::o11y::counter("partial_adoptions");
                        }
                    }
                }
            }
        }
        return false;
    }
}
