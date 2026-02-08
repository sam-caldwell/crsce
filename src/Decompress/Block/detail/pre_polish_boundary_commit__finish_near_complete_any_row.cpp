/**
 * @file pre_polish_boundary_commit__finish_near_complete_any_row.cpp
 * @brief Definition of finish_near_complete_any_row.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/Block/detail/pre_polish_finish_near_complete_any_row.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Block/detail/commit_row_locked.h"
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include "common/O11y/counter.h"
#include <span>
#include <utility>
#include <functional>
#include <vector>

namespace crsce::decompress::detail {
    using crsce::decompress::RowHashVerifier;
    using crsce::decompress::DeterministicElimination;

    /**
     * @name finish_near_complete_any_row
     * @brief Heuristic finisher for any near-complete row via localized DE and LH checks.
     * @param csm_out In/out CSM under construction.
     * @param st In/out constraint state.
     * @param lh LH digest span.
     * @param baseline_csm In/out baseline CSM; updated when adoption occurs.
     * @param baseline_st In/out baseline state; updated when adoption occurs.
     * @param snap In/out snapshot for metrics and events.
     * @param rs Current restart index for event attribution.
     * @return bool True if a near-complete row was finished and adopted.
     */
    bool finish_near_complete_any_row(Csm &csm_out,
                                      ConstraintState &st,
                                      const std::span<const std::uint8_t> lh,
                                      Csm &baseline_csm,
                                      ConstraintState &baseline_st,
                                      BlockSolveSnapshot &snap,
                                      int rs) {
        constexpr std::size_t S = Csm::kS;
        // pick row with smallest U_row > 0
        std::size_t candidate = S;
        std::uint16_t bestU = std::numeric_limits<std::uint16_t>::max();
        for (std::size_t r = 0; r < S; ++r) {
            const auto u = st.U_row.at(r);
            if (u > 0 && u < bestU) { bestU = u; candidate = r; }
        }
        if (candidate >= S) { return false; }
        // Treat rows with ≤~20% unknowns as near-complete
        const auto near_thresh = static_cast<std::uint16_t>((S + 4U) / 5U);
        if (st.U_row.at(candidate) == 0 || st.U_row.at(candidate) > near_thresh) { return false; }
        const RowHashVerifier verifier_now{};
        int steps = 0;
        static constexpr int kFocusMaxSteps = 48;
        static constexpr int kFocusBtIters = 8000;
        while (st.U_row.at(candidate) > 0 && steps < kFocusMaxSteps) {
            ++steps;
            // build top-K ambiguous cells in this row
            std::vector<std::pair<double, std::size_t>> cand_cells;
            cand_cells.reserve(S);
            for (std::size_t c0 = 0; c0 < S; ++c0) {
                if (csm_out.is_locked(candidate, c0)) { continue; }
                const double p = csm_out.get_data(candidate, c0);
                const double amb = std::fabs(p - 0.5);
                cand_cells.emplace_back(amb, c0);
            }
            if (cand_cells.empty()) { break; }
            std::ranges::sort(cand_cells, std::less<double>{}, &std::pair<double,std::size_t>::first);
            const std::size_t K = std::min<std::size_t>(cand_cells.size(), 16);
            for (std::size_t i = 0; i < K; ++i) {
                const std::size_t c_pick = cand_cells[i].second;
                const std::uint16_t before = st.U_row.at(candidate);
                for (int vv = 0; vv < 2; ++vv) {
                    const bool assume_one = (vv == 1);
                    Csm c_try = csm_out;
                    ConstraintState st_try = st;
                    if (c_try.is_locked(candidate, c_pick)) { continue; }
                    c_try.put(candidate, c_pick, assume_one);
                    c_try.lock(candidate, c_pick);
                    if (st_try.U_row.at(candidate) > 0) { --st_try.U_row.at(candidate); }
                    if (st_try.U_col.at(c_pick) > 0) { --st_try.U_col.at(c_pick); }
                    const std::size_t d0 = (c_pick >= candidate) ? (c_pick - candidate) : (c_pick + S - candidate);
                    const std::size_t x0 = (candidate + c_pick) % S;
                    if (st_try.U_diag.at(d0) > 0) { --st_try.U_diag.at(d0); }
                    if (st_try.U_xdiag.at(x0) > 0) { --st_try.U_xdiag.at(x0); }
                    if (assume_one) {
                        if (st_try.R_row.at(candidate) > 0) { --st_try.R_row.at(candidate); }
                        if (st_try.R_col.at(c_pick) > 0) { --st_try.R_col.at(c_pick); }
                        if (st_try.R_diag.at(d0) > 0) { --st_try.R_diag.at(d0); }
                        if (st_try.R_xdiag.at(x0) > 0) { --st_try.R_xdiag.at(x0); }
                    }
                    DeterministicElimination det_bt{c_try, st_try};
                    for (int it0 = 0; it0 < kFocusBtIters; ++it0) {
                        const std::size_t p = det_bt.solve_step();
                        if (det_bt.solved()) { break; }
                        if (p == 0) { break; }
                    }
                    if (st_try.U_row.at(candidate) == 0 || st_try.U_row.at(candidate) < before) {
                        if (verifier_now.verify_row(c_try, lh, candidate)) {
                            csm_out = c_try; st = st_try;
                            commit_row_locked(csm_out, st, candidate);
                            baseline_csm = csm_out; baseline_st = st;
                            BlockSolveSnapshot::RestartEvent ev{};
                            ev.restart_index = rs; ev.prefix_rows = candidate; ev.unknown_total = snap.unknown_total; ev.action = BlockSolveSnapshot::RestartAction::lockInRow;
                            snap.restarts.push_back(ev);
                            return true;
                        }
                        if (st_try.U_row.at(candidate) < before) { csm_out = c_try; st = st_try; ++snap.partial_adoptions; ::crsce::o11y::counter("partial_adoptions"); }
                    }
                }
            }
        }
        return false;
    }
}
