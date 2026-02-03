/**
 * @file BlockSolver.cpp
 * @brief High-level block solver implementation: reconstruct CSM from LH and cross-sum payloads.
 * @copyright © Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Block/detail/solve_block.h"
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/LHChainVerifier/LHChainVerifier.h"
#include "decompress/Gobp/GobpSolver.h"
#include "decompress/Utils/detail/verify_cross_sums.h"
#include "decompress/Utils/detail/decode9.tcc"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Block/detail/set_block_solve_snapshot.h"

#include <cstddef>
#include <cstdint> //NOLINT
#include <cstdlib>
#include <iostream>
#include <array>
#include <algorithm>
#include <random>
#include <span>
#include <string>
#include <exception>

namespace crsce::decompress {
    /**
     * @name solve_block
     * @brief Reconstruct a block CSM from LH and cross-sum payloads.
     * @param lh Span of 511 chained LH digests (32 bytes each).
     * @param sums Span of 4 serialized cross-sum vectors (2300 bytes total).
     * @param csm_out Output CSM to populate.
     * @param seed Seed string used by LH chain.
     * @param valid_bits Number of meaningful bits in this block. Any cells beyond
     *        this count are treated as padded: they are set to 0 and locked prior
     *        to solving, so DE/GOBP operate only on valid data.
     * @return bool True on success; false if solving or verification fails.
     */
    bool solve_block(const std::span<const std::uint8_t> lh,
                     const std::span<const std::uint8_t> sums,
                     Csm &csm_out,
                     const std::string &seed,
                     const std::uint64_t valid_bits) {
        constexpr std::size_t S = Csm::kS;
        constexpr std::size_t vec_bytes = 575U;

        const auto lsm = decode_9bit_stream<S>(sums.subspan(0 * vec_bytes, vec_bytes));
        const auto vsm = decode_9bit_stream<S>(sums.subspan(1 * vec_bytes, vec_bytes));
        const auto dsm = decode_9bit_stream<S>(sums.subspan(2 * vec_bytes, vec_bytes));
        const auto xsm = decode_9bit_stream<S>(sums.subspan(3 * vec_bytes, vec_bytes));

        ConstraintState st{};
        st.R_row = lsm;
        st.R_col = vsm;
        st.R_diag = dsm;
        st.R_xdiag = xsm;
        st.U_row.fill(S);
        st.U_col.fill(S);
        st.U_diag.fill(S);
        st.U_xdiag.fill(S);

        // Ensure clean CSM before applying any pre-locks
        csm_out.reset();

        // Pre-lock padded cells (beyond valid_bits) as zeros and decrement unknown counts.
        if (constexpr std::uint64_t total_bits = static_cast<std::uint64_t>(S) * static_cast<std::uint64_t>(S);
            valid_bits < total_bits) {
            // Optional debug: emit one-line summary when enabled
            static int prelock_dbg = -1; // -1 unset, 0 off, 1 on
            if (prelock_dbg < 0) {
                const char *e = std::getenv("CRSCE_PRELOCK_DEBUG"); // NOLINT(concurrency-mt-unsafe)
                prelock_dbg = (e && *e) ? 1 : 0;
            }
            if (prelock_dbg == 1) {
                const auto start_r = static_cast<std::size_t>(valid_bits / S);
                const auto start_c = static_cast<std::size_t>(valid_bits % S);
                std::cerr << "Pre-locking padded tail from bit " << valid_bits
                        << " (r=" << start_r << ", c=" << start_c << ") count="
                        << (total_bits - valid_bits) << "\n";
            }
            for (std::uint64_t idx = valid_bits; idx < total_bits; ++idx) {
                const auto r = static_cast<std::size_t>(idx / S);
                const auto c = static_cast<std::size_t>(idx % S);
                // Set and lock the padded cell
                csm_out.put(r, c, false);
                csm_out.lock(r, c);
                // Update unknown counts for affected lines
                if (st.U_row.at(r) > 0) { --st.U_row.at(r); }
                if (st.U_col.at(c) > 0) { --st.U_col.at(c); }
                const std::size_t d = (c >= r) ? (c - r) : (c + S - r);
                const std::size_t x = (r + c) % S;
                if (st.U_diag.at(d) > 0) { --st.U_diag.at(d); }
                if (st.U_xdiag.at(x) > 0) { --st.U_xdiag.at(x); }
            }
        }
        DeterministicElimination det{csm_out, st};

        // Initialize snapshot
        BlockSolveSnapshot snap{};
        snap.S = S;
        snap.iter = 0;
        snap.phase = "init";
        snap.message = "";
        snap.lsm.assign(lsm.begin(), lsm.end());
        snap.vsm.assign(vsm.begin(), vsm.end());
        snap.dsm.assign(dsm.begin(), dsm.end());
        snap.xsm.assign(xsm.begin(), xsm.end());
        snap.U_row.assign(st.U_row.begin(), st.U_row.end());
        snap.U_col.assign(st.U_col.begin(), st.U_col.end());
        snap.U_diag.assign(st.U_diag.begin(), st.U_diag.end());
        snap.U_xdiag.assign(st.U_xdiag.begin(), st.U_xdiag.end());
        {
            std::size_t sumU = 0;
            for (const auto u: st.U_row) {
                sumU += static_cast<std::size_t>(u);
            }
            snap.unknown_total = sumU;
            snap.solved = (static_cast<std::size_t>(S) * static_cast<std::size_t>(S)) - sumU;
        }
        set_block_solve_snapshot(snap);

        // Hard-coded deterministic elimination iteration cap
        static constexpr int kMaxIters = 60000;

        for (int iter = 0; iter < kMaxIters; ++iter) {
            std::size_t progress = 0;
            try {
                snap.phase = "de";
                progress += det.solve_step();
            } catch (const std::exception &e) {
                snap.iter = static_cast<std::size_t>(iter);
                snap.message = e.what();
                snap.U_row.assign(st.U_row.begin(), st.U_row.end());
                snap.U_col.assign(st.U_col.begin(), st.U_col.end());
                snap.U_diag.assign(st.U_diag.begin(), st.U_diag.end());
                snap.U_xdiag.assign(st.U_xdiag.begin(), st.U_xdiag.end());
                {
                    std::size_t sumU = 0;
                    for (const auto u: st.U_row) {
                        sumU += static_cast<std::size_t>(u);
                    }
                    snap.unknown_total = sumU;
                    snap.solved = (static_cast<std::size_t>(S) * static_cast<std::size_t>(S)) - sumU;
                }
                set_block_solve_snapshot(snap);
                return false;
            }
            // Update snapshot after iteration
            snap.iter = static_cast<std::size_t>(iter);
            snap.U_row.assign(st.U_row.begin(), st.U_row.end());
            snap.U_col.assign(st.U_col.begin(), st.U_col.end());
            snap.U_diag.assign(st.U_diag.begin(), st.U_diag.end());
            snap.U_xdiag.assign(st.U_xdiag.begin(), st.U_xdiag.end());
            {
                std::size_t sumU = 0;
                for (const auto u: st.U_row) {
                    sumU += static_cast<std::size_t>(u);
                }
                snap.unknown_total = sumU;
                snap.solved = (static_cast<std::size_t>(S) * static_cast<std::size_t>(S)) - sumU;
            }
            set_block_solve_snapshot(snap);
            if (det.solved()) {
                std::cerr << "Block Solver terminating with possible solution\n";
                break;
            }
            // Optional early stop when no additional progress is made.
            if (progress == 0) {
                std::cerr << "Block Solver reached steady state (no progress)\n";
                break;
            }
            std::cerr << "Block Solver iteration ended at " << iter << " of " << kMaxIters
                    << " iterations (with " << progress << " progress)\n";
        }
        std::cerr << "Block Solver terminating...\n";
        if (!det.solved()) {
            // --- GOBP fallback (hard-coded schedule) ---
            static constexpr double gobp_damp = 0.50;
            static constexpr double gobp_conf = 0.995;
            static constexpr int gobp_iters = 100000;
            static constexpr bool multiphase = true;
            // Baseline state for restart-on-prefix-mismatch or plateau restarts
            Csm baseline_csm = csm_out;
            ConstraintState baseline_st = st;
            // Hard-coded restarts + perturbation (plateau escape)
            static constexpr int kRestarts = 3; // additional attempts after the initial run
            static constexpr double kPerturb = 0.01; // +/- jitter to beliefs on restart
            std::mt19937_64 rng(0xC0FFEEULL);

            for (int rs = 0; rs <= kRestarts && !det.solved(); ++rs) {
                bool restart_triggered = false;
                if (rs > 0) {
                    // Revert to last locked-in baseline and perturb beliefs slightly to escape plateaus
                    csm_out = baseline_csm;
                    st = baseline_st;
                    std::uniform_real_distribution<double> delta(-kPerturb, kPerturb);
                    for (std::size_t r0 = 0; r0 < S; ++r0) {
                        for (std::size_t c0 = 0; c0 < S; ++c0) {
                            if (csm_out.is_locked(r0, c0)) { continue; }
                            const double v = csm_out.get_data(r0, c0);
                            const double j = delta(rng);
                            const double nv = std::clamp(v + j, 0.0, 1.0);
                            csm_out.set_data(r0, c0, nv);
                        }
                    }
                }

                if (multiphase) {
                // Three-phase schedule (hard-coded)
                static constexpr std::array<double, 3> dampers{{0.50, 0.10, 0.01}};
                static constexpr std::array<double, 3> confs{{0.995, 0.80, 0.48}};
                static constexpr std::array<int, 3> phase_iters{{500, 4500, 95000}};
                for (int ph = 0; ph < 3 && !det.solved(); ++ph) {
                    GobpSolver gobp(csm_out, st, dampers.at(ph), confs.at(ph));
                    for (int gi = 0; gi < phase_iters.at(ph); ++gi) {
                        std::size_t gprog = 0;
                        try {
                            snap.phase = "gobp";
                            gprog += gobp.solve_step();
                            // Follow with a DE sweep to propagate any newly forced moves
                            gprog += det.solve_step();
                        } catch (const std::exception &e) {
                            snap.iter = static_cast<std::size_t>(gi);
                            snap.message = e.what();
                            snap.U_row.assign(st.U_row.begin(), st.U_row.end());
                            snap.U_col.assign(st.U_col.begin(), st.U_col.end());
                            snap.U_diag.assign(st.U_diag.begin(), st.U_diag.end());
                            snap.U_xdiag.assign(st.U_xdiag.begin(), st.U_xdiag.end());
                            {
                                std::size_t sumU = 0;
                                for (const auto u: st.U_row) { sumU += static_cast<std::size_t>(u); }
                                snap.unknown_total = sumU;
                                snap.solved = (static_cast<std::size_t>(S) * static_cast<std::size_t>(S)) - sumU;
                            }
                            set_block_solve_snapshot(snap);
                            return false;
                        }
                        // Update snapshot after this iteration
                        snap.iter = static_cast<std::size_t>(gi);
                        snap.U_row.assign(st.U_row.begin(), st.U_row.end());
                        snap.U_col.assign(st.U_col.begin(), st.U_col.end());
                        snap.U_diag.assign(st.U_diag.begin(), st.U_diag.end());
                        snap.U_xdiag.assign(st.U_xdiag.begin(), st.U_xdiag.end());
                        {
                            std::size_t sumU = 0;
                            for (const auto u: st.U_row) { sumU += static_cast<std::size_t>(u); }
                            snap.unknown_total = sumU;
                            snap.solved = (static_cast<std::size_t>(S) * static_cast<std::size_t>(S)) - sumU;
                        }
                        set_block_solve_snapshot(snap);
                        // Detect largest solved prefix; verify against LH; decide restart/lock-in
                        {
                            std::size_t prefix_rows = 0;
                            for (std::size_t r0 = 0; r0 < S; ++r0) {
                                if (st.U_row.at(r0) == 0) { ++prefix_rows; } else { break; }
                            }
                            if (prefix_rows > 0) {
                                const LHChainVerifier verifier(seed);
                                if (!verifier.verify_rows(csm_out, lh, prefix_rows)) {
                                    std::cerr << "LH prefix gating failed at rows=" << prefix_rows << "; restarting from baseline\n";
                                    // Restart: revert to last good baseline and break to backtracking
                                    csm_out = baseline_csm;
                                    st = baseline_st;
                                    restart_triggered = true;
                                    break; // break gi loop
                                }
                                // Lock-in: commit this state as new baseline when not restarted
                                if (!restart_triggered) {
                                    baseline_csm = csm_out;
                                    baseline_st = st;
                                }
                            }
                        }
                        if (det.solved()) {
                            std::cerr << "GOBP converged to a solution\n";
                            break;
                        }
                        if (gprog == 0) {
                            std::cerr << "GOBP reached steady state (no progress)\n";
                            break;
                        }
                    }
                    if (restart_triggered) { break; }
                }
            } else {
                GobpSolver gobp(csm_out, st, gobp_damp, gobp_conf);
                for (int gi = 0; gi < gobp_iters; ++gi) {
                    std::size_t gprog = 0;
                    try {
                        snap.phase = "gobp";
                        gprog += gobp.solve_step();
                        // Follow with a DE sweep to propagate any newly forced moves
                        gprog += det.solve_step();
                    } catch (const std::exception &e) {
                        snap.iter = static_cast<std::size_t>(gi);
                        snap.message = e.what();
                        snap.U_row.assign(st.U_row.begin(), st.U_row.end());
                        snap.U_col.assign(st.U_col.begin(), st.U_col.end());
                        snap.U_diag.assign(st.U_diag.begin(), st.U_diag.end());
                        snap.U_xdiag.assign(st.U_xdiag.begin(), st.U_xdiag.end());
                        {
                            std::size_t sumU = 0;
                            for (const auto u: st.U_row) { sumU += static_cast<std::size_t>(u); }
                            snap.unknown_total = sumU;
                            snap.solved = (static_cast<std::size_t>(S) * static_cast<std::size_t>(S)) - sumU;
                        }
                        set_block_solve_snapshot(snap);
                        return false;
                    }
                    // Update snapshot after this iteration
                    snap.iter = static_cast<std::size_t>(gi);
                    snap.U_row.assign(st.U_row.begin(), st.U_row.end());
                    snap.U_col.assign(st.U_col.begin(), st.U_col.end());
                    snap.U_diag.assign(st.U_diag.begin(), st.U_diag.end());
                    snap.U_xdiag.assign(st.U_xdiag.begin(), st.U_xdiag.end());
                    {
                        std::size_t sumU = 0;
                        for (const auto u: st.U_row) { sumU += static_cast<std::size_t>(u); }
                        snap.unknown_total = sumU;
                        snap.solved = (static_cast<std::size_t>(S) * static_cast<std::size_t>(S)) - sumU;
                    }
                    set_block_solve_snapshot(snap);
                    // Prefix-LH gating with restart/lock-in
                    {
                        std::size_t prefix_rows = 0;
                        for (std::size_t r0 = 0; r0 < S; ++r0) {
                            if (st.U_row.at(r0) == 0) { ++prefix_rows; } else { break; }
                        }
                        if (prefix_rows > 0) {
                            const LHChainVerifier verifier(seed);
                            if (!verifier.verify_rows(csm_out, lh, prefix_rows)) {
                                std::cerr << "LH prefix gating failed at rows=" << prefix_rows << "; restarting from baseline\n";
                                csm_out = baseline_csm;
                                st = baseline_st;
                                restart_triggered = true;
                                break; // break gi loop
                            }
                            if (!restart_triggered) {
                                baseline_csm = csm_out; // lock-in
                                baseline_st = st;
                            }
                        }
                    }
                    // Focused row completion: deep localized DE/backtrack to tip ≥99% rows to 100%
                    {
                        const auto near_thresh = static_cast<std::uint16_t>((S + 99U) / 100U);
                        bool improved = false;
                        static constexpr int kFocusMaxSteps = 32;
                        static constexpr int kFocusBtIters = 6000;
                        for (std::size_t r0 = 0; r0 < S && !improved; ++r0) {
                            if (st.U_row.at(r0) > 0 && st.U_row.at(r0) <= near_thresh) {
                                int steps = 0;
                                while (st.U_row.at(r0) > 0 && steps < kFocusMaxSteps) {
                                    ++steps;
                                    bool found = false; std::size_t c_pick = 0;
                                    for (std::size_t c0 = 0; c0 < S; ++c0) { if (!csm_out.is_locked(r0, c0)) { c_pick = c0; found = true; break; } }
                                    if (!found) { break; }
                                    bool adopted_step = false;
                                    const std::uint16_t before = st.U_row.at(r0);
                                    for (int vv = 0; vv < 2 && !adopted_step; ++vv) {
                                        const bool assume_one = (vv == 1);
                                        Csm c_try = csm_out;
                                        ConstraintState st_try = st;
                                        c_try.put(r0, c_pick, assume_one);
                                        c_try.lock(r0, c_pick);
                                        if (st_try.U_row.at(r0) > 0) { --st_try.U_row.at(r0); }
                                        if (st_try.U_col.at(c_pick) > 0) { --st_try.U_col.at(c_pick); }
                                        const std::size_t d0 = (c_pick >= r0) ? (c_pick - r0) : (c_pick + S - r0);
                                        const std::size_t x0 = (r0 + c_pick) % S;
                                        if (st_try.U_diag.at(d0) > 0) { --st_try.U_diag.at(d0); }
                                        if (st_try.U_xdiag.at(x0) > 0) { --st_try.U_xdiag.at(x0); }
                                        DeterministicElimination det_bt{c_try, st_try};
                                        for (int it0 = 0; it0 < kFocusBtIters; ++it0) {
                                            const std::size_t prog0 = det_bt.solve_step();
                                            if (st_try.U_row.at(r0) == 0) { break; }
                                            if (prog0 == 0) { break; }
                                        }
                                        if (st_try.U_row.at(r0) == 0 || st_try.U_row.at(r0) < before) {
                                            csm_out = c_try;
                                            st = st_try;
                                            adopted_step = true;
                                        }
                                    }
                                    if (!adopted_step) { break; }
                                }
                                if (steps > 0) { improved = true; }
                            }
                        }
                        if (improved) { for (int it1 = 0; it1 < 50 && !det.solved(); ++it1) { if (det.solve_step() == 0) { break; } } }
                    }
                    if (det.solved()) {
                        std::cerr << "GOBP converged to a solution\n";
                        break;
                    }
                    if (gprog == 0) {
                        std::cerr << "GOBP reached steady state (no progress)\n";
                        break;
                    }
                        if (restart_triggered) { break; }
                }
            }
            }

            // Backtracking: try a single high-uncertainty cell (always on)
            if (!det.solved()) {
                {
                    // Pick row with the most unknowns
                    std::size_t r_pick = 0;
                    std::uint16_t maxU = 0;
                    for (std::size_t r = 0; r < S; ++r) {
                        if (st.U_row.at(r) > maxU) { maxU = st.U_row.at(r); r_pick = r; }
                    }
                    // Find an unlocked column in that row
                    bool found = false; std::size_t c_pick = 0;
                    for (std::size_t c = 0; c < S; ++c) {
                        if (!csm_out.is_locked(r_pick, c)) { c_pick = c; found = true; break; }
                    }
                    if (found && maxU > 0) {
                        bool adopted = false;
                        for (int vv = 0; vv < 2 && !adopted; ++vv) {
                            const bool assume_one = (vv == 1);
                            Csm c_try = csm_out;
                            ConstraintState st_try = st;
                            c_try.put(r_pick, c_pick, assume_one);
                            c_try.lock(r_pick, c_pick);
                            if (st_try.U_row.at(r_pick) > 0) { --st_try.U_row.at(r_pick); }
                            if (st_try.U_col.at(c_pick) > 0) { --st_try.U_col.at(c_pick); }
                            const std::size_t d = (c_pick >= r_pick) ? (c_pick - r_pick) : (c_pick + S - r_pick);
                            const std::size_t x = (r_pick + c_pick) % S;
                            if (st_try.U_diag.at(d) > 0) { --st_try.U_diag.at(d); }
                            if (st_try.U_xdiag.at(x) > 0) { --st_try.U_xdiag.at(x); }
                            DeterministicElimination det_bt{c_try, st_try};
                            static constexpr int bt_iters = 1200;
                            for (int it = 0; it < bt_iters; ++it) {
                                const std::size_t prog = det_bt.solve_step();
                                if (det_bt.solved()) { break; }
                                if (prog == 0) { break; }
                            }
                            if (det_bt.solved()) {
                                csm_out = c_try;
                                st = st_try;
                                adopted = true;
                            }
                        }
                        if (adopted) {
                            // After adoption, run a quick DE sweep to stabilize
                            for (int it = 0; it < 50 && !det.solved(); ++it) {
                                if (det.solve_step() == 0) { break; }
                            }
                        }
                    }
                }
            }

            if (!det.solved()) {
                std::cerr << "Block Solver failed\n";
                snap.phase = "end-of-iterations";
                set_block_solve_snapshot(snap);
                return false;
            }
        }
        if (!verify_cross_sums(csm_out, lsm, vsm, dsm, xsm)) {
            std::cerr << "Block Solver failed to verify cross sums\n";
            snap.phase = "verify";
            snap.message = "cross-sum verification failed";
            set_block_solve_snapshot(snap);
            return false;
        }
        std::cerr << "Block Solver finished successfully (LH verification pending)\n";
        const LHChainVerifier verifier(seed);
        const bool result = verifier.verify_all(csm_out, lh);
        if (result) {
            std::cerr << "Block Solver finished successfully\n";
        } else {
            std::cerr << "Block Solver failed to verify cross sums\n";
        }
        return result;
    }
}
