/**
 * @file BlockSolver_hybrid.cpp
 * @brief Implementation of Hybrid Radditz‑Sift guided by GOBP beliefs.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 *
 * Deterministic 2x2 rectangle swaps that preserve row/col sums and accept
 * only when a combined DSM/XSM error + belief score improves. Updates
 * BlockSolveSnapshot hybrid metrics and respects simple env flags.
 */

#include "decompress/HashMatrix/LateralHashMatrix.h"
#include "decompress/CrossSum/DiagonalSumMatrix.h"
#include "decompress/CrossSum/AntiDiagonalSumMatrix.h"
#include "decompress/Block/detail/run_hybrid_sift.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <string>
#include <cstdint>

#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Utils/detail/index_calc_d.h"
#include "decompress/Utils/detail/index_calc_x.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Block/detail/get_block_solve_snapshot.h"
#include "decompress/Block/detail/set_block_solve_snapshot.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Block/detail/hybrid_read_env_i64.h"
#include "decompress/Block/detail/hybrid_read_env_f64.h"
#include "decompress/Block/detail/hybrid_clamp_need.h"
#include "common/O11y/O11y.h"

namespace crsce::decompress::detail {

/**
 * @name run_hybrid_sift
 * @brief Run the Hybrid Radditz Sift
 * @param csm
 * @param st
 * @param snap
 * @param lh
 * @param dsm
 * @param xsm
 * @return size_t
 */
std::size_t run_hybrid_sift(Csm &csm,
                            ConstraintState &st,
                            BlockSolveSnapshot &snap,
                            [[maybe_unused]] const ::crsce::decompress::hashes::LateralHashMatrix &lh,
                            const ::crsce::decompress::xsum::DiagonalSumMatrix &dsm_t,
                            const ::crsce::decompress::xsum::AntiDiagonalSumMatrix &xsm_t) {

    constexpr std::size_t S = Csm::kS;
    const auto t0 = std::chrono::steady_clock::now();
    const auto dsm = dsm_t.targets();
    const auto xsm = xsm_t.targets();

    // Phase labeling and status
    snap.phase = BlockSolveSnapshot::Phase::radditzSift; // reuse existing label for heartbeat
    snap.radditz_kind = 4; // 4 = hybrid
    ::crsce::o11y::O11y::instance().event("hybrid_sift_start");
    set_block_solve_snapshot(snap);

    const auto timeout_ms = read_env_i64("CRSCE_HYB_TIMEOUT_MS", 0);
    const auto max_passes = static_cast<int>(read_env_i64("CRSCE_HYB_MAX_PASSES", 8));
    const auto samples_per_pass = static_cast<unsigned>(read_env_i64("CRSCE_HYB_SAMPLES_PER_PASS", 4096));
    const auto accepts_per_pass = static_cast<unsigned>(read_env_i64("CRSCE_HYB_ACCEPTS_PER_PASS", 128));
    const auto sat_loss_min_improve = static_cast<unsigned>(read_env_i64("CRSCE_HYB_SAT_LOSS_MIN_IMPROVE", 2));
    const bool lock_dsm_sat = (read_env_i64("CRSCE_HYB_LOCK_DSM_SAT", 0) != 0);
    const bool lock_xsm_sat = (read_env_i64("CRSCE_HYB_LOCK_XSM_SAT", 0) != 0);
    const double alpha = read_env_f64("CRSCE_HYB_ALPHA", 1.0);
    const double beta  = read_env_f64("CRSCE_HYB_BETA", 1.0);

    // Make these visible in snapshot for downstream monitoring
    snap.lock_dsm_sat = lock_dsm_sat ? 1 : 0;
    snap.lock_xsm_sat = lock_xsm_sat ? 1 : 0;

    // Live diag/xdiag counts (local copy to avoid repeated CSM scans)
    std::array<std::uint16_t, S> dcnt{};
    std::array<std::uint16_t, S> xcnt{};
    for (std::size_t i = 0; i < S; ++i) {
        dcnt.at(i) = csm.count_dsm(i);
        xcnt.at(i) = csm.count_xsm(i);
    }

    const crsce::decompress::RowHashVerifier ver{};
    const auto wall_start = std::chrono::steady_clock::now();

    std::size_t total_accepted = 0;
    int passes = 0;
    bool improved = true;
    while (improved && (max_passes == 0 || passes < max_passes)) {
        improved = false;
        ++passes;
        std::size_t accepted = 0;
        // Deterministic rectangle sampling pattern
        for (unsigned s = 0; s < samples_per_pass && accepted < accepts_per_pass; ++s) {
            if (timeout_ms > 0) {
                const auto now = std::chrono::steady_clock::now();
                const auto el_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - wall_start).count();
                if (el_ms >= timeout_ms) {
                    ::crsce::o11y::O11y::instance().event("hybrid_sift_timeout");
                    goto HYB_DONE; // break out of both loops
                }
            }
            const std::size_t r0 = (static_cast<std::size_t>(s) * 73U) % S;
            const std::size_t r1 = (r0 + 1U + ((static_cast<std::size_t>(s) * 97U) % (S - 1U))) % S;
            const std::size_t c0 = (static_cast<std::size_t>(s) * 89U) % S;
            const std::size_t c1 = (c0 + 1U + ((static_cast<std::size_t>(s) * 131U) % (S - 1U))) % S;
            if (r0 == r1 || c0 == c1) { continue; }

            const bool b00 = (csm.get(r0, c0) != 0);
            const bool b11 = (csm.get(r1, c1) != 0);
            const bool b01 = (csm.get(r0, c1) != 0);
            const bool b10 = (csm.get(r1, c0) != 0);

            const bool main = (b00 && b11 && !b01 && !b10);
            const bool off  = (!b00 && !b11 && b01 && b10);
            if (!main && !off) { continue; }

            // Incidence indices
            const std::size_t d00 = ::crsce::decompress::detail::calc_d(r0, c0);
            const std::size_t d11 = ::crsce::decompress::detail::calc_d(r1, c1);
            const std::size_t d01 = ::crsce::decompress::detail::calc_d(r0, c1);
            const std::size_t d10 = ::crsce::decompress::detail::calc_d(r1, c0);
            const std::size_t x00 = ::crsce::decompress::detail::calc_x(r0, c0);
            const std::size_t x11 = ::crsce::decompress::detail::calc_x(r1, c1);
            const std::size_t x01 = ::crsce::decompress::detail::calc_x(r0, c1);
            const std::size_t x10 = ::crsce::decompress::detail::calc_x(r1, c0);

            // Local counts before and after hypothetical swap
            const auto d00_b = static_cast<std::int32_t>(dcnt.at(d00));
            const auto d11_b = static_cast<std::int32_t>(dcnt.at(d11));
            const auto d01_b = static_cast<std::int32_t>(dcnt.at(d01));
            const auto d10_b = static_cast<std::int32_t>(dcnt.at(d10));
            const auto x00_b = static_cast<std::int32_t>(xcnt.at(x00));
            const auto x11_b = static_cast<std::int32_t>(xcnt.at(x11));
            const auto x01_b = static_cast<std::int32_t>(xcnt.at(x01));
            const auto x10_b = static_cast<std::int32_t>(xcnt.at(x10));

            // After: toggle the two 1s → 0 and two 0s → 1
            const auto d00_n = d00_b + (main ? -1 : +1);
            const auto d11_n = d11_b + (main ? -1 : +1);
            const auto d01_n = d01_b + (main ? +1 : -1);
            const auto d10_n = d10_b + (main ? +1 : -1);
            const auto x00_n = x00_b + (main ? -1 : +1);
            const auto x11_n = x11_b + (main ? -1 : +1);
            const auto x01_n = x01_b + (main ? +1 : -1);
            const auto x10_n = x10_b + (main ? +1 : -1);

            // Absolute error to targets (local to affected families)
            const auto d00_t = static_cast<std::int32_t>(dsm[d00]);
            const auto d11_t = static_cast<std::int32_t>(dsm[d11]);
            const auto d01_t = static_cast<std::int32_t>(dsm[d01]);
            const auto d10_t = static_cast<std::int32_t>(dsm[d10]);
            const auto x00_t = static_cast<std::int32_t>(xsm[x00]);
            const auto x11_t = static_cast<std::int32_t>(xsm[x11]);
            const auto x01_t = static_cast<std::int32_t>(xsm[x01]);
            const auto x10_t = static_cast<std::int32_t>(xsm[x10]);

            const std::int32_t dx_before =
                std::abs(d00_b - d00_t) + std::abs(d11_b - d11_t) +
                std::abs(d01_b - d01_t) + std::abs(d10_b - d10_t) +
                std::abs(x00_b - x00_t) + std::abs(x11_b - x11_t) +
                std::abs(x01_b - x01_t) + std::abs(x10_b - x10_t);
            const std::int32_t dx_after =
                std::abs(d00_n - d00_t) + std::abs(d11_n - d11_t) +
                std::abs(d01_n - d01_t) + std::abs(d10_n - d10_t) +
                std::abs(x00_n - x00_t) + std::abs(x11_n - x11_t) +
                std::abs(x01_n - x01_t) + std::abs(x10_n - x10_t);

            // Satisfied family membership before/after (for lock + sat loss gating)
            const bool d00_sat_b = (d00_b == d00_t);
            const bool d11_sat_b = (d11_b == d11_t);
            const bool d01_sat_b = (d01_b == d01_t);
            const bool d10_sat_b = (d10_b == d10_t);
            const bool x00_sat_b = (x00_b == x00_t);
            const bool x11_sat_b = (x11_b == x11_t);
            const bool x01_sat_b = (x01_b == x01_t);
            const bool x10_sat_b = (x10_b == x10_t);
            const bool d00_sat_a = (d00_n == d00_t);
            const bool d11_sat_a = (d11_n == d11_t);
            const bool d01_sat_a = (d01_n == d01_t);
            const bool d10_sat_a = (d10_n == d10_t);
            const bool x00_sat_a = (x00_n == x00_t);
            const bool x11_sat_a = (x11_n == x11_t);
            const bool x01_sat_a = (x01_n == x01_t);
            const bool x10_sat_a = (x10_n == x10_t);
            const int sat_lost = (d00_sat_b && !d00_sat_a) + (d11_sat_b && !d11_sat_a) +
                                 (d01_sat_b && !d01_sat_a) + (d10_sat_b && !d10_sat_a) +
                                 (x00_sat_b && !x00_sat_a) + (x11_sat_b && !x11_sat_a) +
                                 (x01_sat_b && !x01_sat_a) + (x10_sat_b && !x10_sat_a);
            const int sat_gain = (!d00_sat_b && d00_sat_a) + (!d11_sat_b && d11_sat_a) +
                                 (!d01_sat_b && d01_sat_a) + (!d10_sat_b && d10_sat_a) +
                                 (!x00_sat_b && x00_sat_a) + (!x11_sat_b && x11_sat_a) +
                                 (!x01_sat_b && x01_sat_a) + (!x10_sat_b && x10_sat_a);

            // Family lock enforcement
            if ((lock_dsm_sat && ((d00_sat_b && !d00_sat_a) || (d11_sat_b && !d11_sat_a) || (d01_sat_b && !d01_sat_a) || (d10_sat_b && !d10_sat_a))) ||
                (lock_xsm_sat && ((x00_sat_b && !x00_sat_a) || (x11_sat_b && !x11_sat_a) || (x01_sat_b && !x01_sat_a) || (x10_sat_b && !x10_sat_a)))) {
                continue;
            }

            // Belief tie-breaker: prefer moving 1s towards higher belief cells
            double belief_before = 0.0;
            double belief_after = 0.0;
            if (main) {
                belief_before = csm.get_data(r0, c0) + csm.get_data(r1, c1);
                belief_after  = csm.get_data(r0, c1) + csm.get_data(r1, c0);
            } else {
                belief_before = csm.get_data(r0, c1) + csm.get_data(r1, c0);
                belief_after  = csm.get_data(r0, c0) + csm.get_data(r1, c1);
            }

            const double score_before = (alpha * static_cast<double>(dx_before)) - (beta * belief_before);
            const double score_after  = (alpha * static_cast<double>(dx_after))  - (beta * belief_after);

            bool accept = false;
            if (score_after < score_before) {
                // If breaking any satisfied diag/xdiag, require a minimum dx improvement
                if (sat_lost > 0) {
                    const std::int32_t improve = dx_before - dx_after;
                    const auto min_imp = static_cast<std::int32_t>(sat_loss_min_improve);
                    if (improve >= min_imp) {
                        accept = true;
                    }
                } else {
                    accept = true;
                }
            }
            if (!accept) { continue; }

            // Apply swap to CSM and update local counters
            if (main) {
                csm.clear(r0, c0); csm.clear(r1, c1);
                csm.set(r0, c1);   csm.set(r1, c0);
                dcnt.at(d00) = static_cast<std::uint16_t>(d00_n);
                dcnt.at(d11) = static_cast<std::uint16_t>(d11_n);
                dcnt.at(d01) = static_cast<std::uint16_t>(d01_n);
                dcnt.at(d10) = static_cast<std::uint16_t>(d10_n);
                xcnt.at(x00) = static_cast<std::uint16_t>(x00_n);
                xcnt.at(x11) = static_cast<std::uint16_t>(x11_n);
                xcnt.at(x01) = static_cast<std::uint16_t>(x01_n);
                xcnt.at(x10) = static_cast<std::uint16_t>(x10_n);
            } else {
                csm.clear(r0, c1); csm.clear(r1, c0);
                csm.set(r0, c0);   csm.set(r1, c1);
                dcnt.at(d00) = static_cast<std::uint16_t>(d00_n);
                dcnt.at(d11) = static_cast<std::uint16_t>(d11_n);
                dcnt.at(d01) = static_cast<std::uint16_t>(d01_n);
                dcnt.at(d10) = static_cast<std::uint16_t>(d10_n);
                xcnt.at(x00) = static_cast<std::uint16_t>(x00_n);
                xcnt.at(x11) = static_cast<std::uint16_t>(x11_n);
                xcnt.at(x01) = static_cast<std::uint16_t>(x01_n);
                xcnt.at(x10) = static_cast<std::uint16_t>(x10_n);
            }

            // Update snapshot for sat lost/gain and counts
            {
                auto cur = get_block_solve_snapshot();
                if (cur.has_value()) {
                    auto s2 = *cur;
                    s2.hyb_rect_attempts_total += 1;
                    s2.hyb_rect_accepts_total += 1;
                    s2.hyb_sat_lost += static_cast<std::size_t>(sat_lost);
                    s2.hyb_sat_gained += static_cast<std::size_t>(sat_gain);
                    set_block_solve_snapshot(s2);
                }
            }

            // Update residuals for only the touched diag/xdiag indices (clamp R ≤ U)
            
            {
                const std::uint16_t U_d00 = st.U_diag.at(d00);
                const std::uint16_t need_d00 = (dsm[d00] > dcnt.at(d00)) ? static_cast<std::uint16_t>(dsm[d00] - dcnt.at(d00)) : 0U;
                st.R_diag.at(d00) = clamp_need(need_d00, U_d00);
                const std::uint16_t U_d11 = st.U_diag.at(d11);
                const std::uint16_t need_d11 = (dsm[d11] > dcnt.at(d11)) ? static_cast<std::uint16_t>(dsm[d11] - dcnt.at(d11)) : 0U;
                st.R_diag.at(d11) = clamp_need(need_d11, U_d11);
                const std::uint16_t U_d01 = st.U_diag.at(d01);
                const std::uint16_t need_d01 = (dsm[d01] > dcnt.at(d01)) ? static_cast<std::uint16_t>(dsm[d01] - dcnt.at(d01)) : 0U;
                st.R_diag.at(d01) = clamp_need(need_d01, U_d01);
                const std::uint16_t U_d10 = st.U_diag.at(d10);
                const std::uint16_t need_d10 = (dsm[d10] > dcnt.at(d10)) ? static_cast<std::uint16_t>(dsm[d10] - dcnt.at(d10)) : 0U;
                st.R_diag.at(d10) = clamp_need(need_d10, U_d10);
                const std::uint16_t U_x00 = st.U_xdiag.at(x00);
                const std::uint16_t need_x00 = (xsm[x00] > xcnt.at(x00)) ? static_cast<std::uint16_t>(xsm[x00] - xcnt.at(x00)) : 0U;
                st.R_xdiag.at(x00) = clamp_need(need_x00, U_x00);
                const std::uint16_t U_x11 = st.U_xdiag.at(x11);
                const std::uint16_t need_x11 = (xsm[x11] > xcnt.at(x11)) ? static_cast<std::uint16_t>(xsm[x11] - xcnt.at(x11)) : 0U;
                st.R_xdiag.at(x11) = clamp_need(need_x11, U_x11);
                const std::uint16_t U_x01 = st.U_xdiag.at(x01);
                const std::uint16_t need_x01 = (xsm[x01] > xcnt.at(x01)) ? static_cast<std::uint16_t>(xsm[x01] - xcnt.at(x01)) : 0U;
                st.R_xdiag.at(x01) = clamp_need(need_x01, U_x01);
                const std::uint16_t U_x10 = st.U_xdiag.at(x10);
                const std::uint16_t need_x10 = (xsm[x10] > xcnt.at(x10)) ? static_cast<std::uint16_t>(xsm[x10] - xcnt.at(x10)) : 0U;
                st.R_xdiag.at(x10) = clamp_need(need_x10, U_x10);
            }

            ++accepted;
            improved = true;
        }
        total_accepted += accepted;
        // Publish periodic heartbeat
        snap.hyb_passes_last = static_cast<std::size_t>(passes);
        set_block_solve_snapshot(snap);
        if (!improved) { break; }
    }

HYB_DONE:
    const auto t1 = std::chrono::steady_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    snap.time_hybrid_ms += static_cast<std::size_t>(ms);
    snap.hyb_swaps_total += total_accepted;
    ::crsce::o11y::O11y::instance().event("hybrid_sift_end", {{"accepted", std::to_string(total_accepted)}});
    set_block_solve_snapshot(snap);
    return total_accepted;
}

} // namespace crsce::decompress::detail
