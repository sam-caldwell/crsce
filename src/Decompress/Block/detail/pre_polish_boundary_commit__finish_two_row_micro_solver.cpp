/**
 * @file pre_polish_boundary_commit__finish_two_row_micro_solver.cpp
 * @brief Definition of finish_two_row_micro_solver.
 */
#include "decompress/Block/detail/pre_polish_finish_two_row_micro_solver.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <chrono>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Utils/detail/index_calc.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"

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
            if (Rrow == 0 || Rcol == 0 || Rdiag == 0 || Rx == 0) {
                return false;
            }
            if ((Rrow - 1) > static_cast<std::uint16_t>(Urow - 1)) {
                return false;
            }
            if ((Rcol - 1) > static_cast<std::uint16_t>(Ucol - 1)) {
                return false;
            }
            if ((Rdiag - 1) > static_cast<std::uint16_t>(Udiag - 1)) {
                return false;
            }
            if ((Rx - 1) > static_cast<std::uint16_t>(Ux - 1)) {
                return false;
            }
        } else {
            if (Rrow > static_cast<std::uint16_t>(Urow - 1)) {
                return false;
            }
            if (Rcol > static_cast<std::uint16_t>(Ucol - 1)) {
                return false;
            }
            if (Rdiag > static_cast<std::uint16_t>(Udiag - 1)) {
                return false;
            }
            if (Rx > static_cast<std::uint16_t>(Ux - 1)) {
                return false;
            }
        }
        return true;
    }

    inline void assign_cell(Csm &c, ConstraintState &w, std::size_t r, std::size_t cc, bool v) {

        constexpr std::size_t S = Csm::kS;

        c.put(r, cc, v); c.lock(r, cc);
        if (w.U_row.at(r) > 0) {
            --w.U_row.at(r);
        }
        if (w.U_col.at(cc) > 0) {
            --w.U_col.at(cc);
        }
        const std::size_t d = ::crsce::decompress::detail::calc_d(r, cc);
        const std::size_t x = ::crsce::decompress::detail::calc_x(r, cc);
        if (w.U_diag.at(d) > 0) {
            --w.U_diag.at(d);
        }
        if (w.U_xdiag.at(x) > 0) {
            --w.U_xdiag.at(x);
        }
        if (v) {
            if (w.R_row.at(r) > 0) {
                --w.R_row.at(r);
            }
            if (w.R_col.at(cc) > 0) {
                --w.R_col.at(cc);
            }
            if (w.R_diag.at(d) > 0) {
                --w.R_diag.at(d);
            }
            if (w.R_xdiag.at(x) > 0) {
                --w.R_xdiag.at(x);
            }
        }
    }
}

