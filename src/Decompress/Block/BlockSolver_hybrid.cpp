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
#include <algorithm>
#include <limits>

#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Utils/detail/index_calc_d.h"
#include "decompress/Utils/detail/index_calc_x.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Block/detail/get_block_solve_snapshot.h"
#include "decompress/Block/detail/set_block_solve_snapshot.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Block/detail/hybrid_read_env_i64.h"
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
    const auto &vsm = snap.vsm; // column targets from snapshot

    // Phase labeling and status
    snap.phase = BlockSolveSnapshot::Phase::radditzSift; // reuse existing label for heartbeat
    snap.radditz_kind = 4; // 4 = hybrid
    ::crsce::o11y::O11y::instance().event("hybrid_sift_start");
    set_block_solve_snapshot(snap);

    const auto timeout_ms = read_env_i64("CRSCE_HYB_TIMEOUT_MS", 0);
    const auto max_passes = static_cast<int>(read_env_i64("CRSCE_HYB_MAX_PASSES", 65536));
    const auto samples_per_pass = static_cast<unsigned>(read_env_i64("CRSCE_HYB_SAMPLES_PER_PASS", 65536));
    const auto accepts_per_pass = static_cast<unsigned>(read_env_i64("CRSCE_HYB_ACCEPTS_PER_PASS", 1000000000));
    const bool lock_dsm_sat = (read_env_i64("CRSCE_HYB_LOCK_DSM_SAT", 0) != 0);
    const bool lock_xsm_sat = (read_env_i64("CRSCE_HYB_LOCK_XSM_SAT", 0) != 0);
    // Tunables for periodic targeted sweeps and stall handling
    const auto sweep_period = static_cast<unsigned>(read_env_i64("CRSCE_HYB_SWEEP_PERIOD", 16));
    const auto vsm_sweep_cap = static_cast<std::size_t>(read_env_i64("CRSCE_HYB_VSM_SWEEP_MAX_MOVES", 4096));
    const auto dsm_sweep_cap = static_cast<std::size_t>(read_env_i64("CRSCE_HYB_DSM_SWEEP_MAX_MOVES", 8192));
    const auto xsm_sweep_cap = static_cast<std::size_t>(read_env_i64("CRSCE_HYB_XSM_SWEEP_MAX_MOVES", 8192));
    const auto stall_flat_cap = static_cast<int>(read_env_i64("CRSCE_HYB_STALL_PASSES", 256));
    const auto stall_vsm_boost_cap = static_cast<std::size_t>(read_env_i64("CRSCE_HYB_STALL_VSM_BOOST_MAX_MOVES", 65536));

    // Make these visible in snapshot for downstream monitoring
    snap.lock_dsm_sat = lock_dsm_sat ? 1 : 0;
    snap.lock_xsm_sat = lock_xsm_sat ? 1 : 0;

    // Live diag/xdiag counts (local copy to avoid repeated CSM scans)
    std::array<std::uint16_t, S> dcnt{};
    std::array<std::uint16_t, S> xcnt{};
    std::array<std::uint16_t, S> vcnt{};
    for (std::size_t i = 0; i < S; ++i) {
        dcnt.at(i) = csm.count_dsm(i);
        xcnt.at(i) = csm.count_xsm(i);
        vcnt.at(i) = csm.count_vsm(i);
    }

    const crsce::decompress::RowHashVerifier ver{};
    const auto wall_start = std::chrono::steady_clock::now();

    std::size_t total_accepted = 0;
    std::int64_t prev_total_cost = std::numeric_limits<std::int64_t>::max();
    int stall_count = 0;
    int passes = 0;
    bool improved = true;
    while (improved && (max_passes == 0 || passes < max_passes)) {
        improved = false;
        ++passes;
        std::size_t accepted = 0;
        std::size_t attempts_this_pass = 0;

        // Publish pass start with light row stats (first few rows)
        {
            const std::size_t rows_probe = (S < 4 ? S : 4);
            // Compute ones/locked for first rows to aid debugging
            for (std::size_t rr = 0; rr < rows_probe; ++rr) {
                std::size_t ones = 0;
                std::size_t locked = 0;
                for (std::size_t cc = 0; cc < S; ++cc) {
                    if (csm.is_locked(rr, cc)) { ++locked; }
                    if (csm.get(rr, cc)) { ++ones; }
                }
                ::crsce::o11y::O11y::instance().event(
                    "hyb_row_stats",
                    {
                        {"row", std::to_string(rr)},
                        {"ones", std::to_string(ones)},
                        {"locked", std::to_string(locked)},
                        {"pass", std::to_string(passes)}
                    }
                );
            }
            ::crsce::o11y::O11y::instance().event(
                "hyb_pass_start",
                {
                    {"pass", std::to_string(passes)},
                    {"samples_per_pass", std::to_string(samples_per_pass)},
                    {"accepts_per_pass", std::to_string(accepts_per_pass)}
                }
            );
        }
        // Deterministic sampling pattern: first try a residual-driven lateral (in-row) swap to reduce VSM,
        // then try a rectangle swap to reduce DSM/XSM.
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

            // First: try in-row lateral swap (move one from cf to ct) to reduce VSM residuals
            {
                // Find cf (bit=1, unlocked) and ct (bit=0, unlocked, !=cf)
                std::size_t cf = c0; bool ok_cf = false;
                for (std::size_t it = 0; it < S; ++it) {
                    const std::size_t c = (cf + it) % S;
                    if (csm.is_locked(r0, c)) { continue; }
                    if (csm.get(r0, c)) { cf = c; ok_cf = true; break; }
                }
                if (ok_cf) {
                    std::size_t ct = c1; bool ok_ct = false;
                    for (std::size_t it = 0; it < S; ++it) {
                        const std::size_t c = (ct + it) % S;
                        if (c == cf) { continue; }
                        if (csm.is_locked(r0, c)) { continue; }
                        if (!csm.get(r0, c)) { ct = c; ok_ct = true; break; }
                    }
                    if (ok_ct) {
                        const std::size_t d_from = ::crsce::decompress::detail::calc_d(r0, cf);
                        const std::size_t x_from = ::crsce::decompress::detail::calc_x(r0, cf);
                        const std::size_t d_to   = ::crsce::decompress::detail::calc_d(r0, ct);
                        const std::size_t x_to   = ::crsce::decompress::detail::calc_x(r0, ct);

                        auto w = [](std::int32_t gap){ gap = std::abs(gap); return std::max<std::int32_t>(1, gap); };

                        // Before counts
                        const auto vcf_b   = static_cast<std::int32_t>(vcnt.at(cf));
                        const auto vct_b   = static_cast<std::int32_t>(vcnt.at(ct));
                        const auto dfrom_b = static_cast<std::int32_t>(dcnt.at(d_from));
                        const auto dto_b   = static_cast<std::int32_t>(dcnt.at(d_to));
                        const auto xfrom_b = static_cast<std::int32_t>(xcnt.at(x_from));
                        const auto xto_b   = static_cast<std::int32_t>(xcnt.at(x_to));

                        // Targets
                        const std::int32_t vcf_t = (cf < vsm.size()) ? static_cast<std::int32_t>(vsm[cf]) : 0;
                        const std::int32_t vct_t = (ct < vsm.size()) ? static_cast<std::int32_t>(vsm[ct]) : 0;
                        const auto dfrom_t = static_cast<std::int32_t>(dsm[d_from]);
                        const auto dto_t   = static_cast<std::int32_t>(dsm[d_to]);
                        const auto xfrom_t = static_cast<std::int32_t>(xsm[x_from]);
                        const auto xto_t   = static_cast<std::int32_t>(xsm[x_to]);

                        // We remove one from cf and add one to ct; update DSM/XSM accordingly
                        const std::int32_t vcf_n = vcf_b - 1;
                        const std::int32_t vct_n = vct_b + 1;
                        const std::int32_t dfrom_n = dfrom_b - 1;
                        const std::int32_t dto_n   = dto_b + 1;
                        const std::int32_t xfrom_n = xfrom_b - 1;
                        const std::int32_t xto_n   = xto_b + 1;

                        const std::int32_t w_vcf = w(vcf_b - vcf_t);
                        const std::int32_t w_vct = w(vct_b - vct_t);
                        const std::int32_t w_df  = w(dfrom_b - dfrom_t);
                        const std::int32_t w_dt  = w(dto_b - dto_t);
                        const std::int32_t w_xf  = w(xfrom_b - xfrom_t);
                        const std::int32_t w_xt  = w(xto_b - xto_t);

                        const std::int32_t cost_b =
                            (w_vcf * std::abs(vcf_b - vcf_t)) + (w_vct * std::abs(vct_b - vct_t)) +
                            (w_df  * std::abs(dfrom_b - dfrom_t)) + (w_dt  * std::abs(dto_b - dto_t)) +
                            (w_xf  * std::abs(xfrom_b - xfrom_t)) + (w_xt  * std::abs(xto_b - xto_t));
                        const std::int32_t cost_n =
                            (w_vcf * std::abs(vcf_n - vcf_t)) + (w_vct * std::abs(vct_n - vct_t)) +
                            (w_df  * std::abs(dfrom_n - dfrom_t)) + (w_dt  * std::abs(dto_n - dto_t)) +
                            (w_xf  * std::abs(xfrom_n - xfrom_t)) + (w_xt  * std::abs(xto_n - xto_t));

                        const double belief_b = csm.get_data(r0, cf);
                        const double belief_n = csm.get_data(r0, ct);
                        const bool accept_lat = (cost_n < cost_b) || ((cost_n == cost_b) && (belief_n > belief_b));
                        if (accept_lat) {
                            // apply
                            csm.clear(r0, cf); csm.set(r0, ct);
                            vcnt.at(cf) = static_cast<std::uint16_t>(vcf_n);
                            vcnt.at(ct) = static_cast<std::uint16_t>(vct_n);
                            dcnt.at(d_from) = static_cast<std::uint16_t>(dfrom_n);
                            dcnt.at(d_to)   = static_cast<std::uint16_t>(dto_n);
                            xcnt.at(x_from) = static_cast<std::uint16_t>(xfrom_n);
                            xcnt.at(x_to)   = static_cast<std::uint16_t>(xto_n);
                            ++accepted; improved = true; ++attempts_this_pass;
                            // Snapshot (best-effort)
                            if (auto cur = get_block_solve_snapshot(); cur.has_value()) {
                                auto s2 = *cur; s2.hyb_rect_accepts_total += 1; set_block_solve_snapshot(s2);
                            }
                            continue; // proceed next sample
                        }
                    }
                }
            }

            // Rectangle path (preserves VSM, drives DSM/XSM)
            const bool b00 = (csm.get(r0, c0) != 0);
            const bool b11 = (csm.get(r1, c1) != 0);
            const bool b01 = (csm.get(r0, c1) != 0);
            const bool b10 = (csm.get(r1, c0) != 0);

            const bool main = (b00 && b11 && !b01 && !b10);
            const bool off  = (!b00 && !b11 && b01 && b10);
            if (!main && !off) { continue; }

            // Count this rectangle evaluation attempt
            {
                auto cur = get_block_solve_snapshot();
                if (cur.has_value()) {
                    auto s2 = *cur;
                    s2.hyb_rect_attempts_total += 1;
                    set_block_solve_snapshot(s2);
                }
                ++attempts_this_pass;
            }

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

            // Residual-driven cost: error-weighted absolute residuals on affected DSM/XSM families
            const auto d00_t = static_cast<std::int32_t>(dsm[d00]);
            const auto d11_t = static_cast<std::int32_t>(dsm[d11]);
            const auto d01_t = static_cast<std::int32_t>(dsm[d01]);
            const auto d10_t = static_cast<std::int32_t>(dsm[d10]);
            const auto x00_t = static_cast<std::int32_t>(xsm[x00]);
            const auto x11_t = static_cast<std::int32_t>(xsm[x11]);
            const auto x01_t = static_cast<std::int32_t>(xsm[x01]);
            const auto x10_t = static_cast<std::int32_t>(xsm[x10]);

            auto w = [](std::int32_t gap){ gap = std::abs(gap); return std::max<std::int32_t>(1, gap); };
            const std::int32_t d00_w = w(d00_b - d00_t);
            const std::int32_t d11_w = w(d11_b - d11_t);
            const std::int32_t d01_w = w(d01_b - d01_t);
            const std::int32_t d10_w = w(d10_b - d10_t);
            const std::int32_t x00_w = w(x00_b - x00_t);
            const std::int32_t x11_w = w(x11_b - x11_t);
            const std::int32_t x01_w = w(x01_b - x01_t);
            const std::int32_t x10_w = w(x10_b - x10_t);

            const std::int32_t cost_before =
                (d00_w * std::abs(d00_b - d00_t)) + (d11_w * std::abs(d11_b - d11_t)) +
                (d01_w * std::abs(d01_b - d01_t)) + (d10_w * std::abs(d10_b - d10_t)) +
                (x00_w * std::abs(x00_b - x00_t)) + (x11_w * std::abs(x11_b - x11_t)) +
                (x01_w * std::abs(x01_b - x01_t)) + (x10_w * std::abs(x10_b - x10_t));
            const std::int32_t cost_after =
                (d00_w * std::abs(d00_n - d00_t)) + (d11_w * std::abs(d11_n - d11_t)) +
                (d01_w * std::abs(d01_n - d01_t)) + (d10_w * std::abs(d10_n - d10_t)) +
                (x00_w * std::abs(x00_n - x00_t)) + (x11_w * std::abs(x11_n - x11_t)) +
                (x01_w * std::abs(x01_n - x01_t)) + (x10_w * std::abs(x10_n - x10_t));

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

            // Family lock enforcement (optional via env)
            if ((lock_dsm_sat && ((d00_sat_b && !d00_sat_a) || (d11_sat_b && !d11_sat_a) || (d01_sat_b && !d01_sat_a) || (d10_sat_b && !d10_sat_a))) ||
                (lock_xsm_sat && ((x00_sat_b && !x00_sat_a) || (x11_sat_b && !x11_sat_a) || (x01_sat_b && !x01_sat_a) || (x10_sat_b && !x10_sat_a)))) {
                continue;
            }

            // Residual-driven acceptance: strictly decrease cost; if tied, prefer higher belief
            double belief_before = 0.0;
            double belief_after = 0.0;
            if (main) {
                belief_before = csm.get_data(r0, c0) + csm.get_data(r1, c1);
                belief_after  = csm.get_data(r0, c1) + csm.get_data(r1, c0);
            } else {
                belief_before = csm.get_data(r0, c1) + csm.get_data(r1, c0);
                belief_after  = csm.get_data(r0, c0) + csm.get_data(r1, c1);
            }

            const bool accept = (cost_after < cost_before) ||
                                ((cost_after == cost_before) && (belief_after > belief_before));
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

            // Update snapshot for sat lost/gain and accepted count
            {
                auto cur = get_block_solve_snapshot();
                if (cur.has_value()) {
                    auto s2 = *cur;
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

        // Worst-family targeted local sweep every N passes: push VSM and DSM/XSM on worst offenders.
        if (sweep_period != 0U && (static_cast<unsigned>(passes) % sweep_period) == 0U) {
            // Target VSM: move from worst overfull column to worst underfull when possible
            std::size_t over_c = 0;
            std::size_t under_c = 0;
            int over_gap = 0;
            int under_gap = 0;
            for (std::size_t i = 0; i < S; ++i) {
                const int gap = static_cast<int>(vcnt.at(i)) - static_cast<int>(vsm[i]);
                if (gap > over_gap) { over_gap = gap; over_c = i; }
                if (gap < under_gap) { under_gap = gap; under_c = i; }
            }
            if (over_gap > 0 && under_gap < 0 && over_c != under_c) {
                std::size_t improved_moves = 0;
                const std::size_t max_moves = vsm_sweep_cap;
                for (std::size_t r = 0; r < S && improved_moves < max_moves; ++r) {
                    if (!csm.get(r, over_c) || csm.is_locked(r, over_c)) { continue; }
                    if (csm.get(r, under_c) || csm.is_locked(r, under_c)) { continue; }
                    // Evaluate cost delta (reuse residual-driven acceptance used above)
                    auto w = [](int g){ g = std::abs(g); return std::max(1, g); };
                    const int v_over_b = static_cast<int>(vcnt.at(over_c));
                    const int v_under_b= static_cast<int>(vcnt.at(under_c));
                    const int v_over_t = static_cast<int>(vsm[over_c]);
                    const int v_under_t= static_cast<int>(vsm[under_c]);
                    const int d_from   = static_cast<int>(::crsce::decompress::detail::calc_d(r, over_c));
                    const int x_from   = static_cast<int>(::crsce::decompress::detail::calc_x(r, over_c));
                    const int d_to     = static_cast<int>(::crsce::decompress::detail::calc_d(r, under_c));
                    const int x_to     = static_cast<int>(::crsce::decompress::detail::calc_x(r, under_c));
                    const int d_from_b = static_cast<int>(dcnt.at(d_from));
                    const int d_to_b   = static_cast<int>(dcnt.at(d_to));
                    const int x_from_b = static_cast<int>(xcnt.at(x_from));
                    const int x_to_b   = static_cast<int>(xcnt.at(x_to));
                    const int d_from_t = static_cast<int>(dsm[d_from]);
                    const int d_to_t   = static_cast<int>(dsm[d_to]);
                    const int x_from_t = static_cast<int>(xsm[x_from]);
                    const int x_to_t   = static_cast<int>(xsm[x_to]);

                    const int cost_b = (w(v_over_b - v_over_t) * std::abs(v_over_b - v_over_t)) +
                                       (w(v_under_b - v_under_t) * std::abs(v_under_b - v_under_t)) +
                                       (w(d_from_b - d_from_t) * std::abs(d_from_b - d_from_t)) +
                                       (w(d_to_b - d_to_t) * std::abs(d_to_b - d_to_t)) +
                                       (w(x_from_b - x_from_t) * std::abs(x_from_b - x_from_t)) +
                                       (w(x_to_b - x_to_t) * std::abs(x_to_b - x_to_t));
                    const int v_over_n = v_over_b - 1;
                    const int v_under_n= v_under_b + 1;
                    const int d_from_n = d_from_b - 1;
                    const int d_to_n   = d_to_b + 1;
                    const int x_from_n = x_from_b - 1;
                    const int x_to_n   = x_to_b + 1;
                    const int cost_n = (w(v_over_n - v_over_t) * std::abs(v_over_n - v_over_t)) +
                                       (w(v_under_n - v_under_t) * std::abs(v_under_n - v_under_t)) +
                                       (w(d_from_n - d_from_t) * std::abs(d_from_n - d_from_t)) +
                                       (w(d_to_n - d_to_t) * std::abs(d_to_n - d_to_t)) +
                                       (w(x_from_n - x_from_t) * std::abs(x_from_n - x_from_t)) +
                                       (w(x_to_n - x_to_t) * std::abs(x_to_n - x_to_t));
                    if (cost_n < cost_b) {
                        csm.clear(r, over_c); csm.set(r, under_c);
                        vcnt.at(over_c) = static_cast<std::uint16_t>(v_over_n);
                        vcnt.at(under_c)= static_cast<std::uint16_t>(v_under_n);
                        dcnt.at(d_from) = static_cast<std::uint16_t>(d_from_n);
                        dcnt.at(d_to)   = static_cast<std::uint16_t>(d_to_n);
                        xcnt.at(x_from) = static_cast<std::uint16_t>(x_from_n);
                        xcnt.at(x_to)   = static_cast<std::uint16_t>(x_to_n);
                        ++improved_moves; ++total_accepted; improved = true;
                    }
                }
                if (improved_moves > 0) {
                    ::crsce::o11y::O11y::instance().event("hyb_targeted_vsm", {{"moves", std::to_string(improved_moves)},
                                                                                {"over", std::to_string(over_c)},
                                                                                {"under", std::to_string(under_c)},
                                                                                {"pass", std::to_string(passes)}});
                }
            }
            // DSM targeted sweep: rectangles touching worst DSM index
            {
                std::size_t improved = 0;
                const std::size_t max_moves = dsm_sweep_cap;
                // Compute current worst DSM index locally (largest absolute gap)
                std::size_t dstar = 0;
                {
                    std::int32_t worst_gap = 0;
                    for (std::size_t i = 0; i < S; ++i) {
                        const std::int32_t g = static_cast<std::int32_t>(dcnt.at(i)) - static_cast<std::int32_t>(dsm[i]);
                        if (std::abs(g) > std::abs(worst_gap)) { worst_gap = g; dstar = i; }
                    }
                }
                for (std::size_t t = 0; t < S && improved < max_moves; ++t) {
                    const std::size_t r0 = (t * 73U) % S;
                    const std::size_t r1 = (r0 + 1U + ((t * 97U) % (S - 1U))) % S;
                    const std::size_t c0 = (r0 + dstar) % S;
                    const std::size_t c1 = (r1 + dstar) % S;
                    if (r0 == r1 || c0 == c1) { continue; }
                    const bool b00 = (csm.get(r0, c0) != 0);
                    const bool b11 = (csm.get(r1, c1) != 0);
                    const bool b01 = (csm.get(r0, c1) != 0);
                    const bool b10 = (csm.get(r1, c0) != 0);
                    const bool main = (b00 && b11 && !b01 && !b10);
                    const bool off  = (!b00 && !b11 && b01 && b10);
                    if ((!main && !off) || csm.is_locked(r0,c0) || csm.is_locked(r0,c1) || csm.is_locked(r1,c0) || csm.is_locked(r1,c1)) { continue; }
                    const std::size_t d00 = ::crsce::decompress::detail::calc_d(r0, c0);
                    const std::size_t d11 = ::crsce::decompress::detail::calc_d(r1, c1);
                    const std::size_t d01 = ::crsce::decompress::detail::calc_d(r0, c1);
                    const std::size_t d10 = ::crsce::decompress::detail::calc_d(r1, c0);
                    const std::size_t x00 = ::crsce::decompress::detail::calc_x(r0, c0);
                    const std::size_t x11 = ::crsce::decompress::detail::calc_x(r1, c1);
                    const std::size_t x01 = ::crsce::decompress::detail::calc_x(r0, c1);
                    const std::size_t x10 = ::crsce::decompress::detail::calc_x(r1, c0);
                    const auto d00_b = static_cast<std::int32_t>(dcnt.at(d00));
                    const auto d11_b = static_cast<std::int32_t>(dcnt.at(d11));
                    const auto d01_b = static_cast<std::int32_t>(dcnt.at(d01));
                    const auto d10_b = static_cast<std::int32_t>(dcnt.at(d10));
                    const auto x00_b = static_cast<std::int32_t>(xcnt.at(x00));
                    const auto x11_b = static_cast<std::int32_t>(xcnt.at(x11));
                    const auto x01_b = static_cast<std::int32_t>(xcnt.at(x01));
                    const auto x10_b = static_cast<std::int32_t>(xcnt.at(x10));
                    const auto d00_t = static_cast<std::int32_t>(dsm[d00]);
                    const auto d11_t = static_cast<std::int32_t>(dsm[d11]);
                    const auto d01_t = static_cast<std::int32_t>(dsm[d01]);
                    const auto d10_t = static_cast<std::int32_t>(dsm[d10]);
                    const auto x00_t = static_cast<std::int32_t>(xsm[x00]);
                    const auto x11_t = static_cast<std::int32_t>(xsm[x11]);
                    const auto x01_t = static_cast<std::int32_t>(xsm[x01]);
                    const auto x10_t = static_cast<std::int32_t>(xsm[x10]);
                    auto w = [](std::int32_t g){ g = std::abs(g); return std::max<std::int32_t>(1,g); };
                    const std::int32_t d00_w = w(d00_b - d00_t);
                    const std::int32_t d11_w = w(d11_b - d11_t);
                    const std::int32_t d01_w = w(d01_b - d01_t);
                    const std::int32_t d10_w = w(d10_b - d10_t);
                    const std::int32_t x00_w = w(x00_b - x00_t);
                    const std::int32_t x11_w = w(x11_b - x11_t);
                    const std::int32_t x01_w = w(x01_b - x01_t);
                    const std::int32_t x10_w = w(x10_b - x10_t);
                    const auto d00_n = d00_b + (main ? -1 : +1);
                    const auto d11_n = d11_b + (main ? -1 : +1);
                    const auto d01_n = d01_b + (main ? +1 : -1);
                    const auto d10_n = d10_b + (main ? +1 : -1);
                    const auto x00_n = x00_b + (main ? -1 : +1);
                    const auto x11_n = x11_b + (main ? -1 : +1);
                    const auto x01_n = x01_b + (main ? +1 : -1);
                    const auto x10_n = x10_b + (main ? +1 : -1);
                    const std::int32_t cost_b =
                        (d00_w * std::abs(d00_b - d00_t)) + (d11_w * std::abs(d11_b - d11_t)) +
                        (d01_w * std::abs(d01_b - d01_t)) + (d10_w * std::abs(d10_b - d10_t)) +
                        (x00_w * std::abs(x00_b - x00_t)) + (x11_w * std::abs(x11_b - x11_t)) +
                        (x01_w * std::abs(x01_b - x01_t)) + (x10_w * std::abs(x10_b - x10_t));
                    const std::int32_t cost_n =
                        (d00_w * std::abs(d00_n - d00_t)) + (d11_w * std::abs(d11_n - d11_t)) +
                        (d01_w * std::abs(d01_n - d01_t)) + (d10_w * std::abs(d10_n - d10_t)) +
                        (x00_w * std::abs(x00_n - x00_t)) + (x11_w * std::abs(x11_n - x11_t)) +
                        (x01_w * std::abs(x01_n - x01_t)) + (x10_w * std::abs(x10_n - x10_t));
                    if (cost_n < cost_b) {
                        // apply
                        if (main) { csm.clear(r0, c0); csm.clear(r1, c1); csm.set(r0, c1); csm.set(r1, c0); }
                        else { csm.clear(r0, c1); csm.clear(r1, c0); csm.set(r0, c0); csm.set(r1, c1); }
                        dcnt.at(d00) = static_cast<std::uint16_t>(d00_n);
                        dcnt.at(d11) = static_cast<std::uint16_t>(d11_n);
                        dcnt.at(d01) = static_cast<std::uint16_t>(d01_n);
                        dcnt.at(d10) = static_cast<std::uint16_t>(d10_n);
                        xcnt.at(x00) = static_cast<std::uint16_t>(x00_n);
                        xcnt.at(x11) = static_cast<std::uint16_t>(x11_n);
                        xcnt.at(x01) = static_cast<std::uint16_t>(x01_n);
                        xcnt.at(x10) = static_cast<std::uint16_t>(x10_n);
                        ++improved;
                    }
                }
                if (improved > 0) {
                    ::crsce::o11y::O11y::instance().event("hyb_targeted_dsm", {{"moves", std::to_string(improved)},
                                                                                {"diag", std::to_string(dstar)},
                                                                                {"pass", std::to_string(passes)}});
                }
            }
            // XSM targeted sweep: rectangles touching worst XSM index
            {
                std::size_t improved = 0;
                const std::size_t max_moves = xsm_sweep_cap;
                // Compute current worst XSM index locally (largest absolute gap)
                std::size_t xstar = 0;
                {
                    std::int32_t worst_gap = 0;
                    for (std::size_t i = 0; i < S; ++i) {
                        const std::int32_t g = static_cast<std::int32_t>(xcnt.at(i)) - static_cast<std::int32_t>(xsm[i]);
                        if (std::abs(g) > std::abs(worst_gap)) { worst_gap = g; xstar = i; }
                    }
                }
                for (std::size_t t = 0; t < S && improved < max_moves; ++t) {
                    const std::size_t r0 = (t * 89U) % S;
                    const std::size_t r1 = (r0 + 1U + ((t * 131U) % (S - 1U))) % S;
                    const std::size_t c0 = (xstar + S - (r0 % S)) % S;
                    const std::size_t c1 = (xstar + S - (r1 % S)) % S;
                    if (r0 == r1 || c0 == c1) { continue; }
                    const bool b00 = (csm.get(r0, c0) != 0);
                    const bool b11 = (csm.get(r1, c1) != 0);
                    const bool b01 = (csm.get(r0, c1) != 0);
                    const bool b10 = (csm.get(r1, c0) != 0);
                    const bool main = (b00 && b11 && !b01 && !b10);
                    const bool off  = (!b00 && !b11 && b01 && b10);
                    if ((!main && !off) || csm.is_locked(r0,c0) || csm.is_locked(r0,c1) || csm.is_locked(r1,c0) || csm.is_locked(r1,c1)) { continue; }
                    const std::size_t d00 = ::crsce::decompress::detail::calc_d(r0, c0);
                    const std::size_t d11 = ::crsce::decompress::detail::calc_d(r1, c1);
                    const std::size_t d01 = ::crsce::decompress::detail::calc_d(r0, c1);
                    const std::size_t d10 = ::crsce::decompress::detail::calc_d(r1, c0);
                    const std::size_t x00 = ::crsce::decompress::detail::calc_x(r0, c0);
                    const std::size_t x11 = ::crsce::decompress::detail::calc_x(r1, c1);
                    const std::size_t x01 = ::crsce::decompress::detail::calc_x(r0, c1);
                    const std::size_t x10 = ::crsce::decompress::detail::calc_x(r1, c0);
                    const auto d00_b = static_cast<std::int32_t>(dcnt.at(d00));
                    const auto d11_b = static_cast<std::int32_t>(dcnt.at(d11));
                    const auto d01_b = static_cast<std::int32_t>(dcnt.at(d01));
                    const auto d10_b = static_cast<std::int32_t>(dcnt.at(d10));
                    const auto x00_b = static_cast<std::int32_t>(xcnt.at(x00));
                    const auto x11_b = static_cast<std::int32_t>(xcnt.at(x11));
                    const auto x01_b = static_cast<std::int32_t>(xcnt.at(x01));
                    const auto x10_b = static_cast<std::int32_t>(xcnt.at(x10));
                    const auto d00_t = static_cast<std::int32_t>(dsm[d00]);
                    const auto d11_t = static_cast<std::int32_t>(dsm[d11]);
                    const auto d01_t = static_cast<std::int32_t>(dsm[d01]);
                    const auto d10_t = static_cast<std::int32_t>(dsm[d10]);
                    const auto x00_t = static_cast<std::int32_t>(xsm[x00]);
                    const auto x11_t = static_cast<std::int32_t>(xsm[x11]);
                    const auto x01_t = static_cast<std::int32_t>(xsm[x01]);
                    const auto x10_t = static_cast<std::int32_t>(xsm[x10]);
                    auto w = [](std::int32_t g){ g = std::abs(g); return std::max<std::int32_t>(1,g); };
                    const std::int32_t d00_w = w(d00_b - d00_t);
                    const std::int32_t d11_w = w(d11_b - d11_t);
                    const std::int32_t d01_w = w(d01_b - d01_t);
                    const std::int32_t d10_w = w(d10_b - d10_t);
                    const std::int32_t x00_w = w(x00_b - x00_t);
                    const std::int32_t x11_w = w(x11_b - x11_t);
                    const std::int32_t x01_w = w(x01_b - x01_t);
                    const std::int32_t x10_w = w(x10_b - x10_t);
                    const auto d00_n = d00_b + (main ? -1 : +1);
                    const auto d11_n = d11_b + (main ? -1 : +1);
                    const auto d01_n = d01_b + (main ? +1 : -1);
                    const auto d10_n = d10_b + (main ? +1 : -1);
                    const auto x00_n = x00_b + (main ? -1 : +1);
                    const auto x11_n = x11_b + (main ? -1 : +1);
                    const auto x01_n = x01_b + (main ? +1 : -1);
                    const auto x10_n = x10_b + (main ? +1 : -1);
                    const std::int32_t cost_b =
                        (d00_w * std::abs(d00_b - d00_t)) + (d11_w * std::abs(d11_b - d11_t)) +
                        (d01_w * std::abs(d01_b - d01_t)) + (d10_w * std::abs(d10_b - d10_t)) +
                        (x00_w * std::abs(x00_b - x00_t)) + (x11_w * std::abs(x11_b - x11_t)) +
                        (x01_w * std::abs(x01_b - x01_t)) + (x10_w * std::abs(x10_b - x10_t));
                    const std::int32_t cost_n =
                        (d00_w * std::abs(d00_n - d00_t)) + (d11_w * std::abs(d11_n - d11_t)) +
                        (d01_w * std::abs(d01_n - d01_t)) + (d10_w * std::abs(d10_n - d10_t)) +
                        (x00_w * std::abs(x00_n - x00_t)) + (x11_w * std::abs(x11_n - x11_t)) +
                        (x01_w * std::abs(x01_n - x01_t)) + (x10_w * std::abs(x10_n - x10_t));
                    if (cost_n < cost_b) {
                        if (main) { csm.clear(r0, c0); csm.clear(r1, c1); csm.set(r0, c1); csm.set(r1, c0); }
                        else { csm.clear(r0, c1); csm.clear(r1, c0); csm.set(r0, c0); csm.set(r1, c1); }
                        dcnt.at(d00) = static_cast<std::uint16_t>(d00_n);
                        dcnt.at(d11) = static_cast<std::uint16_t>(d11_n);
                        dcnt.at(d01) = static_cast<std::uint16_t>(d01_n);
                        dcnt.at(d10) = static_cast<std::uint16_t>(d10_n);
                        xcnt.at(x00) = static_cast<std::uint16_t>(x00_n);
                        xcnt.at(x11) = static_cast<std::uint16_t>(x11_n);
                        xcnt.at(x01) = static_cast<std::uint16_t>(x01_n);
                        xcnt.at(x10) = static_cast<std::uint16_t>(x10_n);
                        ++improved;
                    }
                }
                if (improved > 0) {
                    ::crsce::o11y::O11y::instance().event("hyb_targeted_xsm", {{"moves", std::to_string(improved)},
                                                                                {"xdiag", std::to_string(xstar)},
                                                                                {"pass", std::to_string(passes)}});
                }
            }
        }
        total_accepted += accepted;
        // Compute whole-matrix residuals and worst offenders for observability
        std::int64_t total_cost = 0;
        std::size_t worst_v_idx = 0;
        std::size_t worst_d_idx = 0;
        std::size_t worst_x_idx = 0;
        std::int32_t worst_v_err = 0;
        std::int32_t worst_d_err = 0;
        std::int32_t worst_x_err = 0;
        auto upd_worst = [](std::int32_t e, std::int32_t &w, std::size_t i, std::size_t &widx){ if (std::abs(e) > std::abs(w)) { w = e; widx = i; } };
        for (std::size_t i = 0; i < S; ++i) {
            const auto vgap = static_cast<std::int32_t>(vcnt.at(i)) - static_cast<std::int32_t>(vsm[i]);
            const auto dgap = static_cast<std::int32_t>(dcnt.at(i)) - static_cast<std::int32_t>(dsm[i]);
            const auto xgap = static_cast<std::int32_t>(xcnt.at(i)) - static_cast<std::int32_t>(xsm[i]);
            const auto wv = std::max<std::int32_t>(1, std::abs(vgap));
            const auto wd = std::max<std::int32_t>(1, std::abs(dgap));
            const auto wx = std::max<std::int32_t>(1, std::abs(xgap));
            total_cost += static_cast<std::int64_t>(wv) * std::abs(vgap);
            total_cost += static_cast<std::int64_t>(wd) * std::abs(dgap);
            total_cost += static_cast<std::int64_t>(wx) * std::abs(xgap);
            upd_worst(vgap, worst_v_err, i, worst_v_idx);
            upd_worst(dgap, worst_d_err, i, worst_d_idx);
            upd_worst(xgap, worst_x_err, i, worst_x_idx);
        }
        ::crsce::o11y::O11y::instance().event("hyb_residuals", {{"worst_vsm_error", std::to_string(worst_v_err)},
                                                                 {"worst_vsm_index", std::to_string(worst_v_idx)},
                                                                 {"worst_dsm_error", std::to_string(worst_d_err)},
                                                                 {"worst_dsm_index", std::to_string(worst_d_idx)},
                                                                 {"worst_xsm_error", std::to_string(worst_x_err)},
                                                                 {"worst_xsm_index", std::to_string(worst_x_idx)},
                                                                 {"total_cost", std::to_string(total_cost)},
                                                                 {"pass", std::to_string(passes)}});
        ::crsce::o11y::O11y::instance().event("hyb_accepts_pass", {{"accepted", std::to_string(accepted)},
                                                                    {"attempts", std::to_string(attempts_this_pass)},
                                                                    {"pass", std::to_string(passes)}});
        // Update point-in-time DSM/XSM error stats for heartbeat/debugging
        {
            std::uint16_t dmin = std::numeric_limits<std::uint16_t>::max();
            std::uint16_t dmax = 0;
            std::uint16_t xmin = std::numeric_limits<std::uint16_t>::max();
            std::uint16_t xmax = 0;
            for (std::size_t i = 0; i < S; ++i) {
                const auto de = static_cast<std::uint16_t>(
                    (dcnt.at(i) > dsm[i]) ? (dcnt.at(i) - dsm[i]) : (dsm[i] - dcnt.at(i))
                );
                const auto xe = static_cast<std::uint16_t>(
                    (xcnt.at(i) > xsm[i]) ? (xcnt.at(i) - xsm[i]) : (xsm[i] - xcnt.at(i))
                );
                dmin = std::min(dmin, de); dmax = std::max(dmax, de);
                xmin = std::min(xmin, xe); xmax = std::max(xmax, xe);
            }
            snap.dsm_err_min = dmin; snap.dsm_err_max = dmax;
            // Percent satisfied (families exactly matching target)
            std::size_t d_ok = 0;
            std::size_t x_ok = 0;
            for (std::size_t i = 0; i < S; ++i) { if (dcnt.at(i) == dsm[i]) { ++d_ok; } if (xcnt.at(i) == xsm[i]) { ++x_ok; } }
            snap.dsm_avg_pct = static_cast<int>((static_cast<double>(d_ok) / static_cast<double>(S)) * 100.0);
            ::crsce::o11y::O11y::instance().metric("hyb_dsm_err_min", static_cast<std::int64_t>(dmin), {{"pass", std::to_string(passes)}});
            ::crsce::o11y::O11y::instance().metric("hyb_dsm_err_max", static_cast<std::int64_t>(dmax), {{"pass", std::to_string(passes)}});
            ::crsce::o11y::O11y::instance().metric("hyb_dsm_sat_pct", static_cast<std::int64_t>(snap.dsm_avg_pct), {{"pass", std::to_string(passes)}});
        }
        // Publish per-pass attempts/accepts
        ::crsce::o11y::O11y::instance().metric("hyb_pass_attempts", static_cast<std::int64_t>(attempts_this_pass), {{"pass", std::to_string(passes)}});
        ::crsce::o11y::O11y::instance().metric("hyb_pass_accepts", static_cast<std::int64_t>(accepted), {{"pass", std::to_string(passes)}});

        // No-regression gate: if total_cost is unchanged/not decreasing for many passes, run intensified targeted sweeps
        if (total_cost < prev_total_cost) {
            prev_total_cost = total_cost;
            stall_count = 0;
        } else {
            ++stall_count;
            if (stall_count >= stall_flat_cap) {
                std::size_t boosts = 0;
                // Intensify VSM sweep
                // reuse worst indices; do larger moves cap
                std::size_t over_c = 0;
                std::size_t under_c = 0;
                int over_gap = 0;
                int under_gap = 0;
                for (std::size_t i = 0; i < S; ++i) {
                    const int gap = static_cast<int>(vcnt.at(i)) - static_cast<int>(vsm[i]);
                    if (gap > over_gap) { over_gap = gap; over_c = i; }
                    if (gap < under_gap) { under_gap = gap; under_c = i; }
                }
                if (over_gap > 0 && under_gap < 0 && over_c != under_c) {
                    std::size_t improved_moves = 0; const std::size_t max_moves = stall_vsm_boost_cap;
                    for (std::size_t r = 0; r < S && improved_moves < max_moves; ++r) {
                        if (!csm.get(r, over_c) || csm.is_locked(r, over_c)) { continue; }
                        if (csm.get(r, under_c) || csm.is_locked(r, under_c)) { continue; }
                        auto w = [](int g){ g = std::abs(g); return std::max(1, g); };
                        const int v_over_b = static_cast<int>(vcnt.at(over_c)); const int v_under_b= static_cast<int>(vcnt.at(under_c));
                        const int v_over_t = static_cast<int>(vsm[over_c]);   const int v_under_t= static_cast<int>(vsm[under_c]);
                        const int d_from   = static_cast<int>(::crsce::decompress::detail::calc_d(r, over_c));
                        const int x_from   = static_cast<int>(::crsce::decompress::detail::calc_x(r, over_c));
                        const int d_to     = static_cast<int>(::crsce::decompress::detail::calc_d(r, under_c));
                        const int x_to     = static_cast<int>(::crsce::decompress::detail::calc_x(r, under_c));
                        const int d_from_b = static_cast<int>(dcnt.at(d_from)); const int d_to_b   = static_cast<int>(dcnt.at(d_to));
                        const int x_from_b = static_cast<int>(xcnt.at(x_from)); const int x_to_b   = static_cast<int>(xcnt.at(x_to));
                        const int d_from_t = static_cast<int>(dsm[d_from]);     const int d_to_t   = static_cast<int>(dsm[d_to]);
                        const int x_from_t = static_cast<int>(xsm[x_from]);     const int x_to_t   = static_cast<int>(xsm[x_to]);
                        const int cost_b = (w(v_over_b - v_over_t) * std::abs(v_over_b - v_over_t)) +
                                           (w(v_under_b - v_under_t) * std::abs(v_under_b - v_under_t)) +
                                           (w(d_from_b - d_from_t) * std::abs(d_from_b - d_from_t)) + (w(d_to_b - d_to_t) * std::abs(d_to_b - d_to_t)) +
                                           (w(x_from_b - x_from_t) * std::abs(x_from_b - x_from_t)) + (w(x_to_b - x_to_t) * std::abs(x_to_b - x_to_t));
                        const int v_over_n = v_over_b - 1;
                        const int v_under_n = v_under_b + 1;
                        const int d_from_n = d_from_b - 1;
                        const int d_to_n = d_to_b + 1;
                        const int x_from_n = x_from_b - 1;
                        const int x_to_n = x_to_b + 1;
                        const int cost_n = (w(v_over_n - v_over_t) * std::abs(v_over_n - v_over_t)) +
                                           (w(v_under_n - v_under_t) * std::abs(v_under_n - v_under_t)) +
                                           (w(d_from_n - d_from_t) * std::abs(d_from_n - d_from_t)) + (w(d_to_n - d_to_t) * std::abs(d_to_n - d_to_t)) +
                                           (w(x_from_n - x_from_t) * std::abs(x_from_n - x_from_t)) + (w(x_to_n - x_to_t) * std::abs(x_to_n - x_to_t));
                        if (cost_n < cost_b) {
                            csm.clear(r, over_c); csm.set(r, under_c);
                            vcnt.at(over_c) = static_cast<std::uint16_t>(v_over_n); vcnt.at(under_c)= static_cast<std::uint16_t>(v_under_n);
                            dcnt.at(d_from) = static_cast<std::uint16_t>(d_from_n); dcnt.at(d_to)   = static_cast<std::uint16_t>(d_to_n);
                            xcnt.at(x_from) = static_cast<std::uint16_t>(x_from_n); xcnt.at(x_to)   = static_cast<std::uint16_t>(x_to_n);
                            ++improved_moves; ++boosts;
                        }
                    }
                    if (improved_moves > 0) { ::crsce::o11y::O11y::instance().event("hyb_stall_boost_vsm", {{"moves", std::to_string(improved_moves)}}); }
                }
                // DSM/XSM boosts by reusing targeted blocks above with larger caps could be added similarly
                ::crsce::o11y::O11y::instance().event("hyb_stall_breaker", {{"prev_cost", std::to_string(prev_total_cost)}, {"cost", std::to_string(total_cost)}, {"boosts", std::to_string(boosts)}, {"pass", std::to_string(passes)}});
                stall_count = 0;
            }
        }
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
