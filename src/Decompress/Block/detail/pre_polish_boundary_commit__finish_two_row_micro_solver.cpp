/**
 * @file pre_polish_boundary_commit__finish_two_row_micro_solver.cpp
 * @brief Definition of finish_two_row_micro_solver.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Block/detail/BlockSolveSnapshot.h" // already included below

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>
#include <chrono>
#include <functional>
#include <cstdlib>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Utils/detail/calc_d.h"
#include "decompress/Utils/detail/calc_x.h"
#include "micro_solver_helpers.h"

namespace crsce::decompress::detail {
    using crsce::decompress::RowHashVerifier;

    // Helpers are provided by micro_solver_helpers.h to avoid local constructs here.

    /**
     * @name finish_two_row_micro_solver
     * @brief Micro-solver that attempts to finish two consecutive boundary rows with a capped DFS.
     * @param csm_out In/out CSM under construction.
     * @param st In/out constraint state.
     * @param lh LH digest span.
     * @param baseline_csm In/out baseline CSM; updated on adoption.
     * @param baseline_st In/out baseline state; updated on adoption.
     * @param snap In/out snapshot for metrics and events.
     * @param rs Current restart index for event attribution.
     * @return bool True if two-row completion path is adopted; false otherwise.
     */
    bool finish_two_row_micro_solver(Csm &csm_out, // NOLINT(misc-use-internal-linkage)
                                     ConstraintState &st,
                                     std::span<const std::uint8_t> lh,
                                     Csm &baseline_csm,
                                     ConstraintState &baseline_st,
                                     BlockSolveSnapshot &snap,
                                     int rs) {

        const auto t0 = std::chrono::steady_clock::now();
        ++snap.micro_solver_attempts; ++snap.micro_solver2_attempts;
        constexpr std::size_t S = Csm::kS;
        // Boundary rows: first two rows with unknowns starting at top
        std::size_t r0 = 0; for (; r0 < S; ++r0) {
            if (baseline_st.U_row.at(r0) > 0) {
                break;
            }
        }
        if (r0 + 1U >= S) {
            return false;
        }
        const std::size_t r1 = r0 + 1U;
        Csm c = baseline_csm; ConstraintState w = baseline_st;
        if (w.U_row.at(r0) == 0 || w.U_row.at(r1) == 0) {
            return false;
        }
        // Gate by near-threshold unknowns for both rows (default ~70%)
        auto near_thresh = static_cast<std::uint16_t>(((S * 7U) + 9U) / 10U);
        if (const char *e = std::getenv("CRSCE_FOCUS_NEAR_THRESH") /* NOLINT(concurrency-mt-unsafe) */; e && *e) {
            int pct = std::atoi(e);
            pct = std::max(pct, 5);
            pct = std::min(pct, 70);
            near_thresh = static_cast<std::uint16_t>((static_cast<unsigned>(S) * static_cast<unsigned>(pct) + 99U) / 100U);
        }
        if (w.U_row.at(r0) > near_thresh || w.U_row.at(r1) > near_thresh) {
            return false;
        }
        // First try greedy fill for both rows; this is cheap.
        if (!finish_row_greedy(c, w, r0)) {
            return false;
        }
        if (!finish_row_greedy(c, w, r1)) {
            // Greedy failed on r1; attempt a small-window BnB on r0, then retry greedy r1.
            // Window from env: CRSCE_MS2_WINDOW (default 48; clamp [24, 96])
            std::size_t window2 = 48;
            if (const char *w2 = std::getenv("CRSCE_MS2_WINDOW") /* NOLINT(concurrency-mt-unsafe) */; w2 && *w2) {
                const int v = std::atoi(w2);
                if (v > 0) {
                    window2 = static_cast<std::size_t>(v);
                }
                window2 = std::max<std::size_t>(24, std::min<std::size_t>(window2, 96));
            }
            if (w.U_row.at(r0) > window2 || w.R_row.at(r0) > window2) {
                return false;
            }
            // Collect free columns for r0
            std::vector<std::pair<double,std::size_t>> free_cand; free_cand.reserve(S);
            {
                const std::uint16_t target0 = w.R_row.at(r0);
                std::size_t remaining0 = target0;
                std::vector<std::size_t> cols0; cols0.reserve(S);
                for (std::size_t c0 = 0; c0 < S; ++c0) {
                    if (!c.is_locked(r0, c0)) {
                        cols0.push_back(c0);
                    }
                }
                for (const auto cc : cols0) {
                    const bool can1 = local_feasible(w, r0, cc, true);
                    const bool can0 = local_feasible(w, r0, cc, false);
                    if (!can1 && !can0) {
                        return false;
                    }
                    if (can1 && !can0) {
                        assign_cell(c, w, r0, cc, true);
                        if (remaining0>0) {
                            --remaining0;
                        }
                        continue;
                    }
                    if (!can1 && can0) {
                        assign_cell(c, w, r0, cc, false);
                        continue;
                    }
                    const double belief = c.get_data(r0, cc);
                    const double pc = static_cast<double>(w.R_col.at(cc)) / static_cast<double>(std::max<std::uint16_t>(1, w.U_col.at(cc)));
                    const std::size_t d = ::crsce::decompress::detail::calc_d(r0, cc);
                    const std::size_t x = ::crsce::decompress::detail::calc_x(r0, cc);
                    const double pd = static_cast<double>(w.R_diag.at(d)) / static_cast<double>(std::max<std::uint16_t>(1, w.U_diag.at(d)));
                    const double px = static_cast<double>(w.R_xdiag.at(x)) / static_cast<double>(std::max<std::uint16_t>(1, w.U_xdiag.at(x)));
                    const double score = (0.5 * belief) + (0.5 * ((0.5 * pc) + (0.25 * pd) + (0.25 * px)));
                    free_cand.emplace_back(score, cc);
                }
                if (remaining0 > 0 && !free_cand.empty()) {
                    std::ranges::sort(free_cand, std::greater<double>{}, &std::pair<double,std::size_t>::first);
                    const std::size_t capN = std::min<std::size_t>(free_cand.size(), window2);
                    if (remaining0 <= capN) {
                        ++snap.micro_solver_bnb_attempts;
                        // Safety caps for DFS
                        std::size_t max_nodes = 200000;
                        if (const char *p = std::getenv("CRSCE_MS2_MAX_NODES") /* NOLINT(concurrency-mt-unsafe) */; p && *p) {
                            const std::int64_t v = std::strtoll(p, nullptr, 10);
                            if (v > 0) {
                                max_nodes = static_cast<std::size_t>(v);
                            }
                        }
                        std::size_t max_ms = 5000;
                        if (const char *p = std::getenv("CRSCE_MS2_MAX_MS") /* NOLINT(concurrency-mt-unsafe) */; p && *p) {
                            const std::int64_t v = std::strtoll(p, nullptr, 10);
                            if (v > 0) {
                                max_ms = static_cast<std::size_t>(v);
                            }
                        }
                        const auto t0 = std::chrono::steady_clock::now();
                        // DFS over r0 choices only; then greedy r1
                        bool found = false;
                        std::size_t nodes = 0;
                        Csm c_best = c; ConstraintState w_best = w;
                        two_row_dfs_bnb(0U, 0U, free_cand, capN, remaining0, r0, r1, lh, c_best, w_best, found, nodes, max_nodes, t0, max_ms, c, w, snap);
                        if (found) { c = c_best; w = w_best; }
                        snap.micro_solver_bnb_nodes += nodes;
                        if (found) {
                            ++snap.micro_solver_bnb_successes;
                        } else {
                            return false;
                        }
                    } else {
                        return false;
                    }
                }
            }
        }
        // Verify 2-row extension via LH
        const std::size_t check_rows = std::min<std::size_t>(r0 + 2U, S);
        const RowHashVerifier ver_try{};
        if (check_rows > 0) {
            ++snap.micro_solver_lh_verifications;
        }
        if (check_rows > 0 && ver_try.verify_rows(c, lh, check_rows)) {
            // Optional minimum benefit: total cells fixed across r0 and r1 must exceed threshold
            std::size_t min_cells2 = 0;
            if (const char *p = std::getenv("CRSCE_MS2_ACCEPT_MIN_CELLS") /* NOLINT(concurrency-mt-unsafe) */; p && *p) {
                const auto v = std::strtoll(p, nullptr, 10); if (v > 0) { min_cells2 = static_cast<std::size_t>(v); }
            }
            const auto delta_cells2 = static_cast<std::size_t>(baseline_st.U_row.at(r0) + baseline_st.U_row.at(r1));
            if (min_cells2 > 0 && delta_cells2 < min_cells2) {
                ++snap.micro_solver_reject_low_benefit;
            } else {
            csm_out = c;
            st = w;
            baseline_csm = csm_out;
            baseline_st = st;
            BlockSolveSnapshot::RestartEvent ev{};
            ev.restart_index = rs;
            ev.prefix_rows = check_rows;
            ev.unknown_total = snap.unknown_total;
            ev.action = BlockSolveSnapshot::RestartAction::lockInMicro;
            snap.restarts.push_back(ev);
            ++snap.micro_solver_successes; ++snap.micro_solver2_successes;
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