namespace {
    bool finish_row_greedy(Csm &c, ConstraintState &w, std::size_t r) {
        constexpr std::size_t S = Csm::kS;
        if (w.U_row.at(r) == 0) {
            return true;
        }
        std::vector<std::size_t> cols; cols.reserve(S);
        for (std::size_t c0 = 0; c0 < S; ++c0) { if (!c.is_locked(r, c0)) { cols.push_back(c0); } }
        if (cols.empty()) {
            return false;
        }
        const std::uint16_t target = w.R_row.at(r);
        // Must-sets
        std::vector<std::size_t> must1; must1.reserve(cols.size());
        std::vector<std::size_t> must0; must0.reserve(cols.size());
        std::vector<std::pair<double, std::size_t>> free_cand; free_cand.reserve(cols.size());
        for (const auto cc : cols) {
            const bool can1 = local_feasible(w, r, cc, true);
            const bool can0 = local_feasible(w, r, cc, false);
            if (!can1 && !can0) { return false; }
            if (can1 && !can0) {
                must1.push_back(cc); continue;
            }
            if (!can1 && can0) {
                must0.push_back(cc); continue;
            }
            const double belief = c.get_data(r, cc);
            const double pc = static_cast<double>(w.R_col.at(cc)) / static_cast<double>(std::max<std::uint16_t>(1, w.U_col.at(cc)));
            const std::size_t d = ::crsce::decompress::detail::calc_d(r, cc);
            const std::size_t x = ::crsce::decompress::detail::calc_x(r, cc);
            const double pd = static_cast<double>(w.R_diag.at(d)) / static_cast<double>(std::max<std::uint16_t>(1, w.U_diag.at(d)));
            const double px = static_cast<double>(w.R_xdiag.at(x)) / static_cast<double>(std::max<std::uint16_t>(1, w.U_xdiag.at(x)));
            const double score = (0.5 * belief) + (0.5 * ((0.5 * pc) + (0.25 * pd) + (0.25 * px)));
            free_cand.emplace_back(score, cc);
        }
        if (must1.size() > static_cast<std::size_t>(target)) { return false; }
        for (const auto cc : must1) {
            assign_cell(c, w, r, cc, true);
        }
        const std::size_t remaining = static_cast<std::size_t>(target) - must1.size();
        if (!free_cand.empty() && remaining > 0) {
            std::ranges::sort(free_cand, std::greater<double>{}, &std::pair<double,std::size_t>::first);
            std::size_t picked = 0;
            for (const auto &p : free_cand) {
                if (picked >= remaining) {
                    break;
                }
                const std::size_t cc = p.second;
                if (!c.is_locked(r, cc) && local_feasible(w, r, cc, true)) {
                    assign_cell(c, w, r, cc, true);
                    ++picked;
                }
            }
        }
        // Assign zeros where feasible
        if (!free_cand.empty()) {
            for (const auto &p : free_cand) {
                if (w.U_row.at(r) == 0) {
                    break;
                }
                const std::size_t cc = p.second;
                if (c.is_locked(r, cc)) {
                    continue;
                }
                if (local_feasible(w, r, cc, false)) {
                    assign_cell(c, w, r, cc, false);
                }
            }
        }
        // Settle
        DeterministicElimination det{c, w};
        for (int it = 0; it < 6000; ++it) {
            const std::size_t prog = det.solve_step();
            if (prog == 0 || w.U_row.at(r) == 0) {
                break;
            }
        }
        return (w.U_row.at(r) == 0);
    }
    } // anonymous namespace

    bool finish_two_row_micro_solver(Csm &csm_out,
                                     ConstraintState &st,
                                     std::span<const std::uint8_t> lh,
                                     Csm &baseline_csm,
                                     ConstraintState &baseline_st,
                                     BlockSolveSnapshot &snap,
                                     int rs) {

        const auto t0 = std::chrono::steady_clock::now();
        ++snap.micro_solver_attempts;
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
        // Gate by near-threshold unknowns for both rows
        auto near_thresh = static_cast<std::uint16_t>((S + 4U) / 5U);
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
                        std::function<void(std::size_t,std::size_t,Csm&,ConstraintState&)> dfs;
                        dfs = [&](std::size_t i, std::size_t ones, Csm &c_cur, ConstraintState &w_cur) {
                            ++nodes;
                            if (found) {
                                return;
                            }
                            if (nodes > max_nodes) {
                                return;
                            }
                            const auto now = std::chrono::steady_clock::now();
                            const std::size_t elapsed_ms = static_cast<std::size_t>(
                                std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count()
                            );
                            if (elapsed_ms > max_ms) {
                                return;
                            }
                            const std::size_t left = (i < capN) ? (capN - i) : 0U;
                            if (ones > remaining0) {
                                return;
                            }
                            if (ones + left < remaining0) {
                                return;
                            }
                            if (i >= capN) {
                                // try greedy r1 on this partial r0 assignment
                                Csm c_try = c_cur; ConstraintState w_try = w_cur;
                                if (finish_row_greedy(c_try, w_try, r1)) {
                                    const std::size_t check_rows = std::min<std::size_t>(r0 + 2U, S);
                                    const RowHashVerifier ver2{};
                                    if (check_rows > 0 && ver2.verify_rows(c_try, lh, check_rows)) {
                                        c = c_try;
                                        w = w_try;
                                        found = true;
                                        return;
                                    }
                                }
                                return;
                            }
                            const std::size_t cc = free_cand[i].second;
                            // take 1 if feasible
                            if (local_feasible(w_cur, r0, cc, true)) {
                                Csm c1 = c_cur;
                                ConstraintState w1 = w_cur;
                                assign_cell(c1, w1, r0, cc, true);
                                dfs(i+1U, ones+1U, c1, w1);
                            }
                            // take 0 if feasible
                            if (local_feasible(w_cur, r0, cc, false)) {
                                Csm c0 = c_cur;
                                ConstraintState w0 = w_cur;
                                assign_cell(c0, w0, r0, cc, false);
                                dfs(i+1U, ones, c0, w0);
                            }
                        };
                        Csm c0 = c;
                        ConstraintState w0 = w;
                        dfs(0U, 0U, c0, w0);
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
            ++snap.micro_solver_successes;
            const auto t1 = std::chrono::steady_clock::now();
            snap.micro_solver_time_ms += static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
            return true;
        }
        const auto t1 = std::chrono::steady_clock::now();
        snap.micro_solver_time_ms += static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
        return false;
    }
}
