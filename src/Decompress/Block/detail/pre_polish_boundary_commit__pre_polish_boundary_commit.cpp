/**
 * @file pre_polish_boundary_commit__pre_polish_boundary_commit.cpp
 * @brief Definition of pre_polish_boundary_commit.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Block/detail/pre_polish_boundary_commit_fn.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Phases/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Utils/detail/calc_d.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "common/O11y/O11y.h"
#include <span>
#include <string>
#include <utility>
#include <functional>
#include <vector>

namespace crsce::decompress::detail {
    using crsce::decompress::RowHashVerifier;
    using crsce::decompress::DeterministicElimination;

    /**
     * @name pre_polish_boundary_commit
     * @brief Pre-polish phase: attempt to extend the prefix by focusing on the current boundary row.
     * @param csm_out In/out CSM under construction.
     * @param st In/out constraint state.
     * @param lh LH digest span.
     * @param seed Pseudorandom seed string (unused).
     * @param baseline_csm In/out baseline CSM; updated on adoption.
     * @param baseline_st In/out baseline state; updated on adoption.
     * @param snap In/out snapshot for metrics and events.
     * @param rs Current restart index for event attribution.
     * @return bool True if boundary row was finished and adopted.
     */
    bool pre_polish_boundary_commit(Csm &csm_out,
                                    ConstraintState &st,
                                    const std::span<const std::uint8_t> lh,
                                    const std::string &seed,
                                    Csm &baseline_csm,
                                    ConstraintState &baseline_st,
                                    BlockSolveSnapshot &snap,
                                    int rs) {
        constexpr std::size_t S = Csm::kS;
        (void)seed;
        const RowHashVerifier verifier_now;
        const std::size_t valid_now = verifier_now.longest_valid_prefix_up_to(baseline_csm, lh, S);
        if (valid_now >= S) { return false; }
        const std::size_t boundary = valid_now;
        static constexpr int kBoundaryMaxSteps = 640;
        static constexpr int kBoundaryBtIters = 30000;
        static constexpr int kBoundaryTryCells = 384;
        Csm c_work = baseline_csm;
        ConstraintState st_work = baseline_st;
        ++snap.boundary_finisher_attempts;
        ::crsce::o11y::O11y::instance().counter("boundary_finisher_attempts");
        int steps = 0;
        while (st_work.U_row.at(boundary) > 0 && steps < kBoundaryMaxSteps) {
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
            std::ranges::sort(candidates, std::less<double>{}, &std::pair<double,std::size_t>::first);
            const std::size_t cap = std::min<std::size_t>(candidates.size(), static_cast<std::size_t>(kBoundaryTryCells));
            for (std::size_t idx = 0; idx < cap && !adopted_any; ++idx) {
                const std::size_t c_pick = candidates[idx].second;
                const std::uint16_t before = st_work.U_row.at(boundary);
                for (int vv = 0; vv < 2 && !adopted_any; ++vv) {
                    const bool assume_one = (vv == 1);
                    Csm c_try = c_work;
                    ConstraintState st_try = st_work;
                    if (assume_one) { c_try.set(boundary, c_pick); } else { c_try.clear(boundary, c_pick); }
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
                    DeterministicElimination det_bt{static_cast<std::uint64_t>(12000), c_try, st_try, snap, lh};
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
        }
        if (st_work.U_row.at(boundary) == 0) {
            const RowHashVerifier ver_try;
            const std::size_t check_rows = std::min<std::size_t>(valid_now + 1, S);
            if (check_rows > 0 && ver_try.verify_rows(c_work, lh, check_rows)) {
                csm_out = c_work; st = st_work;
                std::size_t sumU2 = 0; for (const auto u : st.U_row) { sumU2 += static_cast<std::size_t>(u); }
                ::crsce::o11y::O11y::instance().metric(
                    "prefix_locked_in",
                    static_cast<std::int64_t>(check_rows),
                    {{"unknown", std::to_string(sumU2)},
                     {"rs", std::to_string(rs)},
                     {"stage", std::string("pre_polish")}});
                baseline_csm = csm_out; baseline_st = st;
            BlockSolveSnapshot::RestartEvent ev{};
            ev.restart_index = rs; ev.prefix_rows = check_rows; ev.unknown_total = sumU2; ev.action = BlockSolveSnapshot::RestartAction::lockIn;
                snap.restarts.push_back(ev);
                ++snap.boundary_finisher_successes;
                ::crsce::o11y::O11y::instance().counter("boundary_finisher_successes");
                return true;
            }
        }
        return false;
    }
}
