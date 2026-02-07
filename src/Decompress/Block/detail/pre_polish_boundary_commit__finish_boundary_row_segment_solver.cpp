/**
 * @file pre_polish_boundary_commit__finish_boundary_row_segment_solver.cpp
 * @brief Implementation of sliding-window boundary row segment solver.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/Block/detail/pre_polish_finish_boundary_row_segment_solver.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include <utility>
#include <functional>
#include <cstdlib>
#include <cmath>
#include <chrono>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Utils/detail/index_calc.h"
#include "decompress/Block/detail/micro_solver_helpers.h"

namespace crsce::decompress::detail {
    using crsce::decompress::RowHashVerifier;

    // Helpers are provided by micro_solver_helpers.h to avoid local constructs here.

    /**
     * @name finish_boundary_row_segment_solver
     * @brief Sliding-window micro-solver to complete a boundary row segment under caps.
     * @param csm_out In/out CSM under construction.
     * @param st In/out constraint state.
     * @param lh LH digest span.
     * @param baseline_csm In/out baseline candidate CSM; updated on adoption.
     * @param baseline_st In/out baseline constraint state; updated on adoption.
     * @param snap In/out snapshot for metrics and events.
     * @param rs Current restart index for event attribution (unused).
     * @return bool True if a segment completion was adopted; false otherwise.
     */
    bool finish_boundary_row_segment_solver(Csm &csm_out,
                                            ConstraintState &st,
                                            std::span<const std::uint8_t> lh,
                                            Csm &baseline_csm,
                                            ConstraintState &baseline_st,
                                            BlockSolveSnapshot &snap,
                                            int rs) {
        constexpr std::size_t S = Csm::kS;
        (void)rs; // unused parameter for event attribution in this variant
        const RowHashVerifier verifier_now{};
        const std::size_t valid_now = verifier_now.longest_valid_prefix_up_to(baseline_csm, lh, S);
        if (valid_now >= S) { return false; }
        const std::size_t r = valid_now;
        const Csm c = baseline_csm; const ConstraintState w = baseline_st;
        if (w.U_row.at(r) == 0) { return false; }
        // Window size (env CRSCE_SEG_WINDOW, default 64)
        std::size_t W = 64;
        if (const char *wcap = std::getenv("CRSCE_SEG_WINDOW") /* NOLINT(concurrency-mt-unsafe) */; wcap && *wcap) {
            const int v = std::atoi(wcap); if (v > 0) { W = static_cast<std::size_t>(v); }
            W = std::max<std::size_t>(32, std::min<std::size_t>(W, 128));
        }
        // Gate: only attempt when both unknowns and remaining ones in the boundary row
        // are within the small window. This avoids exponential blowups.
        if (w.U_row.at(r) > W || w.R_row.at(r) > W) { return false; }
        // Collect unknown columns with pressure score
        std::vector<std::pair<double,std::size_t>> cells; cells.reserve(S);
        for (std::size_t c0 = 0; c0 < S; ++c0) {
            if (c.is_locked(r, c0)) { continue; }
            const std::size_t d = (c0 >= r) ? (c0 - r) : (c0 + S - r);
            const std::size_t x = ::crsce::decompress::detail::calc_x(r, c0);
            const double u_c = std::max(1.0, static_cast<double>(w.U_col.at(c0)));
            const double u_d = std::max(1.0, static_cast<double>(w.U_diag.at(d)));
            const double u_x = std::max(1.0, static_cast<double>(w.U_xdiag.at(x)));
            const double rneed_c = std::fabs((static_cast<double>(w.R_col.at(c0)) / u_c) - 0.5);
            const double rneed_d = std::fabs((static_cast<double>(w.R_diag.at(d)) / u_d) - 0.5);
            const double rneed_x = std::fabs((static_cast<double>(w.R_xdiag.at(x)) / u_x) - 0.5);
            const double score = (0.6 * rneed_c) + (0.2 * rneed_d) + (0.2 * rneed_x);
            cells.emplace_back(score, c0);
        }
        if (cells.empty()) { return false; }
        std::ranges::sort(cells, std::greater<double>{}, &std::pair<double,std::size_t>::first);
        const std::size_t capN = std::min<std::size_t>(cells.size(), W);
        if (capN == 0) { return false; }
        // Safety caps to ensure bounded runtime even under adversarial states
        std::size_t max_nodes = 200000; // default node cap
        if (const char *p = std::getenv("CRSCE_SEG_MAX_NODES") /* NOLINT(concurrency-mt-unsafe) */; p && *p) {
            const std::int64_t v = std::strtoll(p, nullptr, 10);
            if (v > 0) { max_nodes = static_cast<std::size_t>(v); }
        }
        std::size_t max_ms = 5000; // default time budget in ms
        if (const char *p = std::getenv("CRSCE_SEG_MAX_MS") /* NOLINT(concurrency-mt-unsafe) */; p && *p) {
            const std::int64_t v = std::strtoll(p, nullptr, 10);
            if (v > 0) { max_ms = static_cast<std::size_t>(v); }
        }
        const auto t0 = std::chrono::steady_clock::now();
        // Simple BnB over sliding window top-K cells; greedily fill outside
        bool adopted = false; std::size_t nodes = 0;
        Csm c_best = c; ConstraintState w_best = w;
        segment_dfs_bnb(0U, cells, capN, r, valid_now, lh, c_best, w_best, adopted, nodes, max_nodes, t0, max_ms, c, w, snap);
        if (adopted) { csm_out = c_best; st = w_best; baseline_csm = csm_out; baseline_st = st; }
        if (adopted) {
            ++snap.micro_solver_bnb_attempts;
            ++snap.micro_solver_bnb_successes;
            snap.micro_solver_bnb_nodes += nodes;
            return true;
        }
        snap.micro_solver_bnb_attempts += (nodes > 0 ? 1 : 0);
        snap.micro_solver_bnb_nodes += nodes;
        return false;
    }
}
