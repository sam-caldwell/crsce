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
#include <cstdlib>
#include <cmath>
#include <functional>
#include <chrono>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Utils/detail/index_calc.h"

namespace crsce::decompress::detail {
    using crsce::decompress::RowHashVerifier;
    using crsce::decompress::DeterministicElimination;

    namespace {
    inline bool local_feasible(const ConstraintState &st, const std::size_t r, const std::size_t c, const bool v) {
        constexpr std::size_t S = Csm::kS;
        const std::size_t d = ::crsce::decompress::detail::calc_d(r, c);
        const std::size_t x = ::crsce::decompress::detail::calc_x(r, c);
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
    inline void assign_cell(Csm &c, ConstraintState &w, std::size_t r, std::size_t cc, bool v) {
        constexpr std::size_t S = Csm::kS;
        c.put(r, cc, v); c.lock(r, cc);
        if (w.U_row.at(r) > 0) { --w.U_row.at(r); }
        if (w.U_col.at(cc) > 0) { --w.U_col.at(cc); }
        const std::size_t d = (cc >= r) ? (cc - r) : (cc + S - r);
        const std::size_t x = ::crsce::decompress::detail::calc_x(r, cc);
        if (w.U_diag.at(d) > 0) { --w.U_diag.at(d); }
        if (w.U_xdiag.at(x) > 0) { --w.U_xdiag.at(x); }
        if (v) {
            if (w.R_row.at(r) > 0) { --w.R_row.at(r); }
            if (w.R_col.at(cc) > 0) { --w.R_col.at(cc); }
            if (w.R_diag.at(d) > 0) { --w.R_diag.at(d); }
            if (w.R_xdiag.at(x) > 0) { --w.R_xdiag.at(x); }
        }
    }
    } // anonymous

    bool finish_boundary_row_segment_solver(Csm &csm_out,
                                            ConstraintState &st,
                                            std::span<const std::uint8_t> lh,
                                            Csm &baseline_csm,
                                            ConstraintState &baseline_st,
                                            BlockSolveSnapshot &snap,
                                            int /*rs*/) {
        constexpr std::size_t S = Csm::kS;
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
        std::function<void(std::size_t,Csm&,ConstraintState&)> dfs;
        dfs = [&](std::size_t i, Csm &c_cur, ConstraintState &w_cur) {
            ++nodes;
            if (adopted) { return; }
            if (nodes > max_nodes) { return; }
            const auto now = std::chrono::steady_clock::now();
            const std::size_t elapsed_ms = static_cast<std::size_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count()
            );
            if (elapsed_ms > max_ms) { return; }
            if (i >= capN) {
                // Greedy fill ones outside if needed
                if (w_cur.U_row.at(r) > 0 && w_cur.R_row.at(r) > 0) {
                    for (std::size_t j = capN; j < cells.size() && w_cur.R_row.at(r) > 0; ++j) {
                        const std::size_t cc = cells[j].second;
                        if (!c_cur.is_locked(r, cc) && local_feasible(w_cur, r, cc, true)) { assign_cell(c_cur, w_cur, r, cc, true); }
                    }
                }
                for (std::size_t j = capN; j < cells.size(); ++j) {
                    const std::size_t cc = cells[j].second;
                    if (!c_cur.is_locked(r, cc) && w_cur.U_row.at(r) > 0 && local_feasible(w_cur, r, cc, false)) { assign_cell(c_cur, w_cur, r, cc, false); }
                }
                DeterministicElimination det_bt{c_cur, w_cur};
                for (int it = 0; it < 4000; ++it) { const auto p = det_bt.solve_step(); if (p == 0 || w_cur.U_row.at(r) == 0) { break; } }
                if (w_cur.U_row.at(r) == 0) {
                    const std::size_t check_rows = std::min<std::size_t>(valid_now + 1, S);
                    const RowHashVerifier ver{}; if (check_rows > 0 && ver.verify_rows(c_cur, lh, check_rows)) {
                        csm_out = c_cur; st = w_cur; baseline_csm = csm_out; baseline_st = st; adopted = true; return; }
                }
                return;
            }
            const std::size_t cc = cells[i].second;
            // Branch 1
            if (local_feasible(w_cur, r, cc, true)) { Csm c1 = c_cur; ConstraintState w1 = w_cur; assign_cell(c1, w1, r, cc, true); dfs(i+1U, c1, w1); }
            // Branch 0
            if (!adopted && local_feasible(w_cur, r, cc, false)) { Csm c0 = c_cur; ConstraintState w0 = w_cur; assign_cell(c0, w0, r, cc, false); dfs(i+1U, c0, w0); }
        };
        Csm c0 = c; ConstraintState w0 = w; dfs(0U, c0, w0);
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
