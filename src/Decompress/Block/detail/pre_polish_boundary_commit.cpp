/**
 * @file pre_polish_boundary_commit.cpp
 * @brief Implementation of pre_polish_boundary_commit helper.
 */
#include "decompress/Block/detail/pre_polish_boundary_commit.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include <limits>

namespace crsce::decompress::detail {
    using crsce::decompress::RowHashVerifier;
    using crsce::decompress::DeterministicElimination;

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
            std::ranges::sort(candidates, [](const auto &a, const auto &b){ return a.first < b.first; });
            const std::size_t cap = std::min<std::size_t>(candidates.size(), static_cast<std::size_t>(kBoundaryTryCells));
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
                    const std::size_t d0 = (c_pick >= boundary) ? (c_pick - boundary) : (c_pick + S - boundary);
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
        }
        if (st_work.U_row.at(boundary) == 0) {
            const RowHashVerifier ver_try;
            const std::size_t check_rows = std::min<std::size_t>(valid_now + 1, S);
            if (check_rows > 0 && ver_try.verify_rows(c_work, lh, check_rows)) {
                csm_out = c_work; st = st_work;
                std::size_t sumU2 = 0; for (const auto u : st.U_row) { sumU2 += static_cast<std::size_t>(u); }
                std::cerr << "PREFIX LOCKED-IN (pre-polish): rows=" << check_rows << ", unknown=" << sumU2 << ", rs=" << rs << "\n";
                baseline_csm = csm_out; baseline_st = st;
                BlockSolveSnapshot::RestartEvent ev{};
                ev.restart_index = rs; ev.prefix_rows = check_rows; ev.unknown_total = sumU2; ev.action = "lock-in";
                snap.restarts.push_back(ev);
                ++snap.boundary_finisher_successes;
                return true;
            }
        }
        return false;
    }

    bool finish_boundary_row_adaptive(Csm &csm_out, /* NOLINT(misc-use-internal-linkage) */
                                      ConstraintState &st,
                                      const std::span<const std::uint8_t> lh,
                                      Csm &baseline_csm,
                                      ConstraintState &baseline_st,
                                      BlockSolveSnapshot &snap,
                                      int rs,
                                      int stall_ticks) {
        constexpr std::size_t S = Csm::kS;
        const RowHashVerifier verifier_now{};
        const std::size_t valid_now = verifier_now.longest_valid_prefix_up_to(baseline_csm, lh, S);
        if (valid_now >= S) { return false; }
        const std::size_t boundary = valid_now;
        std::cerr << "Boundary finisher invoked: boundary=" << boundary
                  << " U_row[boundary]=" << static_cast<unsigned>(st.U_row.at(boundary)) << "\n";
        const std::size_t scale = static_cast<std::size_t>(std::min(stall_ticks / 128, 4));
        const int kBoundaryMaxSteps = 640 + (static_cast<int>(scale) * 256);
        const int kBoundaryBtIters = 30000 + (static_cast<int>(scale) * 10000);
        const std::size_t kBoundaryTryCells = std::min<std::size_t>(S, 384 + (scale * 128));
        Csm c_work = baseline_csm;
        ConstraintState st_work = baseline_st;
        int steps = 0;
        bool any_adopted_ever = false;
        // Treat rows with ≤~20% unknowns as near-complete (more aggressive bias)
        const auto near_thresh = static_cast<std::uint16_t>((S + 4U) / 5U); // ~20%
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
            std::ranges::sort(candidates, [](const auto &a, const auto &b){ return a.first < b.first; });
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
                    const std::size_t d0 = (c_pick >= boundary) ? (c_pick - boundary) : (c_pick + S - boundary);
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
            const std::size_t check_rows = std::min<std::size_t>(valid_now + 1, S);
            if (check_rows > 0 && ver_try.verify_rows(c_work, lh, check_rows)) {
                csm_out = c_work; st = st_work;
                baseline_csm = csm_out; baseline_st = st;
                BlockSolveSnapshot::RestartEvent ev{};
                ev.restart_index = rs; ev.prefix_rows = check_rows; ev.unknown_total = snap.unknown_total; ev.action = "lock-in";
                snap.restarts.push_back(ev);
                ++snap.boundary_finisher_successes;
                return true;
            }
        }
        // Temporarily adopt partial improvements even without verification
        if (any_adopted_ever) {
            csm_out = c_work; st = st_work;
            ++snap.partial_adoptions;
            return false;
        }
        return false;
    }

    namespace {
    void commit_row_locked(Csm &csm, ConstraintState &st, const std::size_t r) {
        constexpr std::size_t S = Csm::kS;
        for (std::size_t c = 0; c < S; ++c) {
            if (csm.is_locked(r, c)) { continue; }
            const bool bit = csm.get(r, c);
            csm.lock(r, c);
            if (st.U_row.at(r) > 0) { --st.U_row.at(r); }
            if (st.U_col.at(c) > 0) { --st.U_col.at(c); }
            const std::size_t d = (c >= r) ? (c - r) : (c + S - r);
            const std::size_t x = (r + c) % S;
            if (st.U_diag.at(d) > 0) { --st.U_diag.at(d); }
            if (st.U_xdiag.at(x) > 0) { --st.U_xdiag.at(x); }
            if (bit) {
                if (st.R_row.at(r) > 0) { --st.R_row.at(r); }
                if (st.R_col.at(c) > 0) { --st.R_col.at(c); }
                if (st.R_diag.at(d) > 0) { --st.R_diag.at(d); }
                if (st.R_xdiag.at(x) > 0) { --st.R_xdiag.at(x); }
            }
        }
    }
    } // anonymous namespace

    bool commit_any_verified_rows(Csm &csm_out,
                                  ConstraintState &st,
                                  const std::span<const std::uint8_t> lh,
                                  Csm &baseline_csm,
                                  ConstraintState &baseline_st,
                                  BlockSolveSnapshot &snap,
                                  int rs) {
        constexpr std::size_t S = Csm::kS;
        const RowHashVerifier ver{};
        bool any = false;
        for (std::size_t r = 0; r < S; ++r) {
            if (st.U_row.at(r) == 0) { continue; }
            if (ver.verify_row(csm_out, lh, r)) {
                commit_row_locked(csm_out, st, r);
                baseline_csm = csm_out; baseline_st = st;
                BlockSolveSnapshot::RestartEvent ev{};
                ev.restart_index = rs; ev.prefix_rows = r; ev.unknown_total = snap.unknown_total; ev.action = "lock-in-row";
                snap.restarts.push_back(ev);
                ++snap.rows_committed;
                ++snap.lock_in_row_count;
                const std::size_t pnew = ver.longest_valid_prefix_up_to(csm_out, lh, S);
                if (snap.prefix_progress.empty() || snap.prefix_progress.back().prefix_len != pnew) {
                    BlockSolveSnapshot::PrefixSample s{}; s.iter = snap.iter; s.prefix_len = pnew; snap.prefix_progress.push_back(s);
                }
                any = true;
            }
        }
        return any;
    }

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
            std::ranges::sort(cand_cells, [](const auto &a, const auto &b){ return a.first < b.first; });
            const std::size_t K = std::min<std::size_t>(cand_cells.size(), 16);
            bool improved_any = false;
            for (std::size_t i = 0; i < K; ++i) {
                const std::size_t c_pick = cand_cells[i].second;
                bool adopted_step = false;
                const std::uint16_t before = st.U_row.at(candidate);
                for (int vv = 0; vv < 2 && !adopted_step; ++vv) {
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
                            ev.restart_index = rs; ev.prefix_rows = candidate; ev.unknown_total = snap.unknown_total; ev.action = "lock-in-row";
                            snap.restarts.push_back(ev);
                            ++snap.rows_committed;
                            ++snap.lock_in_row_count;
                            const std::size_t pnew = verifier_now.longest_valid_prefix_up_to(csm_out, lh, S);
                            if (snap.prefix_progress.empty() || snap.prefix_progress.back().prefix_len != pnew) {
                                BlockSolveSnapshot::PrefixSample s{}; s.iter = snap.iter; s.prefix_len = pnew; snap.prefix_progress.push_back(s);
                            }
                            return true;
                        }
                        if (st_try.U_row.at(candidate) < before) { csm_out = c_try; st = st_try; ++snap.partial_adoptions; adopted_step = true; }
                    }
                }
                if (adopted_step) { improved_any = true; break; }
            }
            if (!improved_any) { break; }
        }
        return false;
    }

    bool finish_near_complete_top_rows(Csm &csm_out,
                                       ConstraintState &st,
                                       const std::span<const std::uint8_t> lh,
                                       Csm &baseline_csm,
                                       ConstraintState &baseline_st,
                                       BlockSolveSnapshot &snap,
                                       int rs,
                                       const std::size_t top_n,
                                       const std::size_t top_k_cells) {
        constexpr std::size_t S = Csm::kS;
        // Gather candidate rows with U_row > 0
        std::vector<std::pair<std::uint16_t, std::size_t>> rows;
        rows.reserve(S);
        for (std::size_t r = 0; r < S; ++r) {
            const auto u = st.U_row.at(r);
            if (u > 0) { rows.emplace_back(u, r); }
        }
        if (rows.empty()) { return false; }
        std::ranges::sort(rows, [](const auto &a, const auto &b){ return a.first < b.first; });
        const std::size_t take = std::min<std::size_t>(rows.size(), top_n);
        for (std::size_t ir = 0; ir < take; ++ir) {
            const std::size_t r = rows[ir].second;
            const auto u = rows[ir].first;
            // only near-complete rows (≤~20%)
            const auto near_thresh = static_cast<std::uint16_t>((S + 4U) / 5U); // ~20%
            if (u == 0 || u > near_thresh) { continue; }
            // build top-K ambiguous cells for the row
            std::vector<std::pair<double, std::size_t>> cells;
            cells.reserve(S);
            for (std::size_t c0 = 0; c0 < S; ++c0) {
                if (csm_out.is_locked(r, c0)) { continue; }
                const double p = csm_out.get_data(r, c0);
                const double amb = std::fabs(p - 0.5);
                cells.emplace_back(amb, c0);
            }
            if (cells.empty()) { continue; }
            std::ranges::sort(cells, [](const auto &a, const auto &b){ return a.first < b.first; });
            const std::size_t K = std::min<std::size_t>(cells.size(), top_k_cells);
            const RowHashVerifier verifier{};
            for (std::size_t i = 0; i < K; ++i) {
                const std::size_t c_pick = cells[i].second;
                const std::uint16_t before = st.U_row.at(r);
                for (int vv = 0; vv < 2; ++vv) {
                    const bool assume_one = (vv == 1);
                    Csm c_try = csm_out;
                    ConstraintState st_try = st;
                    if (c_try.is_locked(r, c_pick)) { continue; }
                    c_try.put(r, c_pick, assume_one);
                    c_try.lock(r, c_pick);
                    if (st_try.U_row.at(r) > 0) { --st_try.U_row.at(r); }
                    if (st_try.U_col.at(c_pick) > 0) { --st_try.U_col.at(c_pick); }
                    const std::size_t d0 = (c_pick >= r) ? (c_pick - r) : (c_pick + S - r);
                    const std::size_t x0 = (r + c_pick) % S;
                    if (st_try.U_diag.at(d0) > 0) { --st_try.U_diag.at(d0); }
                    if (st_try.U_xdiag.at(x0) > 0) { --st_try.U_xdiag.at(x0); }
                    if (assume_one) {
                        if (st_try.R_row.at(r) > 0) { --st_try.R_row.at(r); }
                        if (st_try.R_col.at(c_pick) > 0) { --st_try.R_col.at(c_pick); }
                        if (st_try.R_diag.at(d0) > 0) { --st_try.R_diag.at(d0); }
                        if (st_try.R_xdiag.at(x0) > 0) { --st_try.R_xdiag.at(x0); }
                    }
                    DeterministicElimination det_bt{c_try, st_try};
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
                            ev.restart_index = rs; ev.prefix_rows = r; ev.unknown_total = snap.unknown_total; ev.action = "lock-in-row";
                            snap.restarts.push_back(ev);
                            return true;
                        }
                        if (st_try.U_row.at(r) < before) { csm_out = c_try; st = st_try; ++snap.partial_adoptions; }
                    }
                }
            }
        }
        return false;
    }

    /**
     * @name finish_near_complete_rows_scored
     * @brief Try to finish specific candidate rows (in given order) using local backtrack + DE, verify/commit on success.
     * @param csm_out
     * @param st
     * @param lh
     * @param baseline_csm
     * @param baseline_st
     * @param snap
     * @param rs
     * @param rows List of row indices to try (ordered by external score).
     * @param top_k_cells Max ambiguous cells to probe per row.
     * @return true if any row was verified and committed.
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
            std::ranges::sort(cells, [](const auto &a, const auto &b){ return a.first < b.first; });
            const std::size_t K = std::min<std::size_t>(cells.size(), top_k_cells);
            for (std::size_t i = 0; i < K; ++i) {
                const std::size_t c_pick = cells[i].second;
                const std::uint16_t before = st.U_row.at(r);
                for (int vv = 0; vv < 2; ++vv) {
                    const bool assume_one = (vv == 1);
                    Csm c_try = csm_out;
                    ConstraintState st_try = st;
                    if (c_try.is_locked(r, c_pick)) { continue; }
                    c_try.put(r, c_pick, assume_one);
                    c_try.lock(r, c_pick);
                    if (st_try.U_row.at(r) > 0) { --st_try.U_row.at(r); }
                    if (st_try.U_col.at(c_pick) > 0) { --st_try.U_col.at(c_pick); }
                    const std::size_t d0 = (c_pick >= r) ? (c_pick - r) : (c_pick + S - r);
                    const std::size_t x0 = (r + c_pick) % S;
                    if (st_try.U_diag.at(d0) > 0) { --st_try.U_diag.at(d0); }
                    if (st_try.U_xdiag.at(x0) > 0) { --st_try.U_xdiag.at(x0); }
                    if (assume_one) {
                        if (st_try.R_row.at(r) > 0) { --st_try.R_row.at(r); }
                        if (st_try.R_col.at(c_pick) > 0) { --st_try.R_col.at(c_pick); }
                        if (st_try.R_diag.at(d0) > 0) { --st_try.R_diag.at(d0); }
                        if (st_try.R_xdiag.at(x0) > 0) { --st_try.R_xdiag.at(x0); }
                    }
                    DeterministicElimination det_bt{c_try, st_try};
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
                            ev.restart_index = rs; ev.prefix_rows = r; ev.unknown_total = snap.unknown_total; ev.action = "lock-in-row";
                            snap.restarts.push_back(ev);
                            return true;
                        }
                        if (st_try.U_row.at(r) < before) { csm_out = c_try; st = st_try; ++snap.partial_adoptions; }
                    }
                }
            }
        }
        return false;
    }

    /**
     * @name finish_near_complete_columns_scored
     * @brief Try to finish specific candidate columns (in given order) using local backtrack + DE, verify/commit on success.
     */
    bool finish_near_complete_columns_scored(Csm &csm_out,
                                             ConstraintState &st,
                                             const std::span<const std::uint8_t> lh,
                                             Csm &baseline_csm,
                                             ConstraintState &baseline_st,
                                             BlockSolveSnapshot &snap,
                                             int rs,
                                             const std::vector<std::size_t> &cols,
                                             const std::size_t top_k_cells) {
        constexpr std::size_t S = Csm::kS;
        const auto near_thresh = static_cast<std::uint16_t>((S + 4U) / 5U);
        const RowHashVerifier verifier{};
        for (const auto c : cols) {
            if (c >= S) { continue; }
            const auto u = st.U_col.at(c);
            if (u == 0 || u > near_thresh) { continue; }
            std::vector<std::pair<double, std::size_t>> cells;
            cells.reserve(S);
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
                    const std::size_t d0 = (c >= r) ? (c - r) : (c + S - r);
                    const std::size_t x0 = (r + c) % S;
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
                    if (st_try.U_row.at(r) == 0 || st_try.U_row.at(r) < before_row || st_try.U_col.at(c) == 0 || st_try.U_col.at(c) < before_col) {
                        if (verifier.verify_row(c_try, lh, r)) {
                            csm_out = c_try; st = st_try;
                            commit_row_locked(csm_out, st, r);
                            baseline_csm = csm_out; baseline_st = st;
                            BlockSolveSnapshot::RestartEvent ev{};
                            ev.restart_index = rs; ev.prefix_rows = r; ev.unknown_total = snap.unknown_total; ev.action = "lock-in-row";
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
                            csm_out = c_try; st = st_try; ++snap.partial_adoptions;
                        }
                    }
                }
            }
        }
        return false;
    }
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
        // Gather candidate columns with U_col > 0 and sort by fewest unknowns
        std::vector<std::pair<std::uint16_t, std::size_t>> cols;
        cols.reserve(S);
        for (std::size_t c = 0; c < S; ++c) {
            const auto u = st.U_col.at(c);
            if (u > 0) { cols.emplace_back(u, c); }
        }
        if (cols.empty()) { return false; }
        std::ranges::sort(cols, [](const auto &a, const auto &b){ return a.first < b.first; });
        const std::size_t take = std::min<std::size_t>(cols.size(), top_n_cols);

        // Near-complete threshold (~20%)
        const auto near_thresh = static_cast<std::uint16_t>((S + 4U) / 5U);
        const RowHashVerifier verifier{};

        for (std::size_t ic = 0; ic < take; ++ic) {
            const std::size_t c = cols[ic].second;
            const auto u = cols[ic].first;
            if (u == 0 || u > near_thresh) { continue; }

            // Build top-K ambiguous cells for this column across rows
            std::vector<std::pair<double, std::size_t>> cells;
            cells.reserve(S);
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
                    const std::size_t d0 = (c >= r) ? (c - r) : (c + S - r);
                    const std::size_t x0 = (r + c) % S;
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
                            ev.restart_index = rs; ev.prefix_rows = r; ev.unknown_total = snap.unknown_total; ev.action = "lock-in-row";
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
                            csm_out = c_try; st = st_try; ++snap.partial_adoptions;
                        }
                    }
                }
            }
        }
        return false;
    }

    bool audit_and_restart_on_contradiction(Csm &csm_out,
                                            ConstraintState &st,
                                            const std::span<const std::uint8_t> lh,
                                            Csm &baseline_csm,
                                            ConstraintState &baseline_st,
                                            BlockSolveSnapshot &snap,
                                            int rs,
                                            const std::uint64_t valid_bits,
                                            const int cooldown_ticks,
                                            int &since_last_restart) {
        constexpr std::size_t S = Csm::kS;
        const RowHashVerifier ver{};
        // compute last data row index; skip padded-only rows
        if (valid_bits == 0) { return false; }
        const auto last_data_row = static_cast<std::size_t>((valid_bits - 1U) / S);
        for (std::size_t r = 0; r < S; ++r) {
            if (r > last_data_row) { continue; }
            if (st.U_row.at(r) == 0) {
                if (!ver.verify_row(csm_out, lh, r)) {
                    if (since_last_restart < cooldown_ticks) { continue; }
                    // Contradiction: fully locked row fails digest. Revert to baseline and record restart.
                    csm_out = baseline_csm;
                    st = baseline_st;
                    BlockSolveSnapshot::RestartEvent ev{};
                    ev.restart_index = rs;
                    ev.prefix_rows = r;
                    ev.unknown_total = snap.unknown_total;
                    ev.action = "restart-contradiction";
            snap.restarts.push_back(ev);
            ++snap.restart_contradiction_count;
            since_last_restart = 0;
            return true;
                }
            }
        }
        return false;
    }
}
namespace crsce::decompress::detail {
    bool commit_valid_prefix(Csm &csm_out, /* NOLINT(misc-use-internal-linkage) */
                             ConstraintState &st,
                             const std::span<const std::uint8_t> lh,
                             Csm &baseline_csm,
                             ConstraintState &baseline_st,
                             BlockSolveSnapshot &snap,
                             int rs) {
        constexpr std::size_t S = Csm::kS;
        const RowHashVerifier ver{};
        const std::size_t k = ver.longest_valid_prefix_up_to(csm_out, lh, S);
        if (k == 0) { return false; }
        bool any = false;
        for (std::size_t r = 0; r < k; ++r) {
            if (st.U_row.at(r) > 0) {
                commit_row_locked(csm_out, st, r);
                any = true;
            }
        }
        if (any) {
            baseline_csm = csm_out; baseline_st = st;
            BlockSolveSnapshot::RestartEvent ev{};
            ev.restart_index = rs; ev.prefix_rows = k; ev.unknown_total = snap.unknown_total; ev.action = "lock-in-prefix";
            snap.restarts.push_back(ev);
            ++snap.lock_in_prefix_count;
            ++snap.rows_committed; // at least one row was committed
            BlockSolveSnapshot::PrefixSample s{}; s.iter = snap.iter; s.prefix_len = k; snap.prefix_progress.push_back(s);
        }
        return any;
    }
}
