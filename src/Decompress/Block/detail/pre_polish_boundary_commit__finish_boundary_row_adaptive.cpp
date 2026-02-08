/**
 * @file pre_polish_boundary_commit__finish_boundary_row_adaptive.cpp
 * @brief Definition of finish_boundary_row_adaptive.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/Block/detail/pre_polish_finish_boundary_row_adaptive.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Utils/detail/calc_d.h"

#include <algorithm>
#include <cstdlib> // getenv
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "common/O11y/metric_i64.h"
#include "common/O11y/counter.h"
#include <string>
#include <span>
#include <utility>
#include <vector>
#include <functional>

namespace crsce::decompress::detail {
    using crsce::decompress::RowHashVerifier;
    using crsce::decompress::DeterministicElimination;
    /**
     * @name finish_boundary_row_adaptive
     * @brief Attempt to complete a near-complete boundary row using adaptive heuristics and DE.
     * @param csm_out In/out CSM under construction.
     * @param st In/out constraint state.
     * @param lh LH digest span.
     * @param baseline_csm Out: updated baseline CSM if adoption occurs.
     * @param baseline_st Out: updated baseline state if adoption occurs.
     * @param snap In/out snapshot for metrics and events.
     * @param rs Current restart index for event attribution.
     * @param stall_ticks Number of iterations without progress to scale effort.
     * @return bool True if any adoption occurred; false otherwise.
     */
    bool finish_boundary_row_adaptive(Csm &csm_out, /* NOLINT(misc-use-internal-linkage) */
                                      ConstraintState &st,
                                      const std::span<const std::uint8_t> lh,
                                      Csm &baseline_csm,
                                      ConstraintState &baseline_st,
                                      BlockSolveSnapshot &snap,
                                      int rs,
                                      int stall_ticks) {
        constexpr std::size_t S = Csm::kS;
        ++snap.boundary_finisher_attempts;
        ::crsce::o11y::counter("boundary_finisher_attempts");
        // Use first row with unknown cells as boundary candidate (avoid full LH scan here)
        std::size_t boundary = 0;
        for (; boundary < S; ++boundary) { if (baseline_st.U_row.at(boundary) > 0) { break; } }
        if (boundary >= S) { return false; }
        ::crsce::o11y::metric(
            "boundary_finisher_invoked",
            static_cast<std::int64_t>(boundary),
            {{"U", std::to_string(static_cast<unsigned>(st.U_row.at(boundary)))}});
        const std::size_t scale = static_cast<std::size_t>(std::min(stall_ticks / 128, 4));
        const int kBoundaryMaxSteps = 640 + (static_cast<int>(scale) * 256);
        const int kBoundaryBtIters = 30000 + (static_cast<int>(scale) * 10000);
        const std::size_t kBoundaryTryCells = std::min<std::size_t>(S, 384 + (scale * 128));
        Csm c_work = baseline_csm;
        ConstraintState st_work = baseline_st;
        int steps = 0;
        bool any_adopted_ever = false;
        // Treat rows with ≤X% unknowns as near-complete (default 20%, overridable via env)
        auto near_thresh = static_cast<std::uint16_t>((S + 4U) / 5U); // ~20%
        if (const char *e = std::getenv("CRSCE_FOCUS_NEAR_THRESH") /* NOLINT(concurrency-mt-unsafe) */; e && *e) {
            int pct = std::atoi(e);
            pct = std::max(pct, 5);
            pct = std::min(pct, 70);
            near_thresh = static_cast<std::uint16_t>((static_cast<unsigned>(S) * static_cast<unsigned>(pct) + 99U) / 100U);
        }
        while (st_work.U_row.at(boundary) > 0 && st_work.U_row.at(boundary) <= near_thresh && steps < kBoundaryMaxSteps) {
            ++steps;
            bool adopted_any = false;
            std::vector<std::pair<double, std::size_t>> candidates;
            candidates.reserve(S);
            for (std::size_t c0 = 0; c0 < S; ++c0) {
                if (c_work.is_locked(boundary, c0)) { continue; }
                const double p = c_work.get_data(boundary, c0);
                const double amb = std::fabs(p - 0.5);
                candidates.emplace_back(amb, c0);
            }
            std::ranges::sort(candidates, std::less<>{}, &std::pair<double,std::size_t>::first);
            const std::size_t cap = std::min<std::size_t>(candidates.size(), kBoundaryTryCells);
            for (std::size_t idx = 0; idx < cap && !adopted_any; ++idx) {
                const std::size_t c_pick = candidates[idx].second;
                const std::uint16_t before = st_work.U_row.at(boundary);
                for (int vv = 0; vv < 2 && !adopted_any; ++vv) {
                    const bool assume_one = (vv == 1);
                    Csm c_try = c_work;
                    ConstraintState st_try = st_work;
                    c_try.put(boundary, c_pick, assume_one);
                    c_try.lock(boundary, c_pick);
                    if (st_try.U_row.at(boundary) > 0) { --st_try.U_row.at(boundary); }
                    if (st_try.U_col.at(c_pick) > 0) { --st_try.U_col.at(c_pick); }
                    const std::size_t d0 = ::crsce::decompress::detail::calc_d(boundary, c_pick);
                    const std::size_t x0 = (boundary + c_pick) % S;
                    if (st_try.U_diag.at(d0) > 0) { --st_try.U_diag.at(d0); }
                    if (st_try.U_xdiag.at(x0) > 0) { --st_try.U_xdiag.at(x0); }
                    if (assume_one) {
                        if (st_try.R_row.at(boundary) > 0) { --st_try.R_row.at(boundary); }
                        if (st_try.R_col.at(c_pick) > 0) { --st_try.R_col.at(c_pick); }
                        if (st_try.R_diag.at(d0) > 0) { --st_try.R_diag.at(d0); }
                        if (st_try.R_xdiag.at(x0) > 0) { --st_try.R_xdiag.at(x0); }
                    }
                    DeterministicElimination det_bt{c_try, st_try};
                    for (int it0 = 0; it0 < kBoundaryBtIters; ++it0) {
                        const std::size_t prog0 = det_bt.solve_step();
                        if (st_try.U_row.at(boundary) == 0) { break; }
                        if (prog0 == 0) { break; }
                    }
                    if (st_try.U_row.at(boundary) == 0 || st_try.U_row.at(boundary) < before) {
                        c_work = c_try; st_work = st_try; adopted_any = true;
                    }
                }
            }
            if (!adopted_any) { break; }
            any_adopted_ever = true;
        }
        if (st_work.U_row.at(boundary) == 0) {
            const RowHashVerifier ver_try{};
            const std::size_t check_rows = std::min<std::size_t>(boundary + 1, S);
            if (check_rows > 0 && ver_try.verify_rows(c_work, lh, check_rows)) {
                csm_out = c_work; st = st_work;
                baseline_csm = csm_out; baseline_st = st;
                BlockSolveSnapshot::RestartEvent ev{};
                ev.restart_index = rs; ev.prefix_rows = check_rows; ev.unknown_total = snap.unknown_total; ev.action = BlockSolveSnapshot::RestartAction::lockIn;
                snap.restarts.push_back(ev);
                ++snap.boundary_finisher_successes;
                ::crsce::o11y::counter("boundary_finisher_successes");
                return true;
            }
        }
        // Temporarily adopt partial improvements even without verification
        if (any_adopted_ever) {
            csm_out = c_work; st = st_work;
            ++snap.partial_adoptions;
            ::crsce::o11y::counter("partial_adoptions");
            return false;
        }
        return false;
    }
}
