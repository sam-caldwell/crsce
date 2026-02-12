/**
 * @file reseed_residuals_from_csm.cpp
 * @brief Implementation: recompute residual R/U arrays from CSM and target cross-sums.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */

#include "decompress/Block/detail/reseed_residuals_from_csm.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Utils/detail/index_calc_d.h"
#include "decompress/Utils/detail/index_calc_x.h"

namespace crsce::decompress::detail {

    void reseed_residuals_from_csm(const Csm &csm,
                                   ConstraintState &st,
                                   std::span<const std::uint16_t> lsm,
                                   std::span<const std::uint16_t> vsm,
                                   std::span<const std::uint16_t> dsm,
                                   std::span<const std::uint16_t> xsm) {
        constexpr std::size_t S = Csm::kS;

        std::array<std::uint16_t, S> ones_row{};
        std::array<std::uint16_t, S> ones_col{};
        std::array<std::uint16_t, S> ones_diag{};
        std::array<std::uint16_t, S> ones_x{};

        std::array<std::uint16_t, S> unk_row{};
        std::array<std::uint16_t, S> unk_col{};
        std::array<std::uint16_t, S> unk_diag{};
        std::array<std::uint16_t, S> unk_x{};

        for (std::size_t r = 0; r < S; ++r) {
            for (std::size_t c = 0; c < S; ++c) {
                const bool locked = csm.is_locked(r, c);
                const bool bit = csm.get(r, c);
                const auto d = detail::calc_d(r, c);
                const auto x = detail::calc_x(r, c);
                if (bit) {
                    ++ones_row.at(r);
                    ++ones_col.at(c);
                    ++ones_diag.at(d);
                    ++ones_x.at(x);
                }
                if (!locked) {
                    ++unk_row.at(r);
                    ++unk_col.at(c);
                    ++unk_diag.at(d);
                    ++unk_x.at(x);
                }
            }
        }

        auto clamp_nonneg = [](int v) -> std::uint16_t {
            return static_cast<std::uint16_t>(v > 0 ? v : 0);
        };

        for (std::size_t i = 0; i < S; ++i) {
            const int r_need = (i < lsm.size()) ? static_cast<int>(lsm[i]) : 0;
            const int c_need = (i < vsm.size()) ? static_cast<int>(vsm[i]) : 0;
            const int d_need = (i < dsm.size()) ? static_cast<int>(dsm[i]) : 0;
            const int x_need = (i < xsm.size()) ? static_cast<int>(xsm[i]) : 0;

            // Unknowns (degrees of freedom) from lock map
            const std::uint16_t U_r = unk_row.at(i);
            const std::uint16_t U_c = unk_col.at(i);
            const std::uint16_t U_d = unk_diag.at(i);
            const std::uint16_t U_x = unk_x.at(i);
            st.U_row.at(i) = U_r;
            st.U_col.at(i) = U_c;
            st.U_diag.at(i) = U_d;
            st.U_xdiag.at(i) = U_x;

            // Residual ones needed, clamped to available unknowns to preserve 0 ≤ R ≤ U
            const std::uint16_t Rr = clamp_nonneg(r_need - static_cast<int>(ones_row.at(i)));
            const std::uint16_t Rc = clamp_nonneg(c_need - static_cast<int>(ones_col.at(i)));
            const std::uint16_t Rd = clamp_nonneg(d_need - static_cast<int>(ones_diag.at(i)));
            const std::uint16_t Rx = clamp_nonneg(x_need - static_cast<int>(ones_x.at(i)));
            st.R_row.at(i) = (Rr > U_r) ? U_r : Rr;
            st.R_col.at(i) = (Rc > U_c) ? U_c : Rc;
            st.R_diag.at(i) = (Rd > U_d) ? U_d : Rd;
            st.R_xdiag.at(i) = (Rx > U_x) ? U_x : Rx;
        }
    }
}
