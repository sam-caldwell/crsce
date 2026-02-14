/**
 * @file execute_radditz_xsm_sift_and_validate.cpp
 * @brief XSM-focused 2x2 swap sift to reduce anti-diagonal deficits while preserving row/col.
 */
#include "decompress/Block/detail/execute_radditz_xsm_sift_and_validate.h"

#include <vector>
#include <cstddef>
#include <cstdint>
#include <span>

#include "decompress/Utils/detail/index_calc_d.h"
#include "decompress/Utils/detail/index_calc_x.h"
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/get_block_solve_snapshot.h"
#include <cstdlib>

namespace {
    bool xsm_accept_swap(const std::size_t xx_before, const std::size_t xx_after,
                         const int x_sat_lost) {
        // Accept only if XSM local cost does not increase; when locked,
        // do not accept swaps that would reduce currently satisfied X-diagonals.
        if (xx_after > xx_before) { return false; }
        int lock_x = 0;
        if (const auto snap = crsce::decompress::get_block_solve_snapshot(); snap.has_value()) {
            lock_x = snap->lock_xsm_sat;
        }
        if (lock_x && x_sat_lost > 0) { return false; }
        return true;
    }
}

namespace crsce::decompress::detail {
    bool execute_radditz_xsm_sift_and_validate(Csm &csm,
                                               ConstraintState &st,
                                               BlockSolveSnapshot &snap,
                                               const std::span<const std::uint16_t> xsm,
                                               const std::span<const std::uint8_t> lh) {
        (void)lh; // no LH validation during XSM sift
        using crsce::decompress::detail::calc_d;
        using crsce::decompress::detail::calc_x;
        constexpr std::size_t S = Csm::kS;
        // No LH validation during XSM sift

        // Ensure heartbeat shows we are in XSM-focused Radditz stage
        snap.phase = BlockSolveSnapshot::Phase::radditzSift;
        snap.radditz_kind = 3;
        std::vector<std::uint16_t> dcnt(S, 0);
        std::vector<std::uint16_t> xcnt(S, 0);
        for (std::size_t i = 0; i < S; ++i) {
            dcnt[i] = csm.count_dsm(i);
            xcnt[i] = csm.count_xsm(i);
        }

        auto xerr = [&](std::size_t x, std::uint16_t cnt){ const std::uint16_t tgt = (x<xsm.size())?xsm[x]:0; return (cnt>tgt)?(cnt-tgt):(tgt-cnt); };

        unsigned max_passes = 2U;
        if (const char *p = std::getenv("CRSCE_XSF_MAX_PASSES")) { // NOLINT(concurrency-mt-unsafe)
            const auto v = std::strtoll(p, nullptr, 10);
            if (v > 0) { max_passes = static_cast<unsigned>(v); }
        }
        for (unsigned pass=0; pass<max_passes; ++pass) {
            std::size_t accepts = 0;
            for (std::size_t s = 0; s < static_cast<std::size_t>(S*S) && accepts < 32; ++s) {
                const std::size_t r0 = (s * 71U) % S;
                const std::size_t r1 = (r0 + 1U + ((s * 101U) % (S - 1U))) % S;
                const std::size_t c0 = (s * 83U) % S;
                const std::size_t c1 = (c0 + 1U + ((s * 127U) % (S - 1U))) % S;
                if (r0==r1 || c0==c1) { continue; }
                const bool b00 = csm.get(r0,c0); const bool b11 = csm.get(r1,c1);
                const bool b01 = csm.get(r0,c1); const bool b10 = csm.get(r1,c0);
                const bool main_rect = (b00 && b11 && !b01 && !b10);
                const bool off_rect  = (!b00 && !b11 && b01 && b10);
                if (!main_rect && !off_rect) { continue; }
                if ((main_rect && (csm.is_locked(r0,c0) || csm.is_locked(r1,c1) || csm.is_locked(r0,c1) || csm.is_locked(r1,c0))) ||
                    (off_rect  && (csm.is_locked(r0,c1) || csm.is_locked(r1,c0) || csm.is_locked(r0,c0) || csm.is_locked(r1,c1)))) {
                    continue;
                }
                const std::size_t x00 = calc_x(r0, c0);
                const std::size_t x11 = calc_x(r1, c1);
                const std::size_t x01 = calc_x(r0, c1);
                const std::size_t x10 = calc_x(r1, c0);
                const std::size_t x00_n = xcnt[x00] + (main_rect ? -1 : +1);
                const std::size_t x11_n = xcnt[x11] + (main_rect ? -1 : +1);
                const std::size_t x01_n = xcnt[x01] + (main_rect ? +1 : -1);
                const std::size_t x10_n = xcnt[x10] + (main_rect ? +1 : -1);
                const std::size_t cost_x_before = xerr(x00, xcnt[x00]) + xerr(x11, xcnt[x11]) + xerr(x01, xcnt[x01]) + xerr(x10, xcnt[x10]);
                const std::size_t cost_x_after  = xerr(x00, x00_n) + xerr(x11, x11_n) + xerr(x01, x01_n) + xerr(x10, x10_n);
                if (cost_x_after > cost_x_before) { continue; }
                const bool x00_sat_b = (xcnt[x00] == xsm[x00]);
                const bool x11_sat_b = (xcnt[x11] == xsm[x11]);
                const bool x01_sat_b = (xcnt[x01] == xsm[x01]);
                const bool x10_sat_b = (xcnt[x10] == xsm[x10]);
                const bool x00_sat_a = (x00_n == xsm[x00]);
                const bool x11_sat_a = (x11_n == xsm[x11]);
                const bool x01_sat_a = (x01_n == xsm[x01]);
                const bool x10_sat_a = (x10_n == xsm[x10]);
                const int x_sat_lost = (x00_sat_b && !x00_sat_a) + (x11_sat_b && !x11_sat_a) + (x01_sat_b && !x01_sat_a) + (x10_sat_b && !x10_sat_a);
                if (!xsm_accept_swap(cost_x_before, cost_x_after, x_sat_lost)) {
                    continue;
                }
                if (main_rect) { csm.put(r0,c0,false); csm.put(r1,c1,false); csm.put(r0,c1,true); csm.put(r1,c0,true); }
                else { csm.put(r0,c1,false); csm.put(r1,c0,false); csm.put(r0,c0,true); csm.put(r1,c1,true); }
                xcnt[x00]=x00_n; xcnt[x11]=x11_n; xcnt[x01]=x01_n; xcnt[x10]=x10_n;
                const auto upd = [&](std::size_t x){
                    const std::uint16_t tgt = xsm[x];
                    const std::uint16_t cnt = xcnt[x];
                    const std::uint16_t U = st.U_xdiag.at(x);
                    const std::uint16_t need = (tgt > cnt) ? static_cast<std::uint16_t>(tgt - cnt) : 0U;
                    st.R_xdiag.at(x) = (need > U) ? U : need;
                };
                upd(x00); upd(x11); upd(x01); upd(x10);
                ++accepts;
                if (accepts >= 12) { break; }
            }
            if (accepts == 0) { break; }
        }
        bool ok = true; for (std::size_t i=0;i<S && i<xsm.size();++i) { if (xcnt[i] != xsm[i]) { ok=false; break; } }
        snap.U_xdiag.assign(st.U_xdiag.begin(), st.U_xdiag.end());
        return ok;
    }
}
