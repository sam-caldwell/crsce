/**
 * @file gobp_preserve_rowcol_swap.cpp
 * @brief Implementation of 2x2 rectangle swap micro-pass (preserve row/col; improve DSM/XSM/LH).
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */

#include "decompress/Block/detail/gobp_preserve_rowcol_swap.h"

#include <array>
#include <cstddef>
#include <cstdint> // NOLINT
#include <span>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Utils/detail/index_calc_d.h"
#include "decompress/Utils/detail/index_calc_x.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"

namespace crsce::decompress::detail {

    /**
     * @name gobp_preserve_rowcol_swap
     * @brief Bounded 2×2 rectangle swap pass that preserves row/col sums and
     *        accepts swaps only when DSM/XSM local cost strictly decreases
     *        (tie-break on LH improvement for the two affected rows).
     * @param csm Cross‑Sum Matrix to update in place.
     * @param st Residual constraint state; updates only diag/xdiag residuals for touched indices.
     * @param lh Span of LH payload bytes used for LH tie‑break verification.
     * @param dsm Target per‑diag sums (size ≥ S).
     * @param xsm Target per‑anti‑diag sums (size ≥ S).
     * @param sample_rects Maximum rectangles to evaluate in this pass.
     * @param accept_limit Maximum accepted swaps to apply in this pass.
     * @return std::size_t Number of swaps applied in this pass.
     */
    std::size_t gobp_preserve_rowcol_swap(Csm &csm,
                                          ConstraintState &st,
                                          std::span<const std::uint8_t> lh,
                                          std::span<const std::uint16_t> dsm,
                                          std::span<const std::uint16_t> xsm,
                                          const unsigned sample_rects,
                                          const unsigned accept_limit) {
        constexpr std::size_t S = Csm::kS;
        // Precompute diag/xdiag ones counts once for this pass
        std::array<std::uint16_t, S> dcnt{};
        std::array<std::uint16_t, S> xcnt{};
        for (std::size_t r = 0; r < S; ++r) {
            for (std::size_t c = 0; c < S; ++c) {
                if (csm.get(r, c)) {
                    const auto d = detail::calc_d(r, c);
                    const auto x = detail::calc_x(r, c);
                    ++dcnt.at(d);
                    ++xcnt.at(x);
                }
            }
        }

        std::size_t accepted = 0;
        const crsce::decompress::RowHashVerifier ver{};

        // Deterministic sampling using simple strides to avoid external RNG dependencies.
        // This pass is bounded; occasional modulus/divisions are acceptable here.
        for (unsigned s = 0; s < sample_rects && accepted < accept_limit; ++s) {
            const std::size_t r0 = (static_cast<std::size_t>(s) * 73U) % S;
            const std::size_t r1 = (r0 + 1U + ((static_cast<std::size_t>(s) * 97U) % (S - 1U))) % S;
            const std::size_t c0 = (static_cast<std::size_t>(s) * 89U) % S;
            const std::size_t c1 = (c0 + 1U + ((static_cast<std::size_t>(s) * 131U) % (S - 1U))) % S;
            if (r0 == r1 || c0 == c1) { continue; }

            const bool b00 = (csm.get(r0, c0) != 0);
            const bool b11 = (csm.get(r1, c1) != 0);
            const bool b01 = (csm.get(r0, c1) != 0);
            const bool b10 = (csm.get(r1, c0) != 0);

            // Two valid rectangle patterns: 1s on main diag or on off diag
            const bool main = (b00 && b11 && !b01 && !b10);
            const bool off  = (!b00 && !b11 && b01 && b10);
            if (!main && !off) { continue; }

            // Cells to flip: ones->zero at current ones, zeros->one at current zeros
            // no-op locals removed (dead stores)

            // Check locks for the two cells that would become zero and the two that would become one
            if (main) {
                if (csm.is_locked(r0, c0) || csm.is_locked(r1, c1) || csm.is_locked(r0, c1) || csm.is_locked(r1, c0)) {
                    continue;
                }
            } else { // off
                if (csm.is_locked(r0, c1) || csm.is_locked(r1, c0) || csm.is_locked(r0, c0) || csm.is_locked(r1, c1)) {
                    continue;
                }
            }

            // Gather diag/xdiag indices
            const std::size_t d00 = detail::calc_d(r0, c0);
            const std::size_t d11 = detail::calc_d(r1, c1);
            const std::size_t d01 = detail::calc_d(r0, c1);
            const std::size_t d10 = detail::calc_d(r1, c0);
            const std::size_t x00 = detail::calc_x(r0, c0);
            const std::size_t x11 = detail::calc_x(r1, c1);
            const std::size_t x01 = detail::calc_x(r0, c1);
            const std::size_t x10 = detail::calc_x(r1, c0);

            // Current local cost: abs diff for the four affected diags/xdiags
            const auto adb00 = (dcnt.at(d00) > dsm[d00]) ? static_cast<std::size_t>(dcnt.at(d00) - dsm[d00]) : static_cast<std::size_t>(dsm[d00] - dcnt.at(d00));
            const auto adb11 = (dcnt.at(d11) > dsm[d11]) ? static_cast<std::size_t>(dcnt.at(d11) - dsm[d11]) : static_cast<std::size_t>(dsm[d11] - dcnt.at(d11));
            const auto adb01 = (dcnt.at(d01) > dsm[d01]) ? static_cast<std::size_t>(dcnt.at(d01) - dsm[d01]) : static_cast<std::size_t>(dsm[d01] - dcnt.at(d01));
            const auto adb10 = (dcnt.at(d10) > dsm[d10]) ? static_cast<std::size_t>(dcnt.at(d10) - dsm[d10]) : static_cast<std::size_t>(dsm[d10] - dcnt.at(d10));
            const std::size_t cost_d_before = adb00 + adb11 + adb01 + adb10;
            const auto axb00 = (xcnt.at(x00) > xsm[x00]) ? static_cast<std::size_t>(xcnt.at(x00) - xsm[x00]) : static_cast<std::size_t>(xsm[x00] - xcnt.at(x00));
            const auto axb11 = (xcnt.at(x11) > xsm[x11]) ? static_cast<std::size_t>(xcnt.at(x11) - xsm[x11]) : static_cast<std::size_t>(xsm[x11] - xcnt.at(x11));
            const auto axb01 = (xcnt.at(x01) > xsm[x01]) ? static_cast<std::size_t>(xcnt.at(x01) - xsm[x01]) : static_cast<std::size_t>(xsm[x01] - xcnt.at(x01));
            const auto axb10 = (xcnt.at(x10) > xsm[x10]) ? static_cast<std::size_t>(xcnt.at(x10) - xsm[x10]) : static_cast<std::size_t>(xsm[x10] - xcnt.at(x10));
            const std::size_t cost_x_before = axb00 + axb11 + axb01 + axb10;

            // Simulate counts after swap
            std::uint16_t d00_n = dcnt.at(d00);
            std::uint16_t d11_n = dcnt.at(d11);
            std::uint16_t d01_n = dcnt.at(d01);
            std::uint16_t d10_n = dcnt.at(d10);
            std::uint16_t x00_n = xcnt.at(x00);
            std::uint16_t x11_n = xcnt.at(x11);
            std::uint16_t x01_n = xcnt.at(x01);
            std::uint16_t x10_n = xcnt.at(x10);
            if (main) {
                // move ones from (r0,c0),(r1,c1) -> (r0,c1),(r1,c0)
                --d00_n; --d11_n; ++d01_n; ++d10_n;
                --x00_n; --x11_n; ++x01_n; ++x10_n;
            } else { // off
                --d01_n; --d10_n; ++d00_n; ++d11_n;
                --x01_n; --x10_n; ++x00_n; ++x11_n;
            }
            const auto ada00 = (d00_n > dsm[d00]) ? static_cast<std::size_t>(d00_n - dsm[d00]) : static_cast<std::size_t>(dsm[d00] - d00_n);
            const auto ada11 = (d11_n > dsm[d11]) ? static_cast<std::size_t>(d11_n - dsm[d11]) : static_cast<std::size_t>(dsm[d11] - d11_n);
            const auto ada01 = (d01_n > dsm[d01]) ? static_cast<std::size_t>(d01_n - dsm[d01]) : static_cast<std::size_t>(dsm[d01] - d01_n);
            const auto ada10 = (d10_n > dsm[d10]) ? static_cast<std::size_t>(d10_n - dsm[d10]) : static_cast<std::size_t>(dsm[d10] - d10_n);
            const std::size_t cost_d_after = ada00 + ada11 + ada01 + ada10;
            const auto axa00 = (x00_n > xsm[x00]) ? static_cast<std::size_t>(x00_n - xsm[x00]) : static_cast<std::size_t>(xsm[x00] - x00_n);
            const auto axa11 = (x11_n > xsm[x11]) ? static_cast<std::size_t>(x11_n - xsm[x11]) : static_cast<std::size_t>(xsm[x11] - x11_n);
            const auto axa01 = (x01_n > xsm[x01]) ? static_cast<std::size_t>(x01_n - xsm[x01]) : static_cast<std::size_t>(xsm[x01] - x01_n);
            const auto axa10 = (x10_n > xsm[x10]) ? static_cast<std::size_t>(x10_n - xsm[x10]) : static_cast<std::size_t>(xsm[x10] - x10_n);
            const std::size_t cost_x_after = axa00 + axa11 + axa01 + axa10;

            const std::size_t dx_before = cost_d_before + cost_x_before;
            const std::size_t dx_after  = cost_d_after + cost_x_after;

            // LH tie-breaker: count how many of the two rows verify before/after
            int lh_before = 0;
            if (ver.verify_row(csm, lh, r0)) { ++lh_before; }
            if (ver.verify_row(csm, lh, r1)) { ++lh_before; }

            bool accept = false;
            if (dx_after < dx_before) {
                accept = true;
            } else if (dx_after == dx_before) {
                // Temporarily apply to check LH improvement
                const bool v00 = b00;
                const bool v11 = b11;
                const bool v01 = b01;
                const bool v10 = b10;
                if (main) {
                    csm.put(r0, c0, false); csm.put(r1, c1, false);
                    csm.put(r0, c1, true);  csm.put(r1, c0, true);
                } else {
                    csm.put(r0, c1, false); csm.put(r1, c0, false);
                    csm.put(r0, c0, true);  csm.put(r1, c1, true);
                }
                int lh_after = 0;
                if (ver.verify_row(csm, lh, r0)) { ++lh_after; }
                if (ver.verify_row(csm, lh, r1)) { ++lh_after; }
                // Revert for now
                csm.put(r0, c0, v00); csm.put(r1, c1, v11);
                csm.put(r0, c1, v01); csm.put(r1, c0, v10);
                if (lh_after > lh_before) {
                    accept = true;
                }
            }

            if (!accept) { continue; }

            // Apply swap for real
            if (main) {
                csm.put(r0, c0, false); csm.put(r1, c1, false);
                csm.put(r0, c1, true);  csm.put(r1, c0, true);
                dcnt.at(d00) = d00_n; dcnt.at(d11) = d11_n; dcnt.at(d01) = d01_n; dcnt.at(d10) = d10_n;
                xcnt.at(x00) = x00_n; xcnt.at(x11) = x11_n; xcnt.at(x01) = x01_n; xcnt.at(x10) = x10_n;
            } else {
                csm.put(r0, c1, false); csm.put(r1, c0, false);
                csm.put(r0, c0, true);  csm.put(r1, c1, true);
                dcnt.at(d00) = d00_n; dcnt.at(d11) = d11_n; dcnt.at(d01) = d01_n; dcnt.at(d10) = d10_n;
                xcnt.at(x00) = x00_n; xcnt.at(x11) = x11_n; xcnt.at(x01) = x01_n; xcnt.at(x10) = x10_n;
            }

            // Update residuals for only the touched diag/xdiag indices (clamp R ≤ U)
            {
                const std::uint16_t U_d00 = st.U_diag.at(d00);
                const std::uint16_t need_d00 = (dsm[d00] > dcnt.at(d00)) ? static_cast<std::uint16_t>(dsm[d00] - dcnt.at(d00)) : 0U;
                st.R_diag.at(d00) = (need_d00 > U_d00) ? U_d00 : need_d00;
                const std::uint16_t U_d11 = st.U_diag.at(d11);
                const std::uint16_t need_d11 = (dsm[d11] > dcnt.at(d11)) ? static_cast<std::uint16_t>(dsm[d11] - dcnt.at(d11)) : 0U;
                st.R_diag.at(d11) = (need_d11 > U_d11) ? U_d11 : need_d11;
                const std::uint16_t U_d01 = st.U_diag.at(d01);
                const std::uint16_t need_d01 = (dsm[d01] > dcnt.at(d01)) ? static_cast<std::uint16_t>(dsm[d01] - dcnt.at(d01)) : 0U;
                st.R_diag.at(d01) = (need_d01 > U_d01) ? U_d01 : need_d01;
                const std::uint16_t U_d10 = st.U_diag.at(d10);
                const std::uint16_t need_d10 = (dsm[d10] > dcnt.at(d10)) ? static_cast<std::uint16_t>(dsm[d10] - dcnt.at(d10)) : 0U;
                st.R_diag.at(d10) = (need_d10 > U_d10) ? U_d10 : need_d10;
                const std::uint16_t U_x00 = st.U_xdiag.at(x00);
                const std::uint16_t need_x00 = (xsm[x00] > xcnt.at(x00)) ? static_cast<std::uint16_t>(xsm[x00] - xcnt.at(x00)) : 0U;
                st.R_xdiag.at(x00) = (need_x00 > U_x00) ? U_x00 : need_x00;
                const std::uint16_t U_x11 = st.U_xdiag.at(x11);
                const std::uint16_t need_x11 = (xsm[x11] > xcnt.at(x11)) ? static_cast<std::uint16_t>(xsm[x11] - xcnt.at(x11)) : 0U;
                st.R_xdiag.at(x11) = (need_x11 > U_x11) ? U_x11 : need_x11;
                const std::uint16_t U_x01 = st.U_xdiag.at(x01);
                const std::uint16_t need_x01 = (xsm[x01] > xcnt.at(x01)) ? static_cast<std::uint16_t>(xsm[x01] - xcnt.at(x01)) : 0U;
                st.R_xdiag.at(x01) = (need_x01 > U_x01) ? U_x01 : need_x01;
                const std::uint16_t U_x10 = st.U_xdiag.at(x10);
                const std::uint16_t need_x10 = (xsm[x10] > xcnt.at(x10)) ? static_cast<std::uint16_t>(xsm[x10] - xcnt.at(x10)) : 0U;
                st.R_xdiag.at(x10) = (need_x10 > U_x10) ? U_x10 : need_x10;
            }

            ++accepted;
        }
        return accepted;
    }
}
