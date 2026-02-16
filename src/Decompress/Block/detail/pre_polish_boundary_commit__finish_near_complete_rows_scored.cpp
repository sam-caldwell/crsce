/**
 * @file pre_polish_boundary_commit__finish_near_complete_rows_scored.cpp
 * @brief Definition of finish_near_complete_rows_scored.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/Block/detail/pre_polish_finish_near_complete_rows_scored.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Phases/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Block/detail/commit_row_locked.h"
#include "decompress/Utils/detail/calc_x.h"
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "common/O11y/counter.h"
#include <span>
#include <utility>
#include <functional>
#include <vector>

namespace crsce::decompress::detail {
    using crsce::decompress::RowHashVerifier;
    using crsce::decompress::DeterministicElimination;

    /**
     * @name finish_near_complete_rows_scored
     * @brief Attempt completion of near-complete rows using top-K ambiguous cells and DE.
     * @param csm_out In/out CSM under construction.
     * @param st In/out constraint state.
     * @param lh LH digest span.
     * @param baseline_csm In/out baseline CSM; updated on adoption.
     * @param baseline_st In/out baseline state; updated on adoption.
     * @param snap In/out snapshot for metrics and events.
     * @param rs Current restart index for event attribution.
     * @param rows Candidate row indices to consider.
     * @param top_k_cells Number of cells to try per row.
     * @return bool True if a row completion was adopted; false otherwise.
     */
    bool finish_near_complete_rows_scored(Csm &csm_out,
                                          ConstraintState &st,
                                          const std::span<const std::uint8_t> lh,
                                          Csm &baseline_csm,
                                          ConstraintState &baseline_st,
                                          BlockSolveSnapshot &snap,
                                          int rs,
                                          const std::vector<std::size_t> &rows,
                                          const std::size_t top_k_cells) {
        constexpr std::size_t S = Csm::kS;
        const auto near_thresh = static_cast<std::uint16_t>((S + 4U) / 5U); // ~20%
        const RowHashVerifier verifier{};
        for (const auto r : rows) {
            if (r >= S) { continue; }
            const auto u = st.U_row.at(r);
            if (u == 0 || u > near_thresh) { continue; }
            std::vector<std::pair<double, std::size_t>> cells;
            cells.reserve(S);
            for (std::size_t c0 = 0; c0 < S; ++c0) {
                if (csm_out.is_locked(r, c0)) { continue; }
                const double p = csm_out.get_data(r, c0);
                const double amb = std::fabs(p - 0.5);
                cells.emplace_back(amb, c0);
            }
            if (cells.empty()) { continue; }
            std::ranges::sort(cells, std::less<double>{}, &std::pair<double,std::size_t>::first);
            const std::size_t K = std::min<std::size_t>(cells.size(), top_k_cells);
            for (std::size_t i = 0; i < K; ++i) {
                const std::size_t c_pick = cells[i].second;
                const std::uint16_t before = st.U_row.at(r);
                for (int vv = 0; vv < 2; ++vv) {
                    const bool assume_one = (vv == 1);
                    Csm c_try = csm_out;
                    ConstraintState st_try = st;
                    if (c_try.is_locked(r, c_pick)) { continue; }
                    if (assume_one) { c_try.set(r, c_pick); } else { c_try.clear(r, c_pick); }
                    c_try.lock(r, c_pick);
                    if (st_try.U_row.at(r) > 0) { --st_try.U_row.at(r); }
                    if (st_try.U_col.at(c_pick) > 0) { --st_try.U_col.at(c_pick); }
                    const std::size_t d0 = (c_pick >= r) ? (c_pick - r) : (c_pick + S - r);
                        const std::size_t x0 = ::crsce::decompress::detail::calc_x(r, c_pick);
                    if (st_try.U_diag.at(d0) > 0) { --st_try.U_diag.at(d0); }
                    if (st_try.U_xdiag.at(x0) > 0) { --st_try.U_xdiag.at(x0); }
                    if (assume_one) {
                        if (st_try.R_row.at(r) > 0) { --st_try.R_row.at(r); }
                        if (st_try.R_col.at(c_pick) > 0) { --st_try.R_col.at(c_pick); }
                        if (st_try.R_diag.at(d0) > 0) { --st_try.R_diag.at(d0); }
                        if (st_try.R_xdiag.at(x0) > 0) { --st_try.R_xdiag.at(x0); }
                    }
                    DeterministicElimination det_bt{static_cast<std::uint64_t>(12000), c_try, st_try, snap, lh};
                    static constexpr int kFocusBtIters = 12000;
                    for (int it0 = 0; it0 < kFocusBtIters; ++it0) {
                        const std::size_t p = det_bt.solve_step();
                        if (det_bt.solved()) { break; }
                        if (p == 0) { break; }
                    }
                    if (st_try.U_row.at(r) == 0 || st_try.U_row.at(r) < before) {
                        if (verifier.verify_row(c_try, lh, r)) {
                            csm_out = c_try; st = st_try;
                            commit_row_locked(csm_out, st, r);
                            baseline_csm = csm_out; baseline_st = st;
                            BlockSolveSnapshot::RestartEvent ev{};
                            ev.restart_index = rs; ev.prefix_rows = r; ev.unknown_total = snap.unknown_total; ev.action = BlockSolveSnapshot::RestartAction::lockInRow;
                            snap.restarts.push_back(ev);
                            return true;
                        }
                        if (st_try.U_row.at(r) < before) { csm_out = c_try; st = st_try; ++snap.partial_adoptions; ::crsce::o11y::counter("partial_adoptions"); }
                    }
                }
            }
        }
        return false;
    }
}
